//! Waypoint: a local simulator workspace, built entirely with CUI drawing APIs.
use cui::{sys::*, App, CanvasEvent, CanvasRegion, Icon, Result, Scene, Surface, Widget};
use std::{cell::RefCell, rc::Rc};
const W: i32 = 1280;
const H: i32 = 900;
// Apple iPhone 16 Pro: 71.5 × 149.6 mm; 1206 × 2622 pixels at 460 ppi.
// https://support.apple.com/en-au/121031
const PHONE_BODY_RATIO: f32 = 71.5 / 149.6;
const PHONE_SCREEN_RATIO: f32 = 1206. / 2622.;
const SCREEN_BODY_HEIGHT: f32 = (2622. / 460. * 25.4) / 149.6;
const WHITE: u32 = 0xf4f4f8ff;
const MUTED: u32 = 0xc5b9c8ff;
const BLUE: u32 = 0x7cacffff;
const PLAY: u32 = 1;
const RESET: u32 = 2;
const SPEED: u32 = 3;
const ROTATE: u32 = 4;
const NOTICE: u32 = 5;
const PRECISE: u32 = 6;
const PERMIT: u32 = 7;
const SAVE: u32 = 8;
const REMOVE: u32 = 9;
const ZOOM_IN: u32 = 10;
const ZOOM_OUT: u32 = 11;
const THEME: u32 = 12;
const REDUCED: u32 = 13;
const CAPTURE: u32 = 14;
const CLOSE: u32 = 15;
const ALLOW: u32 = 16;
const DENY: u32 = 17;
const DISMISS: u32 = 18;
const MAP: u32 = 19;
#[derive(Clone)]
struct Place {
    name: String,
    subtitle: String,
    x: f32,
    y: f32,
}
struct Model {
    places: Vec<Place>,
    selected: usize,
    page: usize,
    point: [f32; 2],
    running: bool,
    progress: f32,
    speed: f32,
    precise: bool,
    permission: bool,
    sheet: bool,
    notice: bool,
    landscape: bool,
    light: bool,
    reduced: bool,
    zoom: f32,
    pan: [f32; 2],
    drag: Option<[f32; 2]>,
    moved: bool,
    tab: u32,
    focus: u32,
    preview: Option<Icon>,
    large: bool,
}
impl Model {
    fn new() -> Self {
        Self {
            places: vec![
                Place {
                    name: "Marina Bay".into(),
                    subtitle: "37.7821° N  ·  122.4057° W".into(),
                    x: 112.,
                    y: 382.,
                },
                Place {
                    name: "Juniper Park".into(),
                    subtitle: "37.7856° N  ·  122.4021° W".into(),
                    x: 226.,
                    y: 202.,
                },
                Place {
                    name: "Design District".into(),
                    subtitle: "37.7789° N  ·  122.4102° W".into(),
                    x: 86.,
                    y: 265.,
                },
            ],
            selected: 0,
            page: 0,
            point: [112., 382.],
            running: false,
            progress: 0.,
            speed: 1.,
            precise: true,
            permission: true,
            sheet: false,
            notice: false,
            landscape: false,
            light: false,
            reduced: false,
            zoom: 1.,
            pan: [0., 0.],
            drag: None,
            moved: false,
            tab: 0,
            focus: 0,
            preview: None,
            large: std::env::var_os("CUI_LARGE_TEXT").is_some(),
        }
    }
    fn phone(&self) -> [f32; 4] {
        if self.landscape {
            [108., 306., 550., 550. * PHONE_BODY_RATIO]
        } else {
            [
                411. - 674. * PHONE_BODY_RATIO / 2.,
                142.,
                674. * PHONE_BODY_RATIO,
                674.,
            ]
        }
    }
    fn screen(&self) -> [f32; 4] {
        let p = self.phone();
        let long = p[2].max(p[3]) * SCREEN_BODY_HEIGHT;
        let short = long * PHONE_SCREEN_RATIO;
        let (w, h) = if self.landscape {
            (long, short)
        } else {
            (short, long)
        };
        [p[0] + (p[2] - w) / 2., p[1] + (p[3] - h) / 2., w, h]
    }
    fn advance(&mut self, seconds: f32) {
        if self.running {
            self.progress = (self.progress + seconds * self.speed / 45.).min(1.);
            let p = self.progress * 4.;
            let route = [
                [112., 382.],
                [145., 330.],
                [172., 286.],
                [222., 264.],
                [226., 202.],
            ];
            let i = (p as usize).min(3);
            let t = (p - i as f32).min(1.);
            self.point = [
                route[i][0] + (route[i + 1][0] - route[i][0]) * t,
                route[i][1] + (route[i + 1][1] - route[i][1]) * t,
            ];
            if self.progress >= 1. {
                self.running = false;
            }
        }
    }
    fn activate(&mut self, id: u32) {
        match id % 1000 {
            PLAY => {
                if self.progress >= 1. {
                    self.progress = 0.;
                }
                self.running = !self.running
            }
            20 => self.page = self.page.saturating_sub(1),
            21 => self.page = (self.page + 1).min((self.places.len() - 1) / 3),
            RESET => {
                self.progress = 0.;
                self.point = [112., 382.];
                self.running = false
            }
            SPEED => {
                self.speed = if self.speed >= 4. {
                    1.
                } else {
                    self.speed * 2.
                }
            }
            ROTATE => {
                self.landscape = !self.landscape;
                let r = self.screen();
                self.pan = [
                    r[2] / 2. - self.point[0] * self.zoom,
                    r[3] / 2. - self.point[1] * self.zoom,
                ];
            }
            NOTICE => self.notice = true,
            PRECISE => self.precise = !self.precise,
            PERMIT => self.sheet = true,
            SAVE if self.places.len() < 32 => {
                self.places.push(Place {
                    name: format!("Saved location {}", self.places.len() - 2),
                    subtitle: "Custom pin · just now".into(),
                    x: self.point[0],
                    y: self.point[1],
                });
                self.selected = self.places.len() - 1;
                self.page = self.selected / 3;
            }
            REMOVE => {
                if self.selected >= 3 {
                    self.places.remove(self.selected);
                    self.selected = 0;
                    self.page = 0;
                    self.point = [self.places[0].x, self.places[0].y];
                }
            }
            ZOOM_IN => self.zoom = (self.zoom * 1.2).min(2.5),
            ZOOM_OUT => self.zoom = (self.zoom / 1.2).max(0.65),
            THEME => self.light = !self.light,
            REDUCED => self.reduced = !self.reduced,
            CLOSE => self.preview = None,
            ALLOW => {
                self.permission = true;
                self.sheet = false
            }
            DENY => {
                self.permission = false;
                self.sheet = false
            }
            DISMISS => self.notice = false,
            100..=199 => {
                let i = (id - 100) as usize;
                if i < self.places.len() {
                    self.selected = i;
                    self.point = [self.places[i].x, self.places[i].y];
                    self.running = false;
                }
            }
            200..=202 => self.tab = id - 200,
            _ => {}
        }
    }
    fn event(&mut self, e: CanvasEvent) {
        match e.kind {
            CUI_CANVAS_ACTIVATE => {
                if e.id != MAP {
                    self.activate(e.id);
                }
            }
            CUI_CANVAS_FOCUS => self.focus = e.id,
            CUI_CANVAS_PRESS if e.id == MAP => {
                self.drag = Some([e.x as f32, e.y as f32]);
                self.moved = false;
            }
            CUI_CANVAS_MOVE => {
                if let Some(last) = self.drag {
                    let next = [e.x as f32, e.y as f32];
                    let dx = next[0] - last[0];
                    let dy = next[1] - last[1];
                    self.pan[0] += dx;
                    self.pan[1] += dy;
                    self.drag = Some(next);
                    self.moved |= dx.abs() + dy.abs() > 1.;
                }
            }
            CUI_CANVAS_RELEASE => {
                if self.drag.take().is_some() && !self.moved {
                    let r = self.screen();
                    self.point = [
                        (e.x as f32 - r[0] - self.pan[0]) / self.zoom,
                        (e.y as f32 - r[1] - self.pan[1]) / self.zoom,
                    ];
                    self.running = false;
                }
            }
            CUI_CANVAS_SCROLL if e.id == MAP => {
                self.zoom = (self.zoom * (1. - e.dy as f32 * 0.08)).clamp(0.65, 2.5)
            }
            _ => {}
        }
    }
}
struct Art {
    play: Icon,
    pause: Icon,
    pin: Icon,
    rotate: Icon,
    plus: Icon,
    minus: Icon,
    camera: Icon,
    reset: Icon,
    backdrop: Option<Icon>,
}
fn vector(lines: &[[f32; 4]]) -> Result<Icon> {
    let mut commands = Vec::new();
    for l in lines {
        commands.push(cui_icon_command {
            op: CUI_ICON_MOVE,
            values: [l[0], l[1], 0., 0., 0., 0.],
            ..Default::default()
        });
        commands.push(cui_icon_command {
            op: CUI_ICON_LINE,
            values: [l[2], l[3], 0., 0., 0., 0.],
            ..Default::default()
        });
        commands.push(cui_icon_command {
            op: CUI_ICON_STROKE,
            values: [1.8, 1., 1., 0., 0., 0.],
            current_color: 1,
            ..Default::default()
        });
    }
    Icon::vector(24., 24., &commands)
}
impl Art {
    fn new() -> Result<Self> {
        Ok(Self {
            backdrop: None,
            play: Icon::symbol(CUI_SYMBOL_PLAY)?,
            pause: Icon::symbol(CUI_SYMBOL_PAUSE)?,
            pin: vector(&[
                [12., 3., 5., 10.],
                [5., 10., 12., 22.],
                [12., 22., 19., 10.],
                [19., 10., 12., 3.],
                [10., 10., 14., 10.],
            ])?,
            rotate: vector(&[
                [6., 8., 6., 18.],
                [6., 18., 18., 18.],
                [18., 18., 18., 8.],
                [18., 8., 6., 8.],
                [3., 6., 7., 2.],
                [7., 2., 11., 6.],
            ])?,
            plus: vector(&[[12., 5., 12., 19.], [5., 12., 19., 12.]])?,
            minus: vector(&[[5., 12., 19., 12.]])?,
            camera: vector(&[
                [3., 7., 21., 7.],
                [21., 7., 21., 20.],
                [21., 20., 3., 20.],
                [3., 20., 3., 7.],
                [8., 7., 9., 4.],
                [9., 4., 15., 4.],
                [15., 4., 16., 7.],
                [9., 11., 15., 11.],
                [15., 11., 15., 16.],
                [15., 16., 9., 16.],
                [9., 16., 9., 11.],
            ])?,
            reset: vector(&[
                [6., 6., 18., 6.],
                [18., 6., 21., 12.],
                [21., 12., 18., 19.],
                [18., 19., 8., 19.],
                [8., 19., 5., 15.],
                [6., 6., 6., 1.],
                [6., 6., 11., 6.],
            ])?,
        })
    }
}
struct View<'a> {
    s: Scene,
    regions: Vec<CanvasRegion>,
    m: &'a Model,
    a: &'a Art,
}
impl<'a> View<'a> {
    fn new(m: &'a Model, a: &'a Art) -> Self {
        Self {
            s: Scene::new(),
            regions: Vec::new(),
            m,
            a,
        }
    }
    #[allow(clippy::too_many_arguments)] // Compact scene typography helper: geometry and paint.
    fn txt(&mut self, x: f32, y: f32, w: f32, size: f32, weight: i32, color: u32, text: &str) {
        self.s
            .text(
                [x, y],
                w,
                if self.m.large { size * 1.5 } else { size },
                weight,
                color,
                text,
            )
            .unwrap();
    }
    fn region(&mut self, id: u32, r: [f32; 4], label: &str, enabled: bool) {
        self.regions.push(CanvasRegion {
            id,
            rect: r,
            label: label.into(),
            enabled,
        });
        if self.m.focus == id {
            let [x, y, w, h] = r;
            for (from, to) in [
                ([x - 2., y - 2.], [x + w + 2., y - 2.]),
                ([x + w + 2., y - 2.], [x + w + 2., y + h + 2.]),
                ([x + w + 2., y + h + 2.], [x - 2., y + h + 2.]),
                ([x - 2., y + h + 2.], [x - 2., y - 2.]),
            ] {
                self.s.line(from, to, 2., 0x8ab5ffaa);
            }
        }
    }
    fn button(&mut self, id: u32, r: [f32; 4], icon: &Icon, label: &str) {
        self.region(
            id,
            r,
            label,
            match id {
                SAVE => self.m.places.len() < 32,
                REMOVE => self.m.selected >= 3,
                20 => self.m.page > 0,
                21 => (self.m.page + 1) * 3 < self.m.places.len(),
                _ => true,
            },
        );
        self.s.rect(r, 10., 0xffffff12);
        self.s.icon(
            icon,
            [r[0] + (r[2] - 20.) / 2., r[1] + (r[3] - 20.) / 2., 20., 20.],
            WHITE,
        );
    }
    fn text_button(&mut self, id: u32, r: [f32; 4], label: &str, primary: bool) {
        self.region(
            id,
            r,
            label,
            match id {
                SAVE => self.m.places.len() < 32,
                REMOVE => self.m.selected >= 3,
                20 => self.m.page > 0,
                21 => (self.m.page + 1) * 3 < self.m.places.len(),
                _ => true,
            },
        );
        self.s
            .shadow([r[0], r[1] + 2., r[2], r[3]], 10., 3., 0x100d182d);
        self.s
            .rect(r, 10., if primary { 0xb2ccffff } else { 0x82738180 });
        self.s.rect(
            [r[0] + 1., r[1] + 1., r[2] - 2., r[3] - 2.],
            9.,
            if primary { 0x83aaffff } else { 0x504453ff },
        );
        self.s
            .text_box(
                [r[0] + 6., r[1], r[2] - 12., r[3]],
                if self.m.large { 18. } else { 12. },
                600,
                if primary { 0x182540ff } else { WHITE },
                label,
                CUI_DRAW_ALIGN_CENTER,
            )
            .unwrap();
    }

