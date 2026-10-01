use super::ui::{button, label, style, Intent, Queue};
use cui::{chat::Theme, sys, Result, Widget};

fn centered(parent: &Widget, value: &str, size: f64, weight: i32) -> Result<()> {
    let row = parent.box_layout(sys::CUI_HORIZONTAL, 0)?;
    row.box_layout(sys::CUI_HORIZONTAL, 0)?.expand(true)?;
    label(&row, value, size, weight)?;
    row.box_layout(sys::CUI_HORIZONTAL, 0)?.expand(true)
}
pub fn verification(
    layers: &Widget,
    t: &Theme,
    queue: &Queue,
) -> Result<(Widget, Widget, Widget, Widget)> {
    let scrim = layers.stack_layer(sys::CUI_LAYER_FILL, 0, 0, 0)?;
    style(&scrim, t, cui::chat_color(0.25, 0.04, 250., 0.35), 0., 0)?;
    let canvas = scrim.canvas()?;
    canvas.expand(true)?;
    let q = queue.clone();
    canvas.on_canvas_event(move |e| {
        if e.kind == sys::CUI_CANVAS_ACTIVATE {
            q.borrow_mut().push(Intent::Dismiss);
        }
    })?;
    scrim.set_visible(false)?;
    let modal = layers.stack_layer(sys::CUI_LAYER_CENTER, 520, 0, 16)?;
    style(&modal, t, t.surface, 24., 28)?;
    let content = modal.box_layout(sys::CUI_VERTICAL, 6)?;
    label(&content, "Compare emoji", 22., 800)?;
    label(&content, "Confirm the emoji below appear in the same order on your\nother device, signed in as @mathias:lumen.chat.",14.,400)?;
    modal
        .box_layout(sys::CUI_VERTICAL, 0)?
        .set_min_size(1, 18)?;
    let grid = modal.grid(7, 6)?;
    let glyphs = ["🐶", "🔑", "🌵", "🎸", "🚀", "🍄", "⚓"];
    let words = [
        "Dog", "Key", "Cactus", "Guitar", "Rocket", "Mushroom", "Anchor",
    ];
    for i in 0..7 {
        let cell = grid.grid_cell(0, i as u32, 1, 1)?;
        style(&cell, t, t.selected, 14., 2)?;
        let inside = cell.box_layout(sys::CUI_VERTICAL, 0)?;
        inside
            .box_layout(sys::CUI_VERTICAL, 0)?
            .set_min_size(1, 8)?;
        centered(&inside, glyphs[i], 26., 400)?;
        centered(&inside, words[i], 10.5, 600)?;
        inside
            .box_layout(sys::CUI_VERTICAL, 0)?
            .set_min_size(1, 8)?;
    }
    modal
        .box_layout(sys::CUI_VERTICAL, 0)?
        .set_min_size(1, 20)?;
    let actions = modal.box_layout(sys::CUI_HORIZONTAL, 8)?;
    actions.box_layout(sys::CUI_HORIZONTAL, 0)?.expand(true)?;
    button(
        &actions,
        "They don’t match",
        t,
        queue,
        Intent::Answer(false),
    )?;
    let accept = button(&actions, "They match", t, queue, Intent::Answer(true))?;
    accept.set_style(Some(&sys::cui_widget_style {
        background: t.foreground,
        foreground: t.surface,
        radius: 13.,
        padding: 12,
        ..Default::default()
    }))?;
    modal.set_visible(false)?;
    Ok((scrim, canvas, modal, accept))
}
