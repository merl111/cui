use super::state::State;
use cui::{sys, ChatComponent, Result};
fn activate(c: &ChatComponent, e: sys::cui_chat_event) -> Result<()> {
    c.refresh(1.)?;
    let region = c.region(&e).expect("visible native action region");
    assert!(c.part(0)?.canvas_activate_region(region)?);
    Ok(())
}
fn navigation(s: &mut State) -> Result<()> {
    s.ui.composer.part(0)?.set_text("Unsent draft")?;
    activate(
        &s.ui.nav,
        sys::cui_chat_event {
            action: sys::CUI_CHAT_OPEN_ROOM,
            id: 2,
            ..Default::default()
        },
    )?;
    s.tick()?;
    assert_eq!(s.room, 1);
    assert_eq!(s.rooms[1].nav.unread, 0);
    assert!(s.ui.composer.part(0)?.text()?.is_empty());
    activate(
        &s.ui.nav,
        sys::cui_chat_event {
            action: sys::CUI_CHAT_OPEN_ROOM,
            id: 1,
            ..Default::default()
        },
    )?;
    s.tick()?;
    assert_eq!(s.ui.composer.part(0)?.text()?, "Unsent draft");
    s.ui.composer.part(0)?.set_text("")?;
    for tab in [2, 3, 1] {
        activate(
            &s.ui.inspector,
            sys::cui_chat_event {
                action: sys::CUI_CHAT_OPEN_ROOM,
                id: tab,
                ..Default::default()
            },
        )?;
        s.tick()?;
        assert_eq!(s.tab, tab);
    }
    Ok(())
}
fn timeline(s: &mut State) -> Result<()> {
    let poll = s.rooms[0]
        .messages
        .iter()
        .find(|m| m.poll.is_some())
        .unwrap()
        .id;
    s.ui.timeline.scroll_to(poll)?;
    activate(
        &s.ui.timeline,
        sys::cui_chat_event {
            action: sys::CUI_CHAT_VOTE,
            id: poll,
            index: 1,
            ..Default::default()
        },
    )?;
    s.tick()?;
    assert_eq!(
        s.rooms[0]
            .messages
            .iter()
            .find(|m| m.id == poll)
            .unwrap()
            .poll
            .as_ref()
            .unwrap()
            .selected,
        Some(1)
    );
    let root = s.rooms[0]
        .messages
        .iter()
        .find(|m| m.thread.is_some())
        .unwrap()
        .id;
    s.ui.timeline.scroll_to(root)?;
    activate(
        &s.ui.timeline,
        sys::cui_chat_event {
            action: sys::CUI_CHAT_THREAD,
            id: root,
            ..Default::default()
        },
    )?;
    s.tick()?;
    assert!(s.thread_open);
    s.ui.thread_composer
        .part(0)?
        .set_text("A Rust thread reply")?;
    assert!(s.ui.thread_composer.submit()?);
    s.tick()?;
    assert_eq!(
        s.rooms[0]
            .messages
            .iter()
            .find(|m| m.id == root)
            .unwrap()
            .thread
            .as_ref()
            .unwrap()
            .count,
        4
    );
    s.open_room(1)?;
    assert!(s.thread_open);
    s.thread_open = false;
    s.thread()?;
    s.open_room(0)
}
pub fn run(s: &mut State) -> Result<()> {
    assert!(!s.escape.invoke()?);
    s.ui.verify_button.activate()?;
    s.tick()?;
    assert!(s.modal);
    assert!(!s.ui.verify_button.activate()?);
    s.escape.invoke()?;
    s.tick()?;
    assert!(!s.modal);
    assert!(!s.escape.invoke()?);
    s.ui.verify_button.activate()?;
    s.tick()?;
    s.ui.accept.activate()?;
    s.tick()?;
    assert!(s.verified && !s.modal);
    s.ui.verify_button.activate()?;
    s.tick()?;
    assert!(!s.modal);
    navigation(s)?;
    timeline(s)?;
    let before = s.rooms[s.room].messages.len();
    s.ui.composer
        .part(0)?
        .set_text("Rust Daylight integration")?;
    assert!(s.ui.composer.submit()?);
    s.tick()?;
    assert_eq!(s.rooms[s.room].messages.len(), before + 1);
    assert_eq!(s.ui.composer.part(0)?.text()?, "");
    println!("Rust Daylight: native navigation, drafts, inspector tabs, voting, threads, verification and send passed");
    Ok(())
}