    fn background(&mut self) {
        self.s.clear(0x221628ff);
        self.s
            .gradient([0., 0., 1280., 900.], 0., 0x422331ff, 0x172735ff);
        self.s.ellipse([-260., 240., 1460., 920.], 0xcb644bff);
        self.s.ellipse([-210., 328., 1320., 830.], 0xa64547ff);
        self.s.ellipse([-180., 432., 1310., 850.], 0x77394eff);
        self.s.ellipse([304., 562., 1360., 660.], 0x382d4aff);
        self.s.ellipse([546., -90., 960., 420.], 0x98595244);
        self.s.material([0., 0., 1280., 900.], 0., 25., 0x1a152322);
        self.s.rect([46., 34., 34., 34.], 10., 0xc9dbecff);
        self.s.icon(&self.a.pin, [53., 40., 20., 22.], 0x233142ff);
        self.txt(92., 32., 190., 22., 700, WHITE, "Waypoint");
        self.txt(
            94.,
            if self.m.large { 76. } else { 61. },
            220.,
            9.,
            600,
            0xd2bcc8ff,
            "SIMULATOR STUDIO",
        );
        self.s.rect([1003., 40., 225., 31.], 16., 0x241c3288);
        self.s.ellipse([1018., 52., 6., 6.], 0x92d3b0ff);
        self.txt(
            1033.,
            46.,
            180.,
            11.,
            500,
            0xe4dfe8ff,
            if self.m.large {
                "Local workspace"
            } else {
                "Local workspace  /  Design team"
            },
        );
        self.txt(
            48.,
            853.,
            500.,
            11.,
            400,
            0xcbb8caff,
            "A little space to explore.   All locations and routes are fictional.",
        );
        self.txt(
            1010.,
            853.,
            230.,
            11.,
            500,
            0xcbb8caff,
            if self.m.large {
                "CUI / Rust"
            } else {
                "CUI   /   Rust   /   Native canvas"
            },
        );
    }
    fn map(&mut self, r: [f32; 4]) {
        let light = self.m.light;
        self.s.save();
        self.s.clip(r, 38.);
        self.s
            .rect(r, 0., if light { 0xe2e7e2ff } else { 0x17272cff });
        self.s.translate(r[0] + self.m.pan[0], r[1] + self.m.pan[1]);
        self.s.scale(self.m.zoom, self.m.zoom);
        self.s.ellipse(
            [208., -150., 300., 920.],
            if light { 0x8bb8c3ff } else { 0x183e4aff },
        );
        self.s.rect(
            [20., 137., 132., 82.],
            22.,
            if light { 0xb2cbb3ff } else { 0x274337ff },
        );
        self.s.rect(
            [164., 158., 85., 115.],
            18.,
            if light { 0xacccb8ff } else { 0x294f42ff },
        );
        self.s.rect(
            [-30., 453., 142., 163.],
            24.,
            if light { 0xadc9afff } else { 0x27483aff },
        );
        for i in -3..12 {
            let y = i as f32 * 53. + 30.;
            self.s.line(
                [-200., y],
                [420., y - 95.],
                7.,
                if light { 0xf8f7edff } else { 0x33464bff },
            );
            self.s.line(
                [-200., y],
                [420., y - 95.],
                1.,
                if light { 0xc7cfc8ff } else { 0x516268ff },
            );
        }
        for i in -4..12 {
            let x = i as f32 * 55.;
            self.s.line(
                [x, -200.],
                [x + 190., 760.],
                7.,
                if light { 0xf8f7edff } else { 0x33464bff },
            );
        }
        self.s.line(
            [-45., 640.],
            [220., -80.],
            13.,
            if light { 0xf4d798ff } else { 0x766853ff },
        );
        self.s.line(
            [-45., 640.],
            [220., -80.],
            3.,
            if light { 0xe8b76bff } else { 0xba9470ff },
        );
        let route = [
            [112., 382.],
            [145., 330.],
            [172., 286.],
            [222., 264.],
            [226., 202.],
        ];
        for p in route.windows(2) {
            self.s.line(p[0], p[1], 8., 0x173361ff);
            self.s.line(p[0], p[1], 4., 0x75abffff);
        }
        let map_text = if light { 0x53645cff } else { 0x98adb1ff };
        self.txt(7., 93., 220., 10., 600, map_text, "HARBOR CITY");
        self.txt(173., 176., 120., 9., 500, map_text, "Juniper Park");
        self.txt(27., 287., 175., 9., 500, map_text, "DESIGN DISTRICT");
        self.txt(230., 380., 130., 10., 400, 0x77a4b6ff, "Marina Bay");
        self.txt(38., 446., 170., 9., 400, map_text, "Waterfront Avenue");
        if self.m.permission {
            let [x, y] = self.m.point;
            let radius = if self.m.precise { 28. } else { 68. };
            self.s.ellipse(
                [x - radius, y - radius, radius * 2., radius * 2.],
                0x69a8ff32,
            );
            self.s
                .shadow([x - 9., y - 9., 18., 18.], 9., 8., 0x00000060);
            self.s.ellipse([x - 9., y - 9., 18., 18.], 0xffffffff);
            self.s.ellipse([x - 6., y - 6., 12., 12.], 0x4898ffff);
        }
        self.s.restore();
    }
    fn phone(&mut self) {
        let p = self.m.phone();
        self.s.shadow(
            [p[0] - 2., p[1] + 10., p[2] + 4., p[3] + 4.],
            54.,
            24.,
            0x060710aa,
        );
        self.s.gradient(p, 50., 0x656872ff, 0x16171dff);
        self.s.rect(
            [p[0] + 3., p[1] + 3., p[2] - 6., p[3] - 6.],
            48.,
            0x06080bff,
        );
        let r = self.m.screen();
        self.map(r);
        self.region(
            MAP,
            r,
            "Map: click to set location, drag to pan, scroll to zoom",
            !self.m.sheet,
        );
        if self.m.landscape {
            self.s.rect(
                [p[0] + 20., p[1] + p[3] / 2. - 48., 26., 96.],
                13.,
                0x070a0cff,
            );
            self.s
                .ellipse([p[0] + 28., p[1] + p[3] / 2. + 27., 9., 9.], 0x142329ff);
        } else {
            self.s.rect(
                [p[0] + p[2] / 2. - 48., p[1] + 20., 96., 27.],
                14.,
                0x070a0cff,
            );
            self.s
                .ellipse([p[0] + p[2] / 2. + 27., p[1] + 29., 9., 9.], 0x142329ff);
        }
        self.txt(p[0] + 29., p[1] + 26., 66., 12., 600, WHITE, "9:41");
        self.s
            .rect([p[0] + p[2] - 48., p[1] + 31., 20., 9.], 3., 0xe6ecf0dd);
        self.s
            .rect([p[0] + p[2] - 26., p[1] + 34., 2., 3.], 1., 0xe6ecf0dd);
        self.s.material(
            [r[0] + 15., r[1] + 56., r[2] - 30., 42.],
            14.,
            9.,
            0x182329d9,
        );
        self.s
            .icon(&self.a.pin, [r[0] + 27., r[1] + 69., 16., 16.], 0xb7c9d4ff);
        self.txt(
            r[0] + 53.,
            r[1] + 68.,
            r[2] - 100.,
            12.,
            400,
            0xc1ccd3ff,
            "Search Harbor City",
        );
        let bottom = p[1] + p[3] - 122.;
        self.s
            .material([r[0] + 14., bottom, r[2] - 28., 81.], 20., 12., 0x20262aed);
        self.txt(
            r[0] + 29.,
            bottom + 13.,
            r[2] - 62.,
            15.,
            600,
            WHITE,
            if self.m.permission {
                "Waterfront Walk"
            } else {
                "Location unavailable"
            },
        );
        self.txt(
            r[0] + 29.,
            bottom + 40.,
            r[2] - 62.,
            11.,
            400,
            0xadbcc6ff,
            if self.m.permission {
                "1.8 km  ·  4 min  ·  Walking route"
            } else {
                "Allow location in App permissions"
            },
        );
        self.s.rect(
            [p[0] + p[2] / 2. - 56., p[1] + p[3] - 25., 112., 4.],
            2.,
            0xf4f4f8dd,
        );
        if self.m.notice {
            self.s.material(
                [r[0] + 14., r[1] + 111., r[2] - 28., 70.],
                18.,
                12.,
                0xeef0f6ef,
            );
            self.txt(
                r[0] + 28.,
                r[1] + 121.,
                r[2] - 58.,
                10.,
                700,
                0x4e5664ff,
                "WAYPOINT  ·  now",
            );
            self.txt(
                r[0] + 28.,
                r[1] + 143.,
                r[2] - 58.,
                12.,
                500,
                0x273340ff,
                "You’ve arrived somewhere new.",
            );
            self.region(
                DISMISS,
                [r[0] + 14., r[1] + 111., r[2] - 28., 70.],
                "Dismiss notification",
                true,
            );
        }
        if self.m.sheet {
            let y = p[1] + p[3] / 2. - 90.;
            self.s
                .material([r[0] + 16., y, r[2] - 32., 191.], 22., 16., 0x292d3af5);
            self.txt(
                r[0] + 34.,
                y + 19.,
                r[2] - 66.,
                17.,
                650,
                WHITE,
                "Allow location access?",
            );
            self.txt(
                r[0] + 34.,
                y + 54.,
                r[2] - 66.,
                11.,
                400,
                MUTED,
                "Waypoint uses a simulated location.",
            );
            self.text_button(
                ALLOW,
                [r[0] + 32., y + 85., r[2] - 64., 39.],
                "Allow while using the app",
                true,
            );
            self.text_button(
                DENY,
                [r[0] + 32., y + 134., r[2] - 64., 36.],
                "Don’t allow",
                false,
            );
        }
    }
    fn toolbar(&mut self) {
        let p = self.m.phone();
        let y = if self.m.landscape { 228. } else { 87. };
        let x = p[0] + (p[2] - 246.) / 2.;
        self.s.shadow([x, y + 4., 246., 42.], 13., 12., 0x100a1866);
        self.s.material([x, y, 246., 42.], 13., 10., 0x24222acb);
        self.txt(
            x + 16.,
            y + if self.m.large { 9. } else { 12. },
            118.,
            12.,
            600,
            WHITE,
            if self.m.large {
                "iPhone Pro"
            } else {
                "iPhone 16 Pro"
            },
        );
        self.button(
            ROTATE,
            [x + 148., y + 5., 34., 32.],
            &self.a.rotate,
            "Rotate device",
        );
        self.button(
            CAPTURE,
            [x + 195., y + 5., 34., 32.],
            &self.a.camera,
            "Capture preview",
        );
    }
    fn inspector(&mut self) {
        let x = 710.;
        let y = 155.;
        let r = [x, y, 382., 648.];
        self.s
            .shadow([x, y + 14., 382., 648.], 23., 22., 0x130b1c66);
        if self.m.reduced {
            self.s.rect(r, 23., 0x342d3dff);
        } else {
            self.s.material(r, 23., 20., 0x332632c9);
        }

        self.s.icon(&self.a.pin, [x + 22., y + 21., 20., 23.], BLUE);
        self.txt(x + 54., y + 20., 280., 17., 600, WHITE, "Device inspector");
        self.s.ellipse([x + 345., y + 29., 7., 7.], 0x9ce3baff);
        self.txt(
            x + 24.,
            y + 55.,
            330.,
            11.,
            400,
            MUTED,
            "iPhone 16 Pro  ·  iOS mock  ·  Connected",
        );
        self.s.rect([x + 20., y + 89., 342., 38.], 10., 0x120e222f);
        for (i, label) in ["Location", "Device", "Appearance"].iter().enumerate() {
            let xx = x + 24. + i as f32 * 112.;
            if self.m.tab == i as u32 {
                self.s.rect([xx, y + 93., 108., 30.], 8., 0xffffff17);
            }
            self.region(200 + i as u32, [xx, y + 93., 108., 30.], label, true);
            self.txt(
                xx + 13.,
                y + 100.,
                100.,
                11.,
                500,
                if self.m.tab == i as u32 { WHITE } else { MUTED },
                label,
            );
        }
        match self.m.tab {
            0 => self.locations(x, y),
            1 => self.device(x, y),
            _ => self.appearance(x, y),
        }
        self.s
            .line([x + 22., y + 600.], [x + 360., y + 600.], 1., 0xffffff19);
        self.s.ellipse([x + 25., y + 619., 6., 6.], 0x92d3b0ff);
        self.txt(
            x + 40.,
            y + 612.,
            290.,
            10.,
            400,
            MUTED,
            if self.m.running {
                "Route playback in progress"
            } else {
                "Ready for your next experiment"
            },
        );
    }
    fn locations(&mut self, x: f32, y: f32) {
        self.txt(x + 24., y + 151., 230., 10., 600, MUTED, "SAVED LOCATIONS");
        self.button(
            SAVE,
            [x + 318., y + 140., 34., 30.],
            &self.a.plus,
            "Save current location",
        );
        self.text_button(20, [x + 229., y + 140., 36., 30.], "‹", false);
        self.text_button(21, [x + 270., y + 140., 36., 30.], "›", false);
        let start = self.m.page * 3;
        for i in start..(start + 3).min(self.m.places.len()) {
            let yy = y + 181. + (i - start) as f32 * 58.;
            let place = &self.m.places[i];
            let name = place.name.clone();
            let subtitle = place.subtitle.clone();
            let selected = self.m.selected == i;
            let r = [x + 18., yy - 4., 346., 52.];
            self.region(100 + i as u32, r, &format!("Select {name}"), true);
            if selected {
                self.s.rect(r, 11., 0xf2d4ce1b);
            }
            self.s.rect(
                [x + 29., yy + 7., 30., 30.],
                9.,
                if selected { 0x85aaff33 } else { 0xffffff0d },
            );
            self.s.icon(
                &self.a.pin,
                [x + 36., yy + 13., 16., 17.],
                if selected { BLUE } else { MUTED },
            );
            self.txt(x + 72., yy + 3., 260., 13., 600, WHITE, &name);
            self.txt(x + 72., yy + 25., 260., 10., 400, MUTED, &subtitle);
        }
        self.s
            .line([x + 24., y + 355.], [x + 358., y + 355.], 1., 0xffffff18);
        self.txt(x + 25., y + 373., 280., 13., 600, WHITE, "Waterfront Walk");
        self.txt(
            x + 25.,
            y + 398.,
            280.,
            10.,
            400,
            MUTED,
            "1.8 km  ·  Marina Bay → Juniper Park",
        );
        self.s.rect([x + 25., y + 428., 332., 3.], 2., 0xffffff25);
        self.s.rect(
            [x + 25., y + 428., 332. * self.m.progress, 3.],
            2.,
            0x91b9ffff,
        );
        self.button(
            RESET,
            [x + 24., y + 448., 43., 39.],
            &self.a.reset,
            "Reset route",
        );
        self.button(
            PLAY,
            [x + 77., y + 448., 190., 39.],
            if self.m.running {
                &self.a.pause
            } else {
                &self.a.play
            },
            if self.m.running {
                "Pause route"
            } else {
                "Play route"
            },
        );
        self.text_button(
            SPEED,
            [x + 278., y + 448., 77., 39.],
            &format!("{}×", self.m.speed),
            false,
        );
        self.txt(x + 25., y + 503., 235., 12., 500, WHITE, "Precise location");
        self.txt(
            x + 25.,
            y + 529.,
            245.,
            10.,
            400,
            MUTED,
            if self.m.precise {
                "Pinpoint accuracy enabled"
            } else {
                "Approximate neighborhood"
            },
        );
        self.region(
            PRECISE,
            [x + 301., y + 503., 47., 26.],
            "Toggle precise location",
            true,
        );
        self.s.rect(
            [x + 301., y + 503., 47., 26.],
            13.,
            if self.m.precise {
                0x89afffff
            } else {
                0x756679ff
            },
        );
        self.s.ellipse(
            [
                x + 304. + if self.m.precise { 21. } else { 0. },
                y + 506.,
                20.,
                20.,
            ],
            0xffffffff,
        );
        self.text_button(
            PERMIT,
            [x + 24., y + 554., 243., 38.],
            "App permissions",
            false,
        );
        if self.m.places.len() > 3 {
            self.text_button(REMOVE, [x + 277., y + 554., 78., 38.], "Delete", false);
        }
    }
    fn device(&mut self, x: f32, y: f32) {
        self.txt(x + 25., y + 155., 330., 10., 600, MUTED, "DEVICE ACTIONS");
        self.text_button(
            ROTATE + 1000,
            [x + 24., y + 192., 330., 44.],
            "Rotate between portrait and landscape",
            false,
        );
        self.text_button(
            NOTICE,
            [x + 24., y + 250., 330., 44.],
            "Send a sample notification",
            false,
        );
        self.text_button(
            CAPTURE + 1000,
            [x + 24., y + 308., 330., 44.],
            "Capture a screenshot preview",
            false,
        );
        self.text_button(
            PERMIT,
            [x + 24., y + 366., 330., 44.],
            "Review location permission",
            false,
        );
        self.txt(
            x + 25.,
            y + 454.,
            326.,
            14.,
            600,
            WHITE,
            "A sandbox for your ideas.",
        );
        self.txt(
            x + 25.,
            y + 487.,
            326.,
            11.,
            400,
            MUTED,
            "Try a new location. Pause time. Start again.",
        );
        self.txt(
            x + 25.,
            y + 511.,
            326.,
            11.,
            400,
            MUTED,
            "Everything here stays in this workspace.",
        );
    }
    fn appearance(&mut self, x: f32, y: f32) {
        self.txt(x + 25., y + 155., 330., 10., 600, MUTED, "MAKE IT YOURS");
        self.text_button(
            THEME,
            [x + 24., y + 195., 330., 46.],
            if self.m.light {
                "Map appearance: Light"
            } else {
                "Map appearance: Dark"
            },
            false,
        );
        self.text_button(
            REDUCED,
            [x + 24., y + 260., 330., 46.],
            if self.m.reduced {
                "Reduce transparency: On"
            } else {
                "Reduce transparency: Off"
            },
            false,
        );
        self.txt(
            x + 25.,
            y + 359.,
            330.,
            14.,
            600,
            WHITE,
            "Clarity, at every scale.",
        );
        self.txt(
            x + 25.,
            y + 394.,
            330.,
            11.,
            400,
            MUTED,
            "System fonts. Crisp vectors. Room to breathe.",
        );
        self.txt(
            x + 25.,
            y + 421.,
            330.,
            11.,
            400,
            MUTED,
            "Choose a solid inspector for greater contrast.",
        );
    }
    fn paint(mut self) -> (Scene, Vec<CanvasRegion>) {
        if let Some(backdrop) = &self.a.backdrop {
            self.s.icon(backdrop, [0., 0., W as f32, H as f32], WHITE);
        } else {
            self.background();
        }
        self.toolbar();
        self.phone();
        self.inspector();
        self.button(ZOOM_IN, [617., 650., 37., 37.], &self.a.plus, "Zoom in");
        self.button(ZOOM_OUT, [617., 697., 37., 37.], &self.a.minus, "Zoom out");
        self.txt(
            238.,
            828.,
            390.,
            10.,
            500,
            0xddc6d3ff,
            "Drag to explore  ·  Click to move  ·  Scroll to zoom",
        );
        if let Some(preview) = &self.m.preview {
            self.regions.clear();
            self.s.rect([0., 0., 1280., 900.], 0., 0x070a13c9);
            self.s
                .shadow([175., 116., 930., 667.], 20., 24., 0x00000099);
            self.s.rect([175., 106., 930., 677.], 20., 0x272a38ff);
            self.txt(201., 126., 700., 17., 600, WHITE, "Screenshot preview");
            self.s.icon(preview, [194., 181., 892., 575.], WHITE);
            self.text_button(CLOSE, [969., 121., 106., 37.], "Done", true);
        }
        (self.s, self.regions)
    }
}
fn render(canvas: &Widget, surface: &Surface, model: &Model, art: &Art) -> Result<()> {
    let start=std::time::Instant::now();
    let (scene, regions) = View::new(model, art).paint();
    let scene_time=start.elapsed();
    surface.render(&scene)?;
    let draw_time=start.elapsed();
    assert!(canvas.canvas_set_surface(surface)?);
    let upload_time=start.elapsed();
    assert!(canvas.canvas_set_regions(&regions)?);
    eprintln!("PROFILE,{:.3},{:.3},{:.3},{:.3}",scene_time.as_secs_f64()*1000.,(draw_time-scene_time).as_secs_f64()*1000.,(upload_time-draw_time).as_secs_f64()*1000.,(start.elapsed()-upload_time).as_secs_f64()*1000.);
    Ok(())
}
fn smoke_controls(canvas: &Widget, model: &RefCell<Model>) {
    for id in [
        PLAY, SPEED, RESET, 101, SAVE, REMOVE, ZOOM_IN, ZOOM_OUT, ROTATE, ROTATE, PERMIT, DENY,
        PERMIT, ALLOW, 201, NOTICE, DISMISS, 202, THEME, REDUCED, 200, CAPTURE, CLOSE,
    ] {
        assert!(
            canvas.canvas_activate_region(id).unwrap(),
            "Missing active region {id}"
        );
    }
    assert!(model.borrow().permission);
    assert_eq!(model.borrow().places.len(), 3);
    assert!(model.borrow().light && model.borrow().reduced);
    assert!(model.borrow().preview.is_none());
    let mut m = model.borrow_mut();
    m.activate(PLAY);
    m.advance(4.);
    assert!(m.progress > 0.);
    m.activate(PLAY);
    let old = m.progress;
    m.advance(4.);
    assert_eq!(old, m.progress);
    assert!(canvas.set_opacity(0.8).unwrap());
    assert!(!canvas.set_opacity(f64::NAN).unwrap());
    assert!(canvas.set_opacity(1.).unwrap());
    println!("Waypoint: region activation, route, places, permissions, theme, rotation and capture passed");
}

