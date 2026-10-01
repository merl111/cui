use cui::{sys, App, Result};
fn main() -> Result<()> {
    let app = App::new()?;
    if std::env::var_os("CUI_LARGE_TEXT").is_some() {
        app.text_scale(1.5);
    }
    let window = app.window("Hello from Rust", 480, 280)?;
    let root = window.root()?;
    root.label("Native CUI. Written in Rust.")?
        .set_role(sys::CUI_ROLE_HEADING)?;
    let status = root.label("Ready")?;
    let button = root.symbol_button(sys::CUI_SYMBOL_PLAY, "Say hello")?;
    button.on_action(move |_| {
        status.set_text("Hello, Rust!").unwrap();
    })?;
    if std::env::var_os("CUI_SMOKE_TEST").is_some() {
        assert!(button.activate()?);
        let quit = root.clone();
        app.every(100, move || {
            quit.quit().unwrap();
        })?;
    }
    window.show()?;
    app.run()
}
