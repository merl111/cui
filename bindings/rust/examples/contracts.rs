//! Native FFI/lifetime/callback regression, run as a process to own the main thread.
use cui::{
    sys, App, Choice, Error, FileFilter, FileOptions, Font, Icon, InsightSeries, PatternItem,
    RecordFilter, Result, TreeItem,
};
use std::{cell::Cell, rc::Rc};
fn main() -> Result<()> {
    let app = App::new()?;
    if std::env::var_os("CUI_LARGE_TEXT").is_some() {
        app.text_scale(1.5);
    }
    let window = app.window("Rust binding contracts", 900, 750)?;
    window.scrollable(true)?;
    let root = window.root()?;
    let button = root.button("Action")?;
    assert!(matches!(
        button.set_text("bad\0text"),
        Err(Error::InvalidInput(_))
    ));
    let count = Rc::new(Cell::new(0));
    let observed = count.clone();
    button.on_action(move |_| observed.set(observed.get() + 1))?;
    button.set_text("世界 — Grüße")?;
    assert_eq!(button.text()?, "世界 — Grüße");
    assert_eq!(count.get(), 0);
    assert!(button.activate()?);
    assert_eq!(count.get(), 1);
    button.set_enabled(false)?;
    assert!(!button.activate()?);
    button.set_enabled(true)?;
    button.clear_action()?;
    button.activate()?;
    assert_eq!(count.get(), 1);
    let icon = Icon::symbol(sys::CUI_SYMBOL_PLAY)?;
    button.set_icon(Some(&icon))?;
    drop(icon);
    let saved = button.get_icon()?.unwrap();
    button.set_icon(None)?;
    button.set_icon(Some(&saved))?;
    assert!(button.set_icon_only(true)?);
    let vector = Icon::vector(
        24.,
        24.,
        &[
            sys::cui_icon_command {
                op: sys::CUI_ICON_MOVE,
                values: [0.; 6],
                ..Default::default()
            },
            sys::cui_icon_command {
                op: sys::CUI_ICON_LINE,
                values: [24., 24., 0., 0., 0., 0.],
                ..Default::default()
            },
            sys::cui_icon_command {
                op: sys::CUI_ICON_STROKE,
                values: [2., 1., 1., 0., 0., 0.],
                current_color: 1,
                ..Default::default()
            },
        ],
    )?;
    assert!(root.icon(Some(&vector))?.set_icon_size(48)?);
    assert!(Icon::rgba(&[1, 2, 3], 1, 1).is_err());
    assert!(Icon::decode(b"invalid").is_err());
    let table = root.table(&["Name", "Number"])?;
    assert!(table.table_set_rows(&[&["one"]]).is_err());
    assert!(table.table_set_rows(&[&["Zulu", "2"], &["Alpha", "10"]])?);
    assert!(table.table_sort(0, false, false)?);
    assert_eq!(table.table_source_row(0)?, 1);
    assert_eq!(table.table_cell(0, 0)?, "Alpha");
    table.table_set_multiple(true)?;
    table.table_select_row(0, 1)?;
    assert_eq!(table.table_selected_rows()?, vec![0]);
    let nodes = [
        TreeItem {
            id: 1,
            parent: 0,
            text: "Root",
            expanded: false,
        },
        TreeItem {
            id: 2,
            parent: 1,
            text: "Child",
            expanded: false,
        },
    ];
    let tree = root.tree(&nodes)?;
    assert!(tree.tree_select(2)?);
    assert_eq!(tree.tree_selected()?, 2);
    let options = [Choice {
        id: 11,
        label: "Rust",
        detail: "Systems",
        keywords: "native",
        disabled: false,
    }];
    let tokens = root.tokens("Language", 4)?;
    assert!(tokens.tokens_set_items(&options)?);
    assert!(tokens.tokens_set_selected(&[11])?);
    assert_eq!(tokens.tokens_selected()?, vec![11]);
    let chat = root.pattern_create(sys::CUI_CHAT, "Chat")?;
    assert!(chat.pattern_set_items(&[PatternItem::new(42, "Maya")])?);
    let owned = chat.pattern_item_at(0)?.unwrap();
    chat.pattern_remove_item(42)?;
    assert_eq!(owned.title, "Maya");
    let records = root.pattern_create(sys::CUI_FILTER_TABLE, "Records")?;
    assert!(records.pattern_set_records(&[["Alpha", "Active", "10"], ["Beta", "Paused", "20"]])?);
    assert!(records.pattern_set_filters(
        &[RecordFilter {
            column: 0,
            operation: sys::CUI_FILTER_EQUALS,
            value: "Alpha"
        }],
        true
    )?);
    let insights = root.pattern_create(sys::CUI_INSIGHTS, "Signals")?;
    assert!(insights.insights_set_series(&[InsightSeries {
        id: 1,
        title: "Daily",
        detail: "",
        values: &[1., 3.],
        labels: Some(&["Mon", "Tue"])
    }])?);
    assert!(insights.insights_select(1, 1)?);
    assert_eq!(insights.insights_selection()?, Some((1, 1, 3.)));
    assert!(button.font_apply(&Font {
        family: "Sans".into(),
        points: 12.,
        weight: 500,
        italic: false
    })?);
    let command_count = count.clone();
    let command = app.command(
        "Save",
        b'S' as u32,
        sys::CUI_MOD_PRIMARY as u32,
        move || command_count.set(command_count.get() + 1),
    )?;
    let menu = app.menu()?;
    assert!(menu.add(&command)?);
    assert!(command.invoke()?);
    assert_eq!(count.get(), 2);
    let _toolbar = root.toolbar(&[command])?;
    let dialog = window.alert("Test", "Local dialog", "Okay", |_, _, _| {})?;
    dialog.cancel()?;
    assert_eq!(dialog.path_count()?, 0);
    let cancelled = Rc::new(Cell::new(false));
    let cancel_result = cancelled.clone();
    let files = window.file_dialog_with_options(
        sys::CUI_DIALOG_OPEN,
        "Documents",
        &FileOptions {
            filters: vec![
                FileFilter {
                    name: "Documents",
                    extensions: &["txt", "md"],
                },
                FileFilter {
                    name: "All files",
                    extensions: &[],
                },
            ],
            multiple: true,
            ..Default::default()
        },
        move |dialog, result, path| {
            assert_eq!(result, sys::CUI_DIALOG_CANCELLED);
            assert!(path.is_empty());
            assert!(dialog.paths().unwrap().is_empty());
            assert_eq!(dialog.filter().unwrap(), None);
            cancel_result.set(true);
        },
    )?;
    assert_eq!(files.path(0)?, None);
    files.cancel()?;
    // Callback failure is reported in Rust; no unwind crosses C.
    button.on_action(|_| panic!("expected callback failure"))?;
    window.show()?;
    let action = button.clone();
    app.every(100, move || {
        action.activate().unwrap();
    })?;
    assert!(matches!(app.run(), Err(Error::CallbackPanic(_))));
    assert!(cancelled.get());
    drop(app);
    assert_eq!(button.text().unwrap_err(), Error::Closed);
    assert!(dialog.cancel().is_err());
    drop(saved);
    drop(vector);
    println!("Rust binding contracts passed");
    Ok(())
}
