//! Dependency-free Rust bindings for CUI's native controls.
//!
//! Build the C library first; set `CUI_LIB_DIR` to its static archive directory.
//! Create one [`App`] on the process main thread, build widgets, show the window,
//! then call [`App::run`]. See `examples/hello.rs` and `examples/gallery.rs`.
//!
//! Handles borrow the application through checked weak references. Cloning a
//! handle does not clone its native widget or keep the application alive. Calls
//! after application destruction return [`Error::Closed`]. [`Icon`] has separate
//! reference-counted ownership; cloning retains the asset, dropping releases it.
//!
//! Models and strings are copied during setters; getters return owned values.
//! Setters are silent. Explicit actions may call a handler synchronously.
//! Callback closures are retained until app destruction, even after replacement.
//! Panics are caught at the FFI boundary and returned by `run`; `panic=abort`
//! still aborts. Quit inside callbacks and drop the application after `run`.
//!
//! All UI handles and assets are intentionally neither Send nor Sync:
//! ```compile_fail
//! fn require_send<T: Send>() {}
//! require_send::<cui::Widget>();
//! ```
//! ```compile_fail
//! fn require_sync<T: Sync>() {}
//! require_sync::<cui::App>();
//! ```
//! ```compile_fail
//! fn require_send<T: Send>() {}
//! require_send::<cui::Icon>();
//! ```
//!
//! [`sys`] exposes the complete unsafe C ABI, including the original constants.
//! Raw calls require the C lifetime, thread, buffer and callback contracts.
#[rustfmt::skip]
pub mod sys;
pub mod chat;
mod chat_native;
pub use chat_native::{
    chat_color, chat_presentation, ChatCommand, ChatKind, ChatPresentation, Event as ChatEvent,
    NativeChat as ChatComponent,
};
mod desktop;
mod draw;
pub use draw::*;
mod models;
mod widgets;
pub use desktop::*;
pub use models::*;
use std::{
    any::Any,
    cell::{Cell, RefCell},
    ffi::{c_char, c_void, CStr, CString},
    fmt,
    marker::PhantomData,
    panic::{catch_unwind, AssertUnwindSafe},
    ptr::NonNull,
    rc::{Rc, Weak},
};

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    Initialization,
    Closed,
    InvalidInput(&'static str),
    NativeFailure,
    CallbackPanic(String),
}
impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{self:?}")
    }
}
impl std::error::Error for Error {}
pub type Result<T> = std::result::Result<T, Error>;
fn string(value: &str) -> Result<CString> {
    CString::new(value).map_err(|_| Error::InvalidInput("interior NUL in text"))
}
fn copy_text(ptr: *const c_char) -> String {
    if ptr.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(ptr) }
            .to_string_lossy()
            .into_owned()
    }
}
fn strings(items: &[&str]) -> Result<(Vec<CString>, Vec<*const c_char>)> {
    let text = items
        .iter()
        .map(|s| string(s))
        .collect::<Result<Vec<_>>>()?;
    let pointers = text.iter().map(|s| s.as_ptr()).collect();
    Ok((text, pointers))
}
fn read_text(mut call: impl FnMut(*mut c_char, usize) -> usize) -> String {
    let size = call(std::ptr::null_mut(), 0);
    let mut buffer = vec![0u8; size + 1];
    let count = call(buffer.as_mut_ptr().cast(), buffer.len()).min(size);
    String::from_utf8_lossy(&buffer[..count]).into_owned()
}
fn accepted(value: i32) -> bool {
    value != 0
}

