use super::{fixture, model::color};
use cui::{chat::*, sys, App, ChatComponent, ChatKind, Icon, Result, Widget, Window};
use std::{cell::RefCell, rc::Rc};

#[derive(Clone)]
pub enum Intent {
    Verify,
    Answer(bool),
    Dismiss,
    CloseThread,
    Search(String),
    Notice(&'static str),
}
pub type Queue = Rc<RefCell<Vec<Intent>>>;
pub fn style(w: &Widget, theme: &Theme, bg: u32, radius: f64, padding: i32) -> Result<()> {
    w.set_style(Some(&sys::cui_widget_style {
        background: bg,
        foreground: theme.foreground,
        radius,
        padding,
        ..Default::default()
    }))?;
    Ok(())
}
pub fn label(parent: &Widget, value: &str, size: f64, weight: i32) -> Result<Widget> {
    let w = parent.label(value)?;
    w.set_font("sans", size * 0.75, weight)?;
    Ok(w)
}
pub fn button(
    parent: &Widget,
    text: &str,
    theme: &Theme,
    queue: &Queue,
    intent: Intent,
) -> Result<Widget> {
    let w = parent.button(text)?;
    style(&w, theme, theme.selected, 12., 8)?;
    let queue = queue.clone();
    w.on_action(move |_| queue.borrow_mut().push(intent.clone()))?;
    Ok(w)
}
fn spacer(parent: &Widget) -> Result<()> {
    parent.box_layout(sys::CUI_HORIZONTAL, 0)?.expand(true)
}
pub fn component(parent: &Widget, kind: ChatKind, theme: &Theme) -> Result<ChatComponent> {
    ChatComponent::create(parent, kind, theme)
}

pub struct Ui {
    pub window: Window,
    pub theme: Theme,
    pub base: Widget,
    backdrop: Widget,
    shadow_geometry: Option<(i32, i32, i32, u64)>,
    pub spaces: ChatComponent,
    pub nav: ChatComponent,
    pub title: Widget,
    pub header: ChatComponent,
    pub timeline: ChatComponent,
    pub composer: ChatComponent,
    pub inspector: ChatComponent,
    pub thread: Widget,
    pub thread_view: ChatComponent,
    pub thread_composer: ChatComponent,
    pub verify: Widget,
    pub verify_button: Widget,
    pub accept: Widget,
    pub scrim: Widget,
    pub scrim_canvas: Widget,
    pub toast: Widget,
    pub toast_text: Widget,
    pub queue: Queue,
}

fn top(parent: &Widget, theme: &Theme, queue: &Queue) -> Result<(ChatComponent, Widget)> {
    let row = parent.box_layout(sys::CUI_HORIZONTAL, 12)?;
    style(&row, theme, theme.background, 0., 6)?;
    row.set_min_size(1, 56)?;
    let logo = artwork::brand(theme)?;
    row.icon(Some(&logo))?.set_icon_size(22)?;
    label(&row, "Daylight", 18., 800)?;
    let spaces = component(&row, ChatKind::Spaces, theme)?;
    spaces.root.expand(false)?;
    spaces.part(0)?.set_min_size(760, 42)?;
    let mut items = fixture::spaces();
    items[0].avatar.color = theme.selected;
    spaces.rooms(&items)?;
    spaces.select(1)?;
    spacer(&row)?;
    let verify = button(&row, "Verify this session", theme, queue, Intent::Verify)?;
    verify.set_style(Some(&sys::cui_widget_style {
        background: theme.surface,
        foreground: theme.foreground,
        radius: 18.,
        border: cui::chat_color(0.76, 0.14, 75., 1.),
        border_width: 1.5,
        padding: 8,
    }))?;
    verify.set_icon(Some(&artwork::shield()?))?;
    verify.set_icon_size(16)?;
    verify.set_font("", 9.75, 600)?;
    let profile = button(
        &row,
        "Mathias",
        theme,
        queue,
        Intent::Notice(
            "Appearance is controlled by the application's theme and presentation settings",
        ),
    )?;
    style(&profile, theme, theme.surface, 18., 4)?;
    profile.set_icon(Some(&artwork::profile(theme)?))?;
    profile.set_icon_size(28)?;
    profile.set_font("", 9.75, 600)?;
    Ok((spaces, verify))
}
fn sidebar(parent: &Widget, theme: &Theme, queue: &Queue) -> Result<(Widget, ChatComponent)> {
    let side = parent.box_layout(sys::CUI_VERTICAL, 0)?;
    side.set_min_size(300, 1)?;
    style(&side, theme, theme.surface, 20., 0)?;
    let head = side.box_layout(sys::CUI_VERTICAL, 10)?;
    head.box_set_padding(16)?;
    let title = label(&head, "Home", 22., 700)?;
    let search = head.search("Filter rooms and people")?;
    style(&search, theme, theme.selected, 12., 8)?;
    let q = queue.clone();
    search.on_action(move |w| {
        q.borrow_mut()
            .push(Intent::Search(w.text().expect("search text")))
    })?;
    Ok((title, component(&side, ChatKind::Rooms, theme)?))
}
fn thread(
    parent: &Widget,
    theme: &Theme,
    queue: &Queue,
) -> Result<(Widget, ChatComponent, ChatComponent)> {
    let root = parent.box_layout(sys::CUI_VERTICAL, 0)?;
    root.set_min_size(340, 1)?;
    root.set_style(Some(&sys::cui_widget_style {
        background: theme.surface,
        foreground: theme.foreground,
        border: theme.border,
        border_width: 1.,
        ..Default::default()
    }))?;
    let head = root.box_layout(sys::CUI_HORIZONTAL, 12)?;
    head.box_set_padding(18)?;
    head.set_min_size(1, 62)?;
    label(&head, "Thread", 15., 700)?.expand(true)?;
    button(&head, "×", theme, queue, Intent::CloseThread)?;
    let mut t = theme.clone();
    t.background = t.surface;
    let view = component(&root, ChatKind::Timeline, &t)?;
    let composer = component(&root, ChatKind::Composer, theme)?;
    let mut p = composer.presentation()?;
    p.composer_tools = 0;
    p.composer_padding = 14.;
    composer.set_presentation(&p)?;
    composer.part(0)?.set_placeholder("Reply in thread…")?;
    root.set_visible(false)?;
    Ok((root, view, composer))
}
impl Ui {
    pub fn new(app: &App, width: i32, height: i32) -> Result<Self> {
        let mut theme = Theme::new(Appearance::Daylight);
        if std::env::var_os("CUI_LARGE_TEXT").is_some() {
            theme.font_size *= 1.5;
        }
        let window = app.window("CUI Daylight - Rust", width, height)?;
        window.frame(false, true, 0.)?;
        let root = window.root()?;
        root.box_set_padding(0)?;
        root.set_font("sans", 10.5, 400)?;
        style(&root, &theme, theme.background, 0., 0)?;
        let layers = root.stack()?;
        let background = layers.stack_layer(sys::CUI_LAYER_FILL, 0, 0, 0)?;
        let backdrop = background.canvas()?;
        backdrop.expand(true)?;
        backdrop.set_min_size(1100, 500)?;
        let base = layers.stack_layer(sys::CUI_LAYER_FILL, 0, 0, 0)?;
        let page = base.box_layout(sys::CUI_VERTICAL, 0)?;
        page.expand(true)?;
        let queue = Queue::default();
        let (spaces, verify_button) = top(&page, &theme, &queue)?;
        let body_margin = page.box_layout(sys::CUI_HORIZONTAL, 0)?;
        body_margin.expand(true)?;
        body_margin
            .box_layout(sys::CUI_VERTICAL, 0)?
            .set_min_size(12, 1)?;
        let body = body_margin.box_layout(sys::CUI_HORIZONTAL, 12)?;
        body.expand(true)?;
        body_margin
            .box_layout(sys::CUI_VERTICAL, 0)?
            .set_min_size(12, 1)?;
        page.box_layout(sys::CUI_HORIZONTAL, 0)?
            .set_min_size(1, 12)?;
        let (title, nav) = sidebar(&body, &theme, &queue)?;
        let main = body.box_layout(sys::CUI_VERTICAL, 0)?;
        main.expand(true)?;
        style(&main, &theme, theme.surface, 20., 0)?;
        let header = component(&main, ChatKind::Header, &theme)?;
        header.root.expand(false)?;
        header.part(0)?.set_min_size(1, 72)?;
        let timeline = component(&main, ChatKind::Timeline, &theme)?;
        let composer = component(&main, ChatKind::Composer, &theme)?;
        let inspector = component(&body, ChatKind::Inspector, &theme)?;
        inspector.root.expand(false)?;
        inspector.root.set_min_size(300, 1)?;
        let (thread, thread_view, thread_composer) = thread(&body, &theme, &queue)?;
        let (scrim, scrim_canvas, verify, accept) =
            super::overlays::verification(&layers, &theme, &queue)?;
        let toast = layers.stack_layer(sys::CUI_LAYER_BOTTOM, 500, 0, 28)?;
        style(&toast, &theme, theme.foreground, 12., 12)?;
        let toast_text = label(&toast, "", 13., 500)?;
        toast_text.set_style(Some(&sys::cui_widget_style {
            foreground: theme.surface,
            ..Default::default()
        }))?;
        toast.set_visible(false)?;
        Ok(Self {
            window,
            theme,
            base,
            backdrop,
            shadow_geometry: None,
            spaces,
            nav,
            title,
            header,
            timeline,
            composer,
            inspector,
            thread,
            thread_view,
            thread_composer,
            verify,
            verify_button,
            accept,
            scrim,
            scrim_canvas,
            toast,
            toast_text,
            queue,
        })
    }
    fn shadows(&mut self, panel: i32, scale: f64) -> Result<()> {
        let Some((width, height)) = self.backdrop.allocated_size()? else {
            return Ok(());
        };
        let geometry = (width, height, panel, scale.to_bits());
        if width < 1 || height < 1 || self.shadow_geometry == Some(geometry) {
            return Ok(());
        }
        let surface = cui::Surface::new(width, height, scale)?;
        let mut scene = cui::Scene::new();
        scene.clear(self.theme.background);
        let main_width = width - 336 - if panel > 0 { panel + 12 } else { 0 };
        for (x, w) in [(12, 300), (324, main_width), (width - 12 - panel, panel)] {
            if w == 0 {
                continue;
            }
            for (offset, blur) in [(1., 2.), (8., 24.)] {
                scene.shadow(
                    [x as f32, 56. + offset, w as f32, (height - 68) as f32],
                    20.,
                    blur,
                    cui::chat_color(0.30, 0.05, 250., 0.06),
                );
            }
        }
        surface.render(&scene)?;
        self.backdrop.canvas_set_surface(&surface)?;
        self.shadow_geometry = Some(geometry);
        Ok(())
    }
    pub fn refresh(&mut self, panel: i32) -> Result<()> {
        let scale = self.window.scale()?;
        self.shadows(panel, scale)?;
        for c in [
            &self.spaces,
            &self.nav,
            &self.header,
            &self.timeline,
            &self.composer,
            &self.inspector,
            &self.thread_view,
            &self.thread_composer,
        ] {
            c.refresh(scale)?;
        }
        if let Some((w, h)) = self.scrim_canvas.allocated_size()? {
            if w > 0 && h > 0 {
                self.scrim_canvas.canvas_set_regions(&[cui::CanvasRegion {
                    id: 1,
                    rect: [0., 0., w as f32, h as f32],
                    label: "Dismiss verification".into(),
                    enabled: true,
                    role: 0,
                }])?;
            }
        }
        Ok(())
    }
}
mod artwork {
    use super::*;
    fn command(op: i32, p: &[f32], color: u32, foreground: i32) -> sys::cui_icon_command {
        let mut c = sys::cui_icon_command {
            op,
            rgba: color,
            current_color: foreground,
            ..Default::default()
        };
        c.values[..p.len()].copy_from_slice(p);
        c
    }
    pub fn brand(t: &Theme) -> Result<Icon> {
        use sys::*;
        let c = |op, p: &[f32], color| command(op, p, color, 0);
        Icon::vector(
            22.,
            22.,
            &[
                c(CUI_ICON_MOVE, &[7., 0.], 0),
                c(CUI_ICON_LINE, &[15., 0.], 0),
                c(CUI_ICON_CUBIC, &[19., 0., 22., 3., 22., 7.], 0),
                c(CUI_ICON_LINE, &[22., 15.], 0),
                c(CUI_ICON_CUBIC, &[22., 19., 19., 22., 15., 22.], 0),
                c(CUI_ICON_LINE, &[7., 22.], 0),
                c(CUI_ICON_CUBIC, &[3., 22., 0., 19., 0., 15.], 0),
                c(CUI_ICON_LINE, &[0., 7.], 0),
                c(CUI_ICON_CUBIC, &[0., 3., 3., 0., 7., 0.], 0),
                c(CUI_ICON_CLOSE, &[], 0),
                c(CUI_ICON_FILL, &[], t.foreground),
                c(CUI_ICON_MOVE, &[15., 11.], 0),
                c(CUI_ICON_CUBIC, &[15., 13.21, 13.21, 15., 11., 15.], 0),
                c(CUI_ICON_CUBIC, &[8.79, 15., 7., 13.21, 7., 11.], 0),
                c(CUI_ICON_CUBIC, &[7., 8.79, 8.79, 7., 11., 7.], 0),
                c(CUI_ICON_CUBIC, &[13.21, 7., 15., 8.79, 15., 11.], 0),
                c(CUI_ICON_CLOSE, &[], 0),
                c(CUI_ICON_FILL, &[], t.surface),
            ],
        )
    }
    pub fn shield() -> Result<Icon> {
        use sys::*;
        let c = |op, p: &[f32]| command(op, p, 0, 1);
        Icon::vector(
            24.,
            24.,
            &[
                c(CUI_ICON_MOVE, &[12., 3.]),
                c(CUI_ICON_LINE, &[4., 6.]),
                c(CUI_ICON_LINE, &[4., 12.]),
                c(CUI_ICON_CUBIC, &[4., 17., 7.5, 20., 12., 21.]),
                c(CUI_ICON_CUBIC, &[16.5, 20., 20., 17., 20., 12.]),
                c(CUI_ICON_LINE, &[20., 6.]),
                c(CUI_ICON_CLOSE, &[]),
                c(CUI_ICON_STROKE, &[1.9, 1., 1.]),
                c(CUI_ICON_MOVE, &[12., 8.]),
                c(CUI_ICON_LINE, &[12., 12.]),
                c(CUI_ICON_STROKE, &[1.9, 1., 1.]),
                c(CUI_ICON_MOVE, &[12., 15.5]),
                c(CUI_ICON_LINE, &[12., 15.51]),
                c(CUI_ICON_STROKE, &[1.9, 1., 1.]),
            ],
        )
    }
    pub fn profile(t: &Theme) -> Result<Icon> {
        use sys::*;
        let c = |op, p: &[f32], color| command(op, p, color, 0);
        Icon::vector(
            28.,
            28.,
            &[
                c(CUI_ICON_MOVE, &[28., 14.], 0),
                c(CUI_ICON_CUBIC, &[28., 21.73, 21.73, 28., 14., 28.], 0),
                c(CUI_ICON_CUBIC, &[6.27, 28., 0., 21.73, 0., 14.], 0),
                c(CUI_ICON_CUBIC, &[0., 6.27, 6.27, 0., 14., 0.], 0),
                c(CUI_ICON_CUBIC, &[21.73, 0., 28., 6.27, 28., 14.], 0),
                c(CUI_ICON_CLOSE, &[], 0),
                c(CUI_ICON_FILL, &[], color("Mathias")),
                c(CUI_ICON_MOVE, &[10., 19.], 0),
                c(CUI_ICON_LINE, &[10., 9.], 0),
                c(CUI_ICON_LINE, &[14., 15.], 0),
                c(CUI_ICON_LINE, &[18., 9.], 0),
                c(CUI_ICON_LINE, &[18., 19.], 0),
                c(CUI_ICON_STROKE, &[1.8, 0., 1.], t.foreground),
            ],
        )
    }
}
