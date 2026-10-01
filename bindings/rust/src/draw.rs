//! Transactional CPU drawing with native font/icon rasterization.
use super::*;
pub fn draw_capabilities() -> u32 {
    unsafe { sys::cui_draw_capabilities() }
}
/// Measure native text in logical pixels on the UI thread after App creation.
pub fn measure_text(text: &str, family: &str, size: f64, weight: i32) -> Result<(f64, f64)> {
    let text = string(text)?;
    let family = string(family)?;
    let (mut width, mut height) = (0., 0.);
    if unsafe {
        sys::cui_text_measure(
            text.as_ptr(),
            family.as_ptr(),
            size,
            weight,
            &mut width,
            &mut height,
        )
    } == 0
    {
        return Err(Error::NativeFailure);
    }
    Ok((width, height))
}
pub struct Surface {
    ptr: NonNull<sys::cui_surface>,
    _thread: PhantomData<Rc<()>>,
}
impl Surface {
    pub fn new(width: i32, height: i32, scale: f64) -> Result<Self> {
        Ok(Self {
            ptr: NonNull::new(unsafe { sys::cui_surface_create(width, height, scale) })
                .ok_or(Error::NativeFailure)?,
            _thread: PhantomData,
        })
    }
    pub fn render(&self, scene: &Scene) -> Result<()> {
        if unsafe {
            sys::cui_surface_render(
                self.ptr.as_ptr(),
                scene.commands.as_ptr(),
                scene.commands.len(),
            )
        } != 0
        {
            Ok(())
        } else {
            Err(Error::NativeFailure)
        }
    }
    /// Replay a complete scene beginning with CLEAR inside logical damage.
    /// Include old/new content bounds and affected shadows/backdrop materials.
    pub fn render_region(&self, scene: &Scene, damage: [f64; 4]) -> Result<()> {
        if unsafe {
            sys::cui_surface_render_region(
                self.ptr.as_ptr(),
                scene.commands.as_ptr(),
                scene.commands.len(),
                damage[0],
                damage[1],
                damage[2],
                damage[3],
            )
        } != 0
        {
            Ok(())
        } else {
            Err(Error::NativeFailure)
        }
    }
    pub fn rgba(&self) -> (Vec<u8>, i32, i32) {
        let (mut w, mut h) = (0, 0);
        let n = unsafe {
            sys::cui_surface_read(self.ptr.as_ptr(), std::ptr::null_mut(), 0, &mut w, &mut h)
        };
        let mut bytes = vec![0; n];
        unsafe { sys::cui_surface_read(self.ptr.as_ptr(), bytes.as_mut_ptr(), n, &mut w, &mut h) };
        (bytes, w, h)
    }
}
impl Clone for Surface {
    fn clone(&self) -> Self {
        Self {
            ptr: NonNull::new(unsafe { sys::cui_surface_retain(self.ptr.as_ptr()) })
                .expect("surface reference"),
            _thread: PhantomData,
        }
    }
}
impl Drop for Surface {
    fn drop(&mut self) {
        unsafe { sys::cui_surface_release(self.ptr.as_ptr()) }
    }
}
/// Owns all text and retained icons until render completes. Colors: 0xRRGGBBAA.
#[derive(Default)]
pub struct Scene {
    commands: Vec<sys::cui_draw_command>,
    text: Vec<CString>,
    icons: Vec<Icon>,
}
impl Scene {
    pub fn new() -> Self {
        Self::default()
    }
    fn push(&mut self, op: i32, p: &[f32], color: u32, color2: u32) {
        let mut c = sys::cui_draw_command {
            op,
            color,
            color2,
            ..Default::default()
        };
        c.p[..p.len()].copy_from_slice(p);
        self.commands.push(c);
    }
    pub fn clear(&mut self, color: u32) {
        self.push(sys::CUI_DRAW_CLEAR, &[], color, 0)
    }
    pub fn save(&mut self) {
        self.push(sys::CUI_DRAW_SAVE, &[], 0, 0)
    }
    pub fn restore(&mut self) {
        self.push(sys::CUI_DRAW_RESTORE, &[], 0, 0)
    }
    pub fn translate(&mut self, x: f32, y: f32) {
        self.push(sys::CUI_DRAW_TRANSLATE, &[x, y], 0, 0)
    }
    pub fn scale(&mut self, x: f32, y: f32) {
        self.push(sys::CUI_DRAW_SCALE, &[x, y], 0, 0)
    }
    pub fn clip(&mut self, r: [f32; 4], radius: f32) {
        self.push(sys::CUI_DRAW_CLIP, &[r[0], r[1], r[2], r[3], radius], 0, 0)
    }
    pub fn layer(&mut self, opacity: f32) {
        self.push(sys::CUI_DRAW_LAYER, &[opacity], 0, 0)
    }
    pub fn end_layer(&mut self) {
        self.push(sys::CUI_DRAW_END_LAYER, &[], 0, 0)
    }
    pub fn rect(&mut self, r: [f32; 4], radius: f32, color: u32) {
        self.push(
            sys::CUI_DRAW_RECT,
            &[r[0], r[1], r[2], r[3], radius],
            color,
            0,
        )
    }
    pub fn ellipse(&mut self, r: [f32; 4], color: u32) {
        self.push(sys::CUI_DRAW_ELLIPSE, &r, color, 0)
    }
    pub fn line(&mut self, from: [f32; 2], to: [f32; 2], width: f32, color: u32) {
        self.push(
            sys::CUI_DRAW_LINE,
            &[from[0], from[1], to[0], to[1], width],
            color,
            0,
        )
    }
    pub fn gradient(&mut self, r: [f32; 4], radius: f32, top: u32, bottom: u32) {
        self.push(
            sys::CUI_DRAW_GRADIENT,
            &[r[0], r[1], r[2], r[3], radius],
            top,
            bottom,
        )
    }
    pub fn shadow(&mut self, r: [f32; 4], radius: f32, blur: f32, color: u32) {
        self.push(
            sys::CUI_DRAW_SHADOW,
            &[r[0], r[1], r[2], r[3], radius, blur],
            color,
            0,
        )
    }
    pub fn material(&mut self, r: [f32; 4], radius: f32, blur: f32, tint: u32) {
        self.push(
            sys::CUI_DRAW_MATERIAL,
            &[r[0], r[1], r[2], r[3], radius, blur],
            tint,
            0,
        )
    }
    /// Align visible glyphs inside a box; positive height vertically centers them.
    #[allow(clippy::too_many_arguments)]
    pub fn text_box(
        &mut self,
        rect: [f32; 4],
        size: f32,
        weight: i32,
        color: u32,
        text: &str,
        align: i32,
    ) -> Result<()> {
        self.text([rect[0], rect[1]], rect[2], size, weight, color, text)?;
        let command = self.commands.last_mut().unwrap();
        command.p[5] = align as f32;
        command.p[6] = rect[3];
        Ok(())
    }
    pub fn text(
        &mut self,
        at: [f32; 2],
        width: f32,
        size: f32,
        weight: i32,
        color: u32,
        text: &str,
    ) -> Result<()> {
        self.text_font(at, width, size, weight, color, text, None)
    }
    #[allow(clippy::too_many_arguments)]
    pub fn text_font(
        &mut self,
        at: [f32; 2],
        width: f32,
        size: f32,
        weight: i32,
        color: u32,
        text: &str,
        font: Option<&str>,
    ) -> Result<()> {
        let text = string(text)?;
        let family = font.map(string).transpose()?;
        self.push(
            sys::CUI_DRAW_TEXT,
            &[at[0], at[1], width, size, weight as f32],
            color,
            0,
        );
        let c = self.commands.last_mut().unwrap();
        c.text = text.as_ptr();
        self.text.push(text);
        if let Some(family) = family {
            c.font = family.as_ptr();
            self.text.push(family);
        }
        Ok(())
    }
    pub fn icon(&mut self, icon: &Icon, r: [f32; 4], color: u32) {
        self.push(sys::CUI_DRAW_ICON, &r, color, 0);
        self.commands.last_mut().unwrap().icon = icon.ptr.as_ptr();
        self.icons.push(icon.clone());
    }
}
#[derive(Clone, Debug)]
pub struct CanvasRegion {
    pub id: u32,
    pub rect: [f32; 4],
    pub label: String,
    pub enabled: bool,
}
pub type CanvasEvent = sys::cui_canvas_event;
struct CanvasCallback {
    widget: Widget,
    callback: RefCell<Box<dyn FnMut(CanvasEvent)>>,
}
unsafe extern "C" fn canvas_callback(
    _: *mut sys::cui_widget,
    event: *const sys::cui_canvas_event,
    data: *mut c_void,
) {
    let state = unsafe { &*data.cast::<CanvasCallback>() };
    if let Ok(rt) = state.widget.handle.live() {
        let event = unsafe { *event };
        rt.invoke(|| (state.callback.borrow_mut())(event));
    }
}
impl Widget {
    pub fn canvas_activate_region(&self, id: u32) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_canvas_activate_region(self.handle.ptr.as_ptr(), id)
        }))
    }
    pub fn canvas(&self) -> Result<Widget> {
        let rt = self.handle.live()?;
        Widget::from_native(&rt, unsafe { sys::cui_canvas(self.handle.ptr.as_ptr()) })
    }
    pub fn canvas_set_surface(&self, surface: &Surface) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_canvas_set_surface(self.handle.ptr.as_ptr(), surface.ptr.as_ptr())
        }))
    }
    pub fn canvas_set_regions(&self, regions: &[CanvasRegion]) -> Result<bool> {
        let _rt = self.handle.live()?;
        let names = regions
            .iter()
            .map(|r| string(&r.label))
            .collect::<Result<Vec<_>>>()?;
        let raw = regions
            .iter()
            .zip(&names)
            .map(|(r, n)| sys::cui_canvas_region {
                id: r.id,
                x: r.rect[0],
                y: r.rect[1],
                width: r.rect[2],
                height: r.rect[3],
                label: n.as_ptr(),
                enabled: r.enabled.into(),
            })
            .collect::<Vec<_>>();
        Ok(accepted(unsafe {
            sys::cui_canvas_set_regions(self.handle.ptr.as_ptr(), raw.as_ptr(), raw.len())
        }))
    }
    pub fn on_canvas_event(&self, callback: impl FnMut(CanvasEvent) + 'static) -> Result<()> {
        let rt = self.handle.live()?;
        let data = rt.keep(CanvasCallback {
            widget: self.clone(),
            callback: RefCell::new(Box::new(callback)),
        });
        unsafe { sys::cui_canvas_on_event(self.handle.ptr.as_ptr(), Some(canvas_callback), data) };
        Ok(())
    }
    pub fn clear_canvas_handler(&self) -> Result<()> {
        let _rt = self.handle.live()?;
        unsafe { sys::cui_canvas_on_event(self.handle.ptr.as_ptr(), None, std::ptr::null_mut()) };
        Ok(())
    }
    pub fn canvas_focus_region(&self, id: u32) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_canvas_focus_region(self.handle.ptr.as_ptr(), id)
        }))
    }
    pub fn set_opacity(&self, value: f64) -> Result<bool> {
        let _rt = self.handle.live()?;
        Ok(accepted(unsafe {
            sys::cui_set_opacity(self.handle.ptr.as_ptr(), value)
        }))
    }
    pub fn opacity(&self) -> Result<f64> {
        let _rt = self.handle.live()?;
        Ok(unsafe { sys::cui_get_opacity(self.handle.ptr.as_ptr()) })
    }
}