struct Runtime {
    ptr: NonNull<sys::cui_app>,
    callbacks: RefCell<Vec<Box<dyn Any>>>,
    panic: RefCell<Option<String>>,
    running: Cell<bool>,
}
impl Drop for Runtime {
    fn drop(&mut self) {
        unsafe { sys::cui_app_destroy(self.ptr.as_ptr()) };
    }
}
impl Runtime {
    fn keep<T: 'static>(&self, value: T) -> *mut c_void {
        let mut value = Box::new(value);
        let pointer = (&mut *value as *mut T).cast();
        self.callbacks.borrow_mut().push(value);
        pointer
    }
    fn invoke(&self, callback: impl FnOnce()) {
        if let Err(value) = catch_unwind(AssertUnwindSafe(callback)) {
            let message = value
                .downcast_ref::<&str>()
                .map(|s| s.to_string())
                .or_else(|| value.downcast_ref::<String>().cloned())
                .unwrap_or_else(|| "callback panicked".into());
            *self.panic.borrow_mut() = Some(message);
            unsafe { sys::cui_app_quit(self.ptr.as_ptr()) };
        }
    }
}
struct Handle<T> {
    ptr: NonNull<T>,
    owner: Weak<Runtime>,
}
impl<T> Clone for Handle<T> {
    fn clone(&self) -> Self {
        Self {
            ptr: self.ptr,
            owner: self.owner.clone(),
        }
    }
}
impl<T> Handle<T> {
    fn new(owner: &Rc<Runtime>, ptr: *mut T) -> Result<Self> {
        Ok(Self {
            ptr: NonNull::new(ptr).ok_or(Error::NativeFailure)?,
            owner: Rc::downgrade(owner),
        })
    }
    fn live(&self) -> Result<Rc<Runtime>> {
        self.owner.upgrade().ok_or(Error::Closed)
    }
    fn same<U>(&self, other: &Handle<U>) -> Result<()> {
        other.live()?;
        if Weak::ptr_eq(&self.owner, &other.owner) {
            Ok(())
        } else {
            Err(Error::InvalidInput("handles belong to different apps"))
        }
    }
}
// Manual Clone avoids requiring the opaque C types themselves to implement Clone.
/// A checked, clonable reference to an app-owned native widget.
/// Cloning preserves identity; dropping this handle does not destroy the widget.
#[derive(Clone)]
pub struct Widget {
    handle: Handle<sys::cui_widget>,
    columns: usize,
}
#[derive(Clone)]
pub struct Window {
    handle: Handle<sys::cui_window>,
}
#[derive(Clone)]
pub struct Timer {
    handle: Handle<sys::cui_timer>,
}
/// Owns the native application and retained callback closures.
/// Create on the main thread, quit inside callbacks, and drop after `run` returns.
pub struct App {
    runtime: Rc<Runtime>,
}
impl App {
    pub fn new() -> Result<Self> {
        #[cfg(target_os = "macos")]
        {
            unsafe extern "C" {
                fn pthread_main_np() -> i32;
            }
            if unsafe { pthread_main_np() } == 0 {
                return Err(Error::InvalidInput("create App on the main thread"));
            }
        }
        #[cfg(target_os = "linux")]
        {
            unsafe extern "C" {
                fn getpid() -> i32;
                fn gettid() -> i32;
            }
            if unsafe { getpid() != gettid() } {
                return Err(Error::InvalidInput("create App on the main thread"));
            }
        }
        let ptr = NonNull::new(unsafe { sys::cui_app_create() }).ok_or(Error::Initialization)?;
        Ok(Self {
            runtime: Rc::new(Runtime {
                ptr,
                callbacks: RefCell::new(Vec::new()),
                panic: RefCell::new(None),
                running: Cell::new(false),
            }),
        })
    }
    pub fn run(&self) -> Result<()> {
        if let Some(message) = self.runtime.panic.borrow_mut().take() {
            return Err(Error::CallbackPanic(message));
        }
        if self.runtime.running.replace(true) {
            return Err(Error::InvalidInput("event loop already running"));
        };
        unsafe { sys::cui_app_run(self.runtime.ptr.as_ptr()) };
        self.runtime.running.set(false);
        if let Some(message) = self.runtime.panic.borrow_mut().take() {
            Err(Error::CallbackPanic(message))
        } else {
            Ok(())
        }
    }
    pub fn quit(&self) {
        unsafe { sys::cui_app_quit(self.runtime.ptr.as_ptr()) }
    }
    pub fn error(&self) -> String {
        copy_text(unsafe { sys::cui_app_error(self.runtime.ptr.as_ptr()) })
    }
    /// Current light/dark appearance, resolving the system preference.
    pub fn resolved_theme(&self) -> sys::cui_theme {
        unsafe { sys::cui_app_resolved_theme(self.runtime.ptr.as_ptr()) }
    }
    pub fn theme(&self, theme: sys::cui_theme) {
        unsafe { sys::cui_app_set_theme(self.runtime.ptr.as_ptr(), theme) }
    }
    pub fn text_scale(&self, scale: f64) -> bool {
        accepted(unsafe { sys::cui_app_set_text_scale(self.runtime.ptr.as_ptr(), scale) })
    }
    pub fn window(&self, title: &str, width: i32, height: i32) -> Result<Window> {
        let title = string(title)?;
        Ok(Window {
            handle: Handle::new(&self.runtime, unsafe {
                sys::cui_window_create(self.runtime.ptr.as_ptr(), title.as_ptr(), width, height)
            })?,
        })
    }
    pub fn every(&self, milliseconds: u32, callback: impl FnMut() + 'static) -> Result<Timer> {
        let data = self.runtime.keep(TaskCallback {
            owner: Rc::downgrade(&self.runtime),
            callback: RefCell::new(Box::new(callback)),
        });
        Ok(Timer {
            handle: Handle::new(&self.runtime, unsafe {
                sys::cui_every(
                    self.runtime.ptr.as_ptr(),
                    milliseconds,
                    Some(task_callback),
                    data,
                )
            })?,
        })
    }
}
pub fn monotonic_time() -> f64 {
    unsafe { sys::cui_time() }
}
pub fn pattern_name(kind: sys::cui_pattern) -> String {
    copy_text(unsafe { sys::cui_pattern_name(kind) })
}
impl Window {
    pub fn root(&self) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe {
            sys::cui_window_root(self.handle.ptr.as_ptr())
        })
    }
    pub fn show(&self) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_window_show(self.handle.ptr.as_ptr()) };
        Ok(())
    }
    pub fn close(&self) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_window_close(self.handle.ptr.as_ptr()) };
        Ok(())
    }
    /// Configure native decorations, resizing and the custom frame's corner radius.
    pub fn frame(&self, decorated: bool, resizable: bool, radius: f64) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(unsafe {
            sys::cui_window_set_frame(
                self.handle.ptr.as_ptr(),
                decorated.into(),
                resizable.into(),
                radius,
            )
        } != 0)
    }
    pub fn set_size(&self, width: i32, height: i32) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_window_set_size(self.handle.ptr.as_ptr(), width, height) } != 0)
    }
    /// Best effort: Wayland compositors choose their own placement.
    pub fn set_position(&self, x: i32, y: i32) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_window_set_position(self.handle.ptr.as_ptr(), x, y) } != 0)
    }
    pub fn begin_move(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_window_begin_move(self.handle.ptr.as_ptr()) } != 0)
    }
    /// Current native content size in logical pixels.
    pub fn size(&self) -> Result<(i32, i32)> {
        let _rt = self.handle.live()?;
        let (mut width, mut height) = (0, 0);
        if unsafe { sys::cui_window_get_size(self.handle.ptr.as_ptr(), &mut width, &mut height) }
            == 0
        {
            return Err(Error::NativeFailure);
        }
        Ok((width, height))
    }
    /// Native corner drag: 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right.
    pub fn begin_resize(&self, corner: i32) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_window_begin_resize(self.handle.ptr.as_ptr(), corner) } != 0)
    }
    /// Attach this auxiliary window to a rectangle in another window's content.
    pub fn anchor(&self, parent: &Window, rect: [i32; 4]) -> Result<bool> {
        let _rt = self.handle.live()?;
        let _parent = parent.handle.live()?;
        Ok(unsafe {
            sys::cui_window_set_anchor(
                self.handle.ptr.as_ptr(),
                parent.handle.ptr.as_ptr(),
                rect[0],
                rect[1],
                rect[2],
                rect[3],
            )
        } != 0)
    }
    pub fn popup_at(&self, anchor: &Widget, rect: [f64; 4]) -> Result<bool> {
        let _rt = self.handle.live()?; self.handle.same(&anchor.handle)?;
        Ok(unsafe { sys::cui_window_popup_at(self.handle.ptr.as_ptr(),anchor.handle.ptr.as_ptr(),rect[0],rect[1],rect[2],rect[3]) } != 0)
    }
    pub fn popup_region(&self, canvas: &Widget, region: u32) -> Result<bool> {
        let _rt = self.handle.live()?; self.handle.same(&canvas.handle)?;
        Ok(unsafe { sys::cui_window_popup_region(self.handle.ptr.as_ptr(),canvas.handle.ptr.as_ptr(),region) } != 0)
    }
    pub fn is_visible(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_window_is_visible(self.handle.ptr.as_ptr()) } != 0)
    }
    pub fn scrollable(&self, value: bool) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_window_set_scrollable(self.handle.ptr.as_ptr(), value.into()) };
        Ok(())
    }
    pub fn scale(&self) -> Result<f64> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_window_scale(self.handle.ptr.as_ptr()) })
    }
    pub fn clipboard_text(&self, text: &str) -> Result<()> {
        let _rt = self.handle.live()?;
        let text = string(text)?;
        unsafe { sys::cui_clipboard_set_text(self.handle.ptr.as_ptr(), text.as_ptr()) };
        Ok(())
    }
}
impl Timer {
    pub fn stop(&self) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_timer_stop(self.handle.ptr.as_ptr()) };
        Ok(())
    }
    pub fn start(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_timer_start(self.handle.ptr.as_ptr())
        }))
    }
}
struct ActionCallback {
    widget: Widget,
    callback: RefCell<Box<dyn FnMut(Widget)>>,
}
struct TaskCallback {
    owner: Weak<Runtime>,
    callback: RefCell<Box<dyn FnMut()>>,
}
struct KeyCallback {
    widget: Widget,
    callback: RefCell<Box<dyn FnMut(i32, u32) -> bool>>,
}
unsafe extern "C" fn action_callback(_: *mut sys::cui_widget, data: *mut c_void) {
    let state = unsafe { &*(data as *const ActionCallback) };
    if let Ok(rt) = state.widget.handle.live() {
        rt.invoke(|| (state.callback.borrow_mut())(state.widget.clone()));
    }
}
unsafe extern "C" fn task_callback(data: *mut c_void) {
    let state = unsafe { &*(data as *const TaskCallback) };
    if let Some(rt) = state.owner.upgrade() {
        rt.invoke(|| (state.callback.borrow_mut())());
    }
}
unsafe extern "C" fn key_callback(
    _: *mut sys::cui_widget,
    key: i32,
    mods: u32,
    data: *mut c_void,
) -> i32 {
    let state = unsafe { &*(data as *const KeyCallback) };
    let mut result = false;
    if let Ok(rt) = state.widget.handle.live() {
        rt.invoke(|| result = (state.callback.borrow_mut())(key, mods));
    }
    result.into()
}
impl Widget {
    fn from_native(rt: &Rc<Runtime>, ptr: *mut sys::cui_widget) -> Result<Self> {
        Ok(Self {
            handle: Handle::new(rt, ptr)?,
            columns: 0,
        })
    }
    /// Install a handler. Replaced handlers remain allocated until app teardown.
    /// Explicit action calls may invoke this closure synchronously.
    pub fn on_action(&self, callback: impl FnMut(Widget) + 'static) -> Result<()> {
        let rt = self.handle.live()?;
        let data = rt.keep(ActionCallback {
            widget: self.clone(),
            callback: RefCell::new(Box::new(callback)),
        });
        unsafe { sys::cui_on_action(self.handle.ptr.as_ptr(), Some(action_callback), data) };
        Ok(())
    }
    pub fn clear_action(&self) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_on_action(self.handle.ptr.as_ptr(), None, std::ptr::null_mut()) };
        Ok(())
    }
    pub fn on_key(&self, callback: impl FnMut(i32, u32) -> bool + 'static) -> Result<bool> {
        let rt = self.handle.live()?;
        let data = rt.keep(KeyCallback {
            widget: self.clone(),
            callback: RefCell::new(Box::new(callback)),
        });
        Ok(accepted(unsafe {
            sys::cui_on_key(self.handle.ptr.as_ptr(), Some(key_callback), data)
        }))
    }
    pub fn clear_key(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_on_key(self.handle.ptr.as_ptr(), None, std::ptr::null_mut())
        }))
    }
    pub fn quit(&self) -> Result<()> {
        let rt = self.handle.live()?;
        unsafe { sys::cui_app_quit(rt.ptr.as_ptr()) };
        Ok(())
    }
}

