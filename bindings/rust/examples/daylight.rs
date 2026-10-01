//! Daylight reference composition, using the shared C widgets through Rust.
//! Application state and event handling are Rust; layout and rendering are C.
#[path = "daylight/checks.rs"]
mod checks;
#[path = "daylight/fixture.rs"]
mod fixture;
#[path = "daylight/model.rs"]
mod model;
#[path = "daylight/overlays.rs"]
mod overlays;
#[path = "daylight/state.rs"]
mod state;
#[path = "daylight/ui.rs"]
mod ui;
use cui::{sys, App, Result};
use std::{cell::RefCell, rc::Rc};

fn dimension(name: &str, fallback: i32) -> i32 {
    std::env::var(name)
        .ok()
        .and_then(|s| s.parse().ok())
        .filter(|n| *n > 0)
        .unwrap_or(fallback)
}
fn main() -> Result<()> {
    let app = Rc::new(App::new()?);
    app.theme(sys::CUI_THEME_LIGHT);
    let ui = ui::Ui::new(
        &app,
        dimension("CUI_CAPTURE_WIDTH", 1440),
        dimension("CUI_CAPTURE_HEIGHT", 900),
    )?;
    let queue = ui.queue.clone();
    let escape = app.command(
        "Dismiss verification",
        sys::CUI_KEY_ESCAPE as u32,
        0,
        move || queue.borrow_mut().push(ui::Intent::Dismiss),
    )?;
    let state = Rc::new(RefCell::new(state::State::new(ui, escape)?));
    state.borrow().ui.window.show()?;
    state.borrow().ui.composer.part(0)?.focus()?;
    let app_tick = Rc::downgrade(&app);
    let state_tick = state.clone();
    let mut ticks = 0;
    let _timer = app.every(100, move || {
        ticks += 1;
        let mut state = state_tick.borrow_mut();
        state.tick().expect("Daylight update");
        if ticks == 3 {
            match std::env::var("CUI_CAPTURE_STATE").as_deref() {
                Ok("verification") => state.verify(true).expect("verification"),
                Ok("people") => {
                    state.tab = 2;
                    state.inspector().expect("people");
                }
                Ok("media") => {
                    state.tab = 3;
                    state.inspector().expect("media");
                }
                Ok("thread") => {
                    state.thread_open = true;
                    state.thread().expect("thread");
                }
                _ => {}
            }
        }
        if ticks == 20 {
            if std::env::var_os("CUI_SMOKE_TEST").is_some() {
                checks::run(&mut state).expect("Daylight interaction checks");
                if let Some(app) = app_tick.upgrade() {
                    app.quit();
                }
            } else if std::env::var_os("CUI_CAPTURE").is_some() {
                println!("READY");
            }
        }
    })?;
    app.run()
}
