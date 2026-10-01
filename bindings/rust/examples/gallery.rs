use cui::{sys, App, Choice, Icon, PatternItem, Result, TreeItem};
use std::{cell::Cell, rc::Rc};
fn main() -> Result<()> {
    let app = App::new()?;
    if std::env::var_os("CUI_LARGE_TEXT").is_some() {
        app.text_scale(1.5);
    }
    let window = app.window("CUI / Rust studio", 1080, 820)?;
    window.scrollable(true)?;
    let root = window.root()?;
    root.label("A little room to create.")?
        .set_role(sys::CUI_ROLE_TITLE)?;
    root.label("Rust · Native controls · Owned assets")?
        .set_role(sys::CUI_ROLE_CAPTION)?;
    let tabs = root.tabs()?;
    tabs.expand(true)?;
    let studio = tabs.tab_add("Workspace")?;
    let columns = studio.box_layout(sys::CUI_HORIZONTAL, 20)?;
    columns.expand(true)?;
    let tree = columns.tree(&[
        TreeItem {
            id: 1,
            parent: 0,
            text: "Studio",
            expanded: true,
        },
        TreeItem {
            id: 2,
            parent: 1,
            text: "Design",
            expanded: false,
        },
        TreeItem {
            id: 3,
            parent: 1,
            text: "Engineering",
            expanded: false,
        },
    ])?;
    tree.set_min_size(200, 240)?;
    let main = columns.box_layout(sys::CUI_VERTICAL, 12)?;
    main.expand(true)?;
    let search = main.picker(sys::CUI_AUTOCOMPLETE, "Find a project…")?;
    search.picker_set_items(&[
        Choice {
            id: 1,
            label: "Orbit",
            detail: "Design system",
            keywords: "native",
            disabled: false,
        },
        Choice {
            id: 2,
            label: "Harbor",
            detail: "Desktop application",
            keywords: "rust",
            disabled: false,
        },
    ])?;
    let table = main.table(&["Project", "Status", "Owner"])?;
    table.expand(true)?;
    table.table_set_rows(&[
        &["Orbit", "In progress", "Maya"],
        &["Harbor", "Review", "Alex"],
        &["Relay", "Ready", "Sam"],
    ])?;
    let field = main.field("Project name", "Orbit", "Changes stay in this demo")?;
    let field_observer = field.clone();
    field.on_action(move |_| {
        let entry = field_observer.field_entry().unwrap();
        field_observer
            .field_set_error(if entry.text().unwrap().trim().is_empty() {
                "Give the project a name"
            } else {
                ""
            })
            .unwrap();
    })?;
    let count = Rc::new(Cell::new(0));
    let count_action = count.clone();
    let status = main.label("No changes saved")?;
    let status_action = status.clone();
    let save = main.symbol_button(sys::CUI_SYMBOL_CHECK, "Save changes")?;
    save.set_role(sys::CUI_ROLE_PRIMARY)?;
    save.on_action(move |_| {
        count_action.set(count_action.get() + 1);
        status_action
            .set_text(&format!("Saved locally · revision {}", count_action.get()))
            .unwrap();
    })?;
    let media = tabs.tab_add("Icons & data")?;
    let play = Icon::decode(include_bytes!(
        "../../../examples/assets/icons/play.cuiicon"
    ))?;
    let player = media.icon_button(Some(&play), "Play preview")?;
    player.set_icon_size(36)?;
    let pause = Icon::symbol(sys::CUI_SYMBOL_PAUSE)?;
    let playing = Rc::new(Cell::new(false));
    let state = playing.clone();
    player.on_action(move |button| {
        state.set(!state.get());
        button
            .set_icon(Some(if state.get() { &pause } else { &play }))
            .unwrap();
        button
            .set_text(if state.get() {
                "Pause preview"
            } else {
                "Play preview"
            })
            .unwrap();
    })?;
    let tasks = media.pattern_create(sys::CUI_TASK_ROWS, "Today's work")?;
    tasks.pattern_set_items(&[
        PatternItem::new(11, "Review native layouts"),
        PatternItem::new(12, "Ship the Rust example"),
    ])?;
    media.chart(&[12., 19., 16., 28., 23., 38.])?;
    if std::env::var_os("CUI_SMOKE_TEST").is_some() {
        assert!(save.activate()?);
        assert_eq!(count.get(), 1);
        assert!(status.text()?.contains("revision 1"));
        assert!(tree.tree_select(3)?);
        assert_eq!(tree.tree_selected()?, 3);
        assert!(search.picker_select(2)?);
        assert_eq!(search.picker_selected()?, 2);
        assert!(table.table_sort(0, false, false)?);
        assert_eq!(table.table_cell(0, 0)?, "Harbor");
        assert_eq!(tasks.pattern_item_count()?, 2);
        tabs.set_selected(1)?;
        assert!(player.activate()?);
        assert!(playing.get());
        assert_eq!(player.text()?, "Pause preview");
        assert!(player.activate()?);
        assert!(!playing.get());
        tabs.set_selected(0)?;
        let quit = root.clone();
        app.every(100, move || {
            quit.quit().unwrap();
        })?;
    }
    window.show()?;
    app.run()
}