/// An immutable, independently owned native vector or raster asset.
/// Clone retains a reference; Drop releases it. Widgets retain assigned assets.
pub struct Icon {
    ptr: NonNull<sys::cui_icon_asset>,
    _thread: PhantomData<Rc<()>>,
}
impl Clone for Icon {
    fn clone(&self) -> Self {
        Self::wrap(unsafe { sys::cui_icon_retain(self.ptr.as_ptr()) }).expect("valid asset")
    }
}
impl Drop for Icon {
    fn drop(&mut self) {
        unsafe { sys::cui_icon_release(self.ptr.as_ptr()) }
    }
}
impl Icon {
    fn wrap(ptr: *mut sys::cui_icon_asset) -> Result<Self> {
        Ok(Self {
            ptr: NonNull::new(ptr).ok_or(Error::NativeFailure)?,
            _thread: PhantomData,
        })
    }
    pub fn symbol(symbol: sys::cui_symbol) -> Result<Self> {
        Self::wrap(unsafe { sys::cui_icon_symbol(symbol) })
    }
    pub fn load(path: &str) -> Result<Self> {
        let path = string(path)?;
        Self::wrap(unsafe { sys::cui_icon_load(path.as_ptr()) })
    }
    pub fn image(path: &str) -> Result<Self> {
        let path = string(path)?;
        Self::wrap(unsafe { sys::cui_icon_load_image(path.as_ptr()) })
    }
    pub fn decode(bytes: &[u8]) -> Result<Self> {
        Self::wrap(unsafe { sys::cui_icon_decode(bytes.as_ptr().cast(), bytes.len()) })
    }
    pub fn vector(width: f32, height: f32, commands: &[sys::cui_icon_command]) -> Result<Self> {
        Self::wrap(unsafe {
            sys::cui_icon_vector(width, height, commands.as_ptr(), commands.len())
        })
    }
    pub fn rgba(bytes: &[u8], width: i32, height: i32) -> Result<Self> {
        check_pixels(bytes, width, height)?;
        Self::wrap(unsafe { sys::cui_icon_rgba(bytes.as_ptr(), width, height) })
    }
}
fn check_pixels(bytes: &[u8], width: i32, height: i32) -> Result<()> {
    if !(1..=4096).contains(&width)
        || !(1..=4096).contains(&height)
        || bytes.len() != width as usize * height as usize * 4
    {
        return Err(Error::InvalidInput("RGBA dimensions/length"));
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn text_validation_preserves_utf8_and_rejects_nul() {
        assert_eq!(
            string("世界 — Grüße").unwrap().to_str().unwrap(),
            "世界 — Grüße"
        );
        assert!(matches!(string("a\0b"), Err(Error::InvalidInput(_))));
    }
    #[test]
    fn icon_input_validation_and_owned_clones() {
        assert!(Icon::rgba(&[0; 3], 1, 1).is_err());
        assert!(Icon::rgba(&[], -1, 2).is_err());
        assert!(Icon::decode(b"broken").is_err());
        let pixels = vec![255; 16];
        let first = Icon::rgba(&pixels, 2, 2).unwrap();
        let second = first.clone();
        drop(pixels);
        drop(first);
        drop(second);
    }
    #[cfg(any(target_os = "linux", target_os = "macos"))]
    #[test]
    fn initialization_rejects_worker_thread() {
        assert!(
            std::thread::spawn(|| matches!(App::new(), Err(Error::InvalidInput(_))))
                .join()
                .unwrap()
        );
    }
}
