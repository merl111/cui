use super::*;
#[derive(Clone)]
pub struct Command {
    handle: Handle<sys::cui_command>,
}
#[derive(Clone)]
pub struct Menu {
    handle: Handle<sys::cui_menu>,
}
#[derive(Clone)]
pub struct Dialog {
    handle: Handle<sys::cui_dialog>,
}
#[derive(Clone, Debug)]
pub struct FileFilter<'a> {
    pub name: &'a str,
    pub extensions: &'a [&'a str],
}
#[derive(Clone, Debug, Default)]
pub struct FileOptions<'a> {
    pub initial_path: &'a str,
    pub filters: Vec<FileFilter<'a>>,
    pub initial_filter: usize,
    pub multiple: bool,
}
#[derive(Clone, Debug)]
pub struct Font {
    pub family: String,
    pub points: f64,
    pub weight: i32,
    pub italic: bool,
}
impl Font {
    fn native(&self) -> Result<sys::cui_font_value> {
        let family = string(&self.family)?;
        if family.as_bytes().len() > 128 {
            return Err(Error::InvalidInput("font family too long"));
        }
        let mut value = sys::cui_font_value {
            points: self.points,
            weight: self.weight,
            italic: self.italic.into(),
            ..Default::default()
        };
        for (i, b) in family.as_bytes().iter().enumerate() {
            value.family[i] = *b as c_char;
        }
        Ok(value)
    }
    fn owned(value: sys::cui_font_value) -> Self {
        Self {
            family: copy_text(value.family.as_ptr()),
            points: value.points,
            weight: value.weight,
            italic: value.italic != 0,
        }
    }
}
type DialogFn = dyn FnMut(Dialog, sys::cui_dialog_result, String);
struct DialogCallback {
    owner: Weak<Runtime>,
    callback: RefCell<Box<DialogFn>>,
}
unsafe extern "C" fn dialog_callback(
    dialog: *mut sys::cui_dialog,
    result: sys::cui_dialog_result,
    path: *const c_char,
    data: *mut c_void,
) {
    let state = unsafe { &*(data as *const DialogCallback) };
    if let Some(rt) = state.owner.upgrade() {
        if let Ok(handle) = Handle::new(&rt, dialog) {
            rt.invoke(|| (state.callback.borrow_mut())(Dialog { handle }, result, copy_text(path)));
        }
    }
}
fn dialog_data(
    rt: &Rc<Runtime>,
    callback: impl FnMut(Dialog, sys::cui_dialog_result, String) + 'static,
) -> *mut c_void {
    rt.keep(DialogCallback {
        owner: Rc::downgrade(rt),
        callback: RefCell::new(Box::new(callback)),
    })
}
impl App {
    pub fn command(
        &self,
        label: &str,
        key: u32,
        modifiers: u32,
        callback: impl FnMut() + 'static,
    ) -> Result<Command> {
        let label = string(label)?;
        let data = self.runtime.keep(TaskCallback {
            owner: Rc::downgrade(&self.runtime),
            callback: RefCell::new(Box::new(callback)),
        });
        Ok(Command {
            handle: Handle::new(&self.runtime, unsafe {
                sys::cui_command_create(
                    self.runtime.ptr.as_ptr(),
                    label.as_ptr(),
                    key,
                    modifiers,
                    Some(task_callback),
                    data,
                )
            })?,
        })
    }
    pub fn menu(&self) -> Result<Menu> {
        Ok(Menu {
            handle: Handle::new(&self.runtime, unsafe {
                sys::cui_menu_create(self.runtime.ptr.as_ptr())
            })?,
        })
    }
}
impl Command {
    pub fn set_enabled(&self, enabled: bool) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_command_set_enabled(self.handle.ptr.as_ptr(), enabled.into()) };
        Ok(())
    }
    pub fn set_checked(&self, checked: bool) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_command_set_checked(self.handle.ptr.as_ptr(), checked.into()) };
        Ok(())
    }
    pub fn invoke(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_command_invoke(self.handle.ptr.as_ptr())
        }))
    }
}
impl Menu {
    pub fn add(&self, command: &Command) -> Result<bool> {
        let _rt = self.handle.live()?;
        self.handle.same(&command.handle)?;
        Ok(accepted(unsafe {
            sys::cui_menu_add(self.handle.ptr.as_ptr(), command.handle.ptr.as_ptr())
        }))
    }
    pub fn submenu(&self, label: &str, menu: &Menu) -> Result<bool> {
        let _rt = self.handle.live()?;
        self.handle.same(&menu.handle)?;
        let label = string(label)?;
        Ok(accepted(unsafe {
            sys::cui_menu_add_submenu(
                self.handle.ptr.as_ptr(),
                label.as_ptr(),
                menu.handle.ptr.as_ptr(),
            )
        }))
    }
    pub fn separator(&self) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_menu_add_separator(self.handle.ptr.as_ptr())
        }))
    }
    pub fn popup(&self, anchor: &Widget) -> Result<()> {
        let _rt = self.handle.live()?;
        self.handle.same(&anchor.handle)?;
        unsafe { sys::cui_menu_popup(self.handle.ptr.as_ptr(), anchor.handle.ptr.as_ptr()) };
        Ok(())
    }
    /// Open a native menu at a widget-local rectangle in logical units.
    pub fn popup_at(&self, anchor: &Widget, x: f64, y: f64, width: f64, height: f64) -> Result<bool> {
        let _rt = self.handle.live()?;
        self.handle.same(&anchor.handle)?;
        Ok(accepted(unsafe { sys::cui_menu_popup_at(self.handle.ptr.as_ptr(), anchor.handle.ptr.as_ptr(), x, y, width, height) }))
    }
    /// Anchor to a current enabled canvas hit region; stale regions return false.
    pub fn popup_region(&self, canvas: &Widget, region: u32) -> Result<bool> {
        let _rt = self.handle.live()?;
        self.handle.same(&canvas.handle)?;
        Ok(accepted(unsafe { sys::cui_menu_popup_region(self.handle.ptr.as_ptr(), canvas.handle.ptr.as_ptr(), region) }))
    }
}
impl Window {
    pub fn set_menu(&self, menu: Option<&Menu>) -> Result<()> {
        let _rt = self.handle.live()?;
        if let Some(menu) = menu {
            self.handle.same(&menu.handle)?
        };
        unsafe {
            sys::cui_window_set_menu(
                self.handle.ptr.as_ptr(),
                menu.map_or(std::ptr::null_mut(), |m| m.handle.ptr.as_ptr()),
            )
        };
        Ok(())
    }
    pub fn file_dialog(
        &self,
        kind: sys::cui_dialog_kind,
        title: &str,
        initial_path: &str,
        callback: impl FnMut(Dialog, sys::cui_dialog_result, String) + 'static,
    ) -> Result<Dialog> {
        let rt = self.handle.live()?;
        let title = string(title)?;
        let initial = string(initial_path)?;
        let data = dialog_data(&rt, callback);
        Ok(Dialog {
            handle: Handle::new(&rt, unsafe {
                sys::cui_file_dialog(
                    self.handle.ptr.as_ptr(),
                    kind,
                    title.as_ptr(),
                    initial.as_ptr(),
                    Some(dialog_callback),
                    data,
                )
            })?,
        })
    }
    pub fn file_dialog_with_options(
        &self,
        kind: sys::cui_dialog_kind,
        title: &str,
        options: &FileOptions<'_>,
        callback: impl FnMut(Dialog, sys::cui_dialog_result, String) + 'static,
    ) -> Result<Dialog> {
        let rt = self.handle.live()?;
        let title = string(title)?;
        let initial = string(options.initial_path)?;
        let names = options
            .filters
            .iter()
            .map(|f| string(f.name))
            .collect::<Result<Vec<_>>>()?;
        let extensions = options
            .filters
            .iter()
            .map(|f| strings(f.extensions))
            .collect::<Result<Vec<_>>>()?;
        let filters: Vec<_> = names
            .iter()
            .zip(&extensions)
            .map(|(name, (_, p))| sys::cui_file_filter {
                name: name.as_ptr(),
                extensions: p.as_ptr(),
                extension_count: p.len(),
            })
            .collect();
        let value = sys::cui_file_options {
            initial_path: initial.as_ptr(),
            filters: filters.as_ptr(),
            filter_count: filters.len(),
            initial_filter: options.initial_filter,
            multiple: options.multiple.into(),
        };
        let data = dialog_data(&rt, callback);
        Ok(Dialog {
            handle: Handle::new(&rt, unsafe {
                sys::cui_file_dialog_ex(
                    self.handle.ptr.as_ptr(),
                    kind,
                    title.as_ptr(),
                    &value,
                    Some(dialog_callback),
                    data,
                )
            })?,
        })
    }
    pub fn alert(
        &self,
        title: &str,
        message: &str,
        accept: &str,
        callback: impl FnMut(Dialog, sys::cui_dialog_result, String) + 'static,
    ) -> Result<Dialog> {
        let rt = self.handle.live()?;
        let title = string(title)?;
        let message = string(message)?;
        let accept = string(accept)?;
        let data = dialog_data(&rt, callback);
        Ok(Dialog {
            handle: Handle::new(&rt, unsafe {
                sys::cui_alert(
                    self.handle.ptr.as_ptr(),
                    title.as_ptr(),
                    message.as_ptr(),
                    accept.as_ptr(),
                    Some(dialog_callback),
                    data,
                )
            })?,
        })
    }
    pub fn color_dialog(
        &self,
        title: &str,
        rgb: u32,
        callback: impl FnMut(Dialog, sys::cui_dialog_result, String) + 'static,
    ) -> Result<Dialog> {
        let rt = self.handle.live()?;
        let title = string(title)?;
        let data = dialog_data(&rt, callback);
        Ok(Dialog {
            handle: Handle::new(&rt, unsafe {
                sys::cui_color_dialog(
                    self.handle.ptr.as_ptr(),
                    title.as_ptr(),
                    rgb,
                    Some(dialog_callback),
                    data,
                )
            })?,
        })
    }
    pub fn font_dialog(
        &self,
        title: &str,
        font: &Font,
        callback: impl FnMut(Dialog, sys::cui_dialog_result, String) + 'static,
    ) -> Result<Dialog> {
        let rt = self.handle.live()?;
        let title = string(title)?;
        let font = font.native()?;
        let data = dialog_data(&rt, callback);
        Ok(Dialog {
            handle: Handle::new(&rt, unsafe {
                sys::cui_font_dialog(
                    self.handle.ptr.as_ptr(),
                    title.as_ptr(),
                    &font,
                    Some(dialog_callback),
                    data,
                )
            })?,
        })
    }
}
impl Dialog {
    pub fn cancel(&self) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_dialog_cancel(self.handle.ptr.as_ptr()) };
        Ok(())
    }
    pub fn path_count(&self) -> Result<usize> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_dialog_path_count(self.handle.ptr.as_ptr()) })
    }
    pub fn path(&self, index: usize) -> Result<Option<String>> {
        let _rt = self.handle.live()?;
        let ptr = unsafe { sys::cui_dialog_path(self.handle.ptr.as_ptr(), index) };
        Ok(if ptr.is_null() {
            None
        } else {
            Some(copy_text(ptr))
        })
    }
    pub fn paths(&self) -> Result<Vec<String>> {
        (0..self.path_count()?)
            .map(|i| self.path(i).map(|p| p.unwrap_or_default()))
            .collect()
    }
    pub fn filter(&self) -> Result<Option<usize>> {
        let _rt = self.handle.live()?;
        let mut index = 0;
        Ok(
            if unsafe { sys::cui_dialog_filter(self.handle.ptr.as_ptr(), &mut index) } != 0 {
                Some(index)
            } else {
                None
            },
        )
    }
    pub fn color(&self) -> Result<Option<u32>> {
        let _rt = self.handle.live()?;
        let mut color = 0;
        Ok(
            if unsafe { sys::cui_dialog_color(self.handle.ptr.as_ptr(), &mut color) } != 0 {
                Some(color)
            } else {
                None
            },
        )
    }
    pub fn font(&self) -> Result<Option<Font>> {
        let _rt = self.handle.live()?;
        let mut font = sys::cui_font_value::default();
        Ok(
            if unsafe { sys::cui_dialog_font(self.handle.ptr.as_ptr(), &mut font) } != 0 {
                Some(Font::owned(font))
            } else {
                None
            },
        )
    }
}
impl Widget {
    pub fn font_apply(&self, font: &Font) -> Result<bool> {
        let _rt = self.handle.live()?;
        let font = font.native()?;
        Ok(accepted(unsafe {
            sys::cui_font_apply(self.handle.ptr.as_ptr(), &font)
        }))
    }
    pub fn toolbar(&self, commands: &[Command]) -> Result<Self> {
        let rt = self.handle.live()?;
        for command in commands {
            self.handle.same(&command.handle)?
        }
        let pointers: Vec<_> = commands.iter().map(|c| c.handle.ptr.as_ptr()).collect();
        Self::from_native(&rt, unsafe {
            sys::cui_toolbar(self.handle.ptr.as_ptr(), pointers.as_ptr(), pointers.len())
        })
    }
}