fn main() -> Result<()> {
    let app = Rc::new(App::new()?);
    app.theme(CUI_THEME_DARK);
    let window = app.window("Waypoint - Rust simulator", W, H)?;
    let root = window.root()?;
    root.box_set_padding(0)?;
    let canvas = root.canvas()?;
    canvas.expand(true)?;
    canvas.set_min_size(900, 640)?;
    let scale = window.scale()?;
    let surface = Rc::new(Surface::new(W, H, scale)?);
    let model = Rc::new(RefCell::new(Model::new()));
    let mut artwork = Art::new()?;
    {
        let state = model.borrow();
        let mut view = View::new(&state, &artwork);
        view.background();
        surface.render(&view.s)?;
    }
    let (bytes, width, height) = surface.rgba();
    artwork.backdrop = Some(Icon::rgba(&bytes, width, height)?);
    let art = Rc::new(artwork);
    render(&canvas, &surface, &model.borrow(), &art)?;
    {
        let (m, s, a, c) = (model.clone(), surface.clone(), art.clone(), canvas.clone());
        canvas.on_canvas_event(move |e| {
            if e.kind == CUI_CANVAS_MOVE && m.borrow().drag.is_none() {
                return;
            }
            if e.kind == CUI_CANVAS_ACTIVATE && e.id % 1000 == CAPTURE {
                let (bytes, w, h) = s.rgba();
                m.borrow_mut().preview = Some(Icon::rgba(&bytes, w, h).unwrap());
            } else {
                m.borrow_mut().event(e);
            }
            render(&c, &s, &m.borrow(), &a).unwrap();
        })?;
    }
    let smoke = std::env::var_os("CUI_SMOKE_TEST").is_some();
    let capture = std::env::var_os("CUI_CAPTURE").is_some();
    let tick = Rc::new(RefCell::new(0));
    let (m, s, a, c) = (model.clone(), surface.clone(), art.clone(), canvas.clone());
    let app_weak = Rc::downgrade(&app);
    let mut last = std::time::Instant::now();
    app.every(150, move || {
        let now = std::time::Instant::now();
        let elapsed = now.duration_since(last).as_secs_f32();
        last = now;
        *tick.borrow_mut() += 1;
        if *tick.borrow() == 2 && capture {
            println!("READY");
        }
        if smoke && *tick.borrow() == 2 {
            smoke_controls(&c, &m);
            app_weak.upgrade().unwrap().quit();
            return;
        }
        if m.borrow().running {
            m.borrow_mut().advance(elapsed);
            render(&c, &s, &m.borrow(), &a).unwrap();
        }
    })?;

    window.show()?;
    app.run()
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn device_and_display_match_apple_proportions() {
        let mut model = Model::new();
        for landscape in [false, true] {
            model.landscape = landscape;
            let body = model.phone();
            let screen = model.screen();
            assert!((body[2].min(body[3]) / body[2].max(body[3]) - 71.5 / 149.6).abs() < 0.00001);
            assert!(
                (screen[2].min(screen[3]) / screen[2].max(screen[3]) - 1206. / 2622.).abs()
                    < 0.00001
            );
            assert!(screen[0] > body[0] && screen[1] > body[1]);
            assert!(
                screen[0] + screen[2] < body[0] + body[2]
                    && screen[1] + screen[3] < body[1] + body[3]
            );
        }
    }
}
