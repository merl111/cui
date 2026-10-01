//! Native composition example for the three Archaic design directions.
//! Run with nebula/daylight/tiles. CUI_SMOKE_TEST exercises the public contracts.
use cui::{chat::*, sys, App, Result, Widget, Window};
use std::{cell::RefCell, rc::Rc};

fn avatar(name: &str, color: u32) -> Avatar {
    Avatar {
        name: name.into(),
        color,
        online: true,
        square: false,
    }
}
fn history() -> Vec<Message> {
    let ada = avatar("Ada Chen", 0x669dc9ff);
    vec![
        Message{id:1,author:ada.clone(),time:"10:42".into(),date:"Today · October 1".into(),body:"The three directions are ready. Each keeps the same conversations, threads and room tools.".into(),reactions:vec![Reaction{key:"✨".into(),count:4,mine:true}],..Message::default()},
        Message{id:2,author:ada.clone(),time:"10:43".into(),body:RichText(vec![Span{ text:"@Mathias ".into(),style:TextStyle::Mention,link:None},Span::plain("here is the latest handoff. 日本語 and café wrap using native font measurements.")]),attachments:vec![Attachment{id:1,name:"Archaic-designs.pdf".into(),detail:"PDF · 2.4 MB · Download".into()}],thread:Some(ThreadSummary{count:8,preview:"Review the room header".into(),participants:vec![ada.clone(),avatar("Sam",0xc29cd8ff)]}),..Message::default()},
        Message{id:3,author:avatar("You",0xb59bdbff),time:"10:46".into(),outgoing:true,body:"I like the quieter surfaces. Let's keep drafts and reading position when switching layouts.".into(),reply:Some(Reply{message:1,author:"Ada".into(),text:"The three directions are ready".into()}),..Message::default()},
        Message{id:4,author:avatar("Sam Rivera",0xd8b777ff),time:"10:48".into(),body:"Which layout should we use for the review?".into(),poll:Some(Poll{question:"Choose a workspace".into(),options:vec![PollOption{label:"One conversation".into(),votes:3},PollOption{label:"Main + two".into(),votes:5},PollOption{label:"Four tiles".into(),votes:2}],selected:None,closed:false}),..Message::default()},
    ]
}
struct Demo {
    window: Window,
    nav: Navigation,
    workspace: Workspace,
    timelines: Vec<Timeline>,
    composers: Vec<Composer>,
    headers: Vec<Widget>,
    notice: Widget,
    thread: Panel,
    thread_history: Timeline,
    thread_composer: Composer,
}
impl Demo {
    fn open_room(&self, id: u64, new_pane: bool) -> Result<()> {
        let pane = if new_pane {
            let visible = self.workspace.visible_panes();
            let i = (0..4)
                .find(|i| !visible.contains(i))
                .unwrap_or(self.workspace.focused());
            self.workspace.set_layout(match i {
                0 => Layout::Single,
                1 => Layout::Split,
                2 => Layout::MainTwo,
                _ => Layout::Quad,
            })?;
            i
        } else {
            self.workspace.focused()
        };
        self.workspace.set_focus(pane)?;
        self.nav.select(Some(id))?;
        self.headers[pane].set_text(&format!("# {}", room_title(id)))?;
        self.timelines[pane].set_messages(history())?;
        self.notice
            .set_text("Room opened · each pane retains its own draft")
    }
    fn action(&self, index: usize, action: Action) -> Result<()> {
        let timeline = &self.timelines[index];
        match action {
            Action::Focus => self.workspace.set_focus(index)?,
            Action::Reply(id) => {
                if let Some(m) = timeline.messages().iter().find(|m| m.id == id) {
                    self.composers[index].set_mode(ComposeMode::Reply {
                        message: id,
                        author: m.author.name.clone(),
                        preview: m.body.plain(),
                    })?;
                    self.composers[index].input.focus()?;
                }
            }
            Action::Copy(id) => {
                if let Some(m) = timeline.messages().iter().find(|m| m.id == id) {
                    self.window.clipboard_text(&m.body.plain())?;
                    self.notice.set_text("Message copied")?;
                }
            }
            Action::React { message, key } => {
                let mut items = timeline.messages();
                if let Some(m) = items.iter_mut().find(|m| m.id == message) {
                    if let Some(r) = m.reactions.iter_mut().find(|r| r.key == key) {
                        r.mine = !r.mine;
                        r.count = if r.mine {
                            r.count + 1
                        } else {
                            r.count.saturating_sub(1)
                        };
                    } else {
                        m.reactions.push(Reaction {
                            key,
                            count: 1,
                            mine: true,
                        });
                    }
                }
                timeline.set_messages(items)?;
            }
            Action::Vote { message, option } => {
                let mut items = timeline.messages();
                if let Some(p) = items
                    .iter_mut()
                    .find(|m| m.id == message)
                    .and_then(|m| m.poll.as_mut())
                {
                    if !p.closed && option < p.options.len() {
                        if let Some(old) = p.selected {
                            p.options[old].votes = p.options[old].votes.saturating_sub(1);
                        }
                        p.options[option].votes += 1;
                        p.selected = Some(option);
                    }
                }
                timeline.set_messages(items)?;
            }
            Action::Thread(id) => {
                self.thread_history.set_messages(
                    timeline
                        .messages()
                        .into_iter()
                        .filter(|m| m.id == id)
                        .collect(),
                )?;
                self.thread.show(
                    &self.window,
                    None,
                    Some(self.composers[index].input.clone()),
                )?;
            }
            Action::More(id) => {
                self.composers[index].set_mode(ComposeMode::Edit { message: id })?;
                if let Some(m) = timeline.messages().iter().find(|m| m.id == id) {
                    self.composers[index].set_text(&m.body.plain())?;
                }
            }
            Action::LoadOlder => self
                .notice
                .set_text("LoadOlder event: connect to the application's history provider")?,
            other => self
                .notice
                .set_text(&format!("Application event: {other:?}"))?,
        }
        Ok(())
    }
    fn compose(&self, timeline: &Timeline, composer: &Composer) -> Result<()> {
        for event in composer.drain_events() {
            match event {
                ComposeEvent::Send {
                    text,
                    mode,
                    attachments,
                } => {
                    let mut items = timeline.messages();
                    if let ComposeMode::Edit { message } = &mode {
                        if let Some(m) = items.iter_mut().find(|m| m.id == *message) {
                            m.body = text.into();
                        }
                    } else {
                        let reply = if let ComposeMode::Reply {
                            message,
                            author,
                            preview,
                        } = mode
                        {
                            Some(Reply {
                                message,
                                author,
                                text: preview,
                            })
                        } else {
                            None
                        };
                        items.push(Message {
                            id: items.iter().map(|m| m.id).max().unwrap_or(0) + 1,
                            author: avatar("You", 0xb59bdbff),
                            time: "now".into(),
                            body: text.into(),
                            outgoing: true,
                            reply,
                            attachments: attachments
                                .into_iter()
                                .enumerate()
                                .map(|(i, name)| Attachment {
                                    id: i as u64 + 1,
                                    name,
                                    detail: "Local demonstration attachment".into(),
                                })
                                .collect(),
                            ..Message::default()
                        });
                    }
                    timeline.set_messages(items)?;
                    timeline.scroll_to_end();
                    composer.clear()?;
                }
                ComposeEvent::Emoji => composer.set_text(&format!("{} ✨", composer.text()?))?,
                ComposeEvent::Attach => {
                    composer.set_attachments(vec!["Design notes.txt".into()])?
                }
                ComposeEvent::Poll => {
                    let mut items = timeline.messages();
                    let mut poll = history().pop().unwrap();
                    poll.id = items.iter().map(|m| m.id).max().unwrap_or(0) + 1;
                    items.push(poll);
                    timeline.set_messages(items)?;
                }
                _ => (),
            }
        }
        if let Some(error) = composer.take_error() {
            return Err(error);
        }
        Ok(())
    }
    fn refresh(&self) -> Result<()> {
        let scale = self.window.scale()?;
        for i in self.workspace.visible_panes() {
            if i != self.workspace.focused()
                && (self.composers[i].input.has_focus()?
                    || self.timelines[i].widget().has_focus()?)
            {
                self.workspace.set_focus(i)?;
            }
        }
        self.nav.refresh(scale)?;
        for action in self.nav.drain_events() {
            if let Action::OpenRoom { id, new_pane } = action {
                self.open_room(id, new_pane)?;
            }
        }
        for i in self.workspace.visible_panes() {
            for action in self.timelines[i].drain_events() {
                self.action(i, action)?;
            }
            self.compose(&self.timelines[i], &self.composers[i])?;
            self.composers[i].update()?;
            self.timelines[i].refresh(scale)?;
        }
        if self.thread.window.is_visible()? {
            self.compose(&self.thread_history, &self.thread_composer)?;
            self.thread_history.refresh(scale)?;
        }
        Ok(())
    }
}
fn room_title(id: u64) -> &'static str {
    match id {
        1 => "design-system",
        2 => "general",
        3 => "release-planning",
        _ => "Ada Chen",
    }
}
fn auxiliary(app: &App, parent: &Window, toolbar: &Widget) -> Result<()> {
    let info = Panel::new(app, "Room inspector", 380, 500)?;
    info.content
        .label("# design-system")?
        .set_role(sys::CUI_ROLE_HEADING)?;
    info.content
        .label("Design files, members and room information")?;
    let members = Navigation::new(&info.content, Theme::new(Appearance::Daylight))?;
    members.set_items(vec![
        NavItem {
            id: 1,
            title: "Ada Chen".into(),
            detail: "Designer · online".into(),
            avatar: avatar("Ada", 0x669dc9ff),
            ..NavItem::default()
        },
        NavItem {
            id: 2,
            title: "Sam Rivera".into(),
            detail: "Member · online".into(),
            avatar: avatar("Sam", 0xd8b777ff),
            ..NavItem::default()
        },
    ])?;
    let close = info.clone();
    info.content
        .button("Close")?
        .on_action(move |_| close.close().unwrap())?;
    let panel = info.clone();
    let window = parent.clone();
    toolbar
        .button("Members / info")?
        .on_action(move |_| panel.show(&window, None, None).unwrap())?;
    app.every(80, move || {
        if info.window.is_visible().unwrap() {
            members.refresh(info.window.scale().unwrap()).unwrap();
        }
    })?;
    let verify = Panel::new(app, "Verify session", 460, 260)?;
    verify
        .content
        .label("Compare these symbols on both devices")?
        .set_role(sys::CUI_ROLE_HEADING)?;
    verify.content.label("☀  ☂  ♟  ♫  ✈  ⚓  ★")?;
    verify
        .content
        .label("Example UI only. The application supplies the verification data.")?;
    let result = verify.content.label("Waiting for your comparison")?;
    let r = result.clone();
    verify
        .content
        .button("They match")?
        .on_action(move |_| r.set_text("Match action received by the demo").unwrap())?;
    let v = verify.clone();
    verify
        .content
        .button("Cancel / close")?
        .on_action(move |_| v.close().unwrap())?;
    let window = parent.clone();
    toolbar
        .button("Verify")?
        .on_action(move |_| verify.show(&window, None, None).unwrap())?;
    let call = Panel::new(app, "Call controls", 360, 250)?;
    call.content
        .label("Design review · call preview")?
        .set_role(sys::CUI_ROLE_HEADING)?;
    call.content.label("Ada Chen · Sam Rivera · You")?;
    let muted = Rc::new(RefCell::new(false));
    call.content.button("Mute")?.on_action(move |w| {
        let mut m = muted.borrow_mut();
        *m = !*m;
        w.set_text(if *m { "Unmute" } else { "Mute" }).unwrap();
    })?;
    let c = call.clone();
    call.content
        .button("End call preview")?
        .on_action(move |_| c.close().unwrap())?;
    let window = parent.clone();
    toolbar
        .button("Call preview")?
        .on_action(move |_| call.show(&window, None, None).unwrap())?;
    Ok(())
}
fn main() -> Result<()> {
    let app = App::new()?;
    let appearance = match std::env::args().nth(1).as_deref() {
        Some("daylight") => Appearance::Daylight,
        Some("tiles") => Appearance::Tiles,
        _ => Appearance::Nebula,
    };
    app.theme(if appearance == Appearance::Nebula {
        sys::CUI_THEME_DARK
    } else {
        sys::CUI_THEME_LIGHT
    });
    let mut theme = Theme::new(appearance);
    if std::env::var_os("CUI_LARGE_TEXT").is_some() {
        app.text_scale(1.5);
        theme.font_size = 21.;
    }
    let window = app.window("CUI Chat Components", 1440, 1000)?;
    let root = window.root()?;
    root.box_set_padding(10)?;
    let toolbar = root.box_layout(sys::CUI_HORIZONTAL, 8)?;
    toolbar
        .label(&format!("ARCHAIC · {appearance:?}"))?
        .set_role(sys::CUI_ROLE_HEADING)?;
    let palette = Palette::new(&app)?;
    palette.set_items(
        &(1..=4)
            .map(|id| PaletteItem {
                id,
                group: "Rooms".into(),
                title: room_title(id).into(),
                detail: "Open conversation".into(),
                keywords: "chat design".into(),
                disabled: false,
            })
            .collect::<Vec<_>>(),
    )?;
    let p = palette.clone();
    let w = window.clone();
    toolbar
        .button("Search · Ctrl K")?
        .on_action(move |_| p.open(&w, None).unwrap())?;
    let p = palette.clone();
    let w = window.clone();
    app.command(
        "Search rooms",
        'k' as u32,
        sys::CUI_MOD_PRIMARY as u32,
        move || p.open(&w, None).unwrap(),
    )?;
    auxiliary(&app, &window, &toolbar)?;
    let columns = root.split(sys::CUI_HORIZONTAL, 0.22)?;
    columns.expand(true)?;
    let sidebar = columns.split_pane(0)?;
    sidebar.box_set_padding(6)?;
    let spaces = sidebar.box_layout(sys::CUI_HORIZONTAL, 6)?;
    let space_buttons = [
        spaces.button("All")?,
        spaces.button("Design")?,
        spaces.button("People")?,
    ];
    let search = sidebar.search("Filter rooms")?;
    let nav = Navigation::new(&sidebar, theme.clone())?;
    nav.set_items(
        (1..=4)
            .map(|id| NavItem {
                active: false,
                id,
                group: if id < 4 { "Rooms" } else { "Direct messages" }.into(),
                title: room_title(id).into(),
                detail: if id == 1 {
                    "Ada: the handoff is ready"
                } else {
                    "Last message preview"
                }
                .into(),
                avatar: avatar(room_title(id), 0x769bb8ff),
                unread: if id == 1 { 3 } else { 0 },
                mention: id == 1,
                trailing: "10:48".into(),
                symbol: sys::CUI_SYMBOL_NONE,
                disabled: false,
            })
            .collect(),
    )?;
    nav.select(Some(1))?;
    for (button, query) in space_buttons.into_iter().zip(["", "design", "Ada"]) {
        let n = nav.clone();
        button.on_action(move |_| n.set_query(query).unwrap())?;
    }
    let n = nav.clone();
    search.on_action(move |w| n.set_query(&w.text().unwrap()).unwrap())?;
    let workspace = Workspace::new(&columns.split_pane(1)?)?;
    let mut timelines = Vec::new();
    let mut composers = Vec::new();
    let mut headers = Vec::new();
    for i in 0..4 {
        let pane = workspace.pane(i)?;
        let header = pane.box_layout(sys::CUI_HORIZONTAL, 5)?;
        headers.push(header.label(&format!("# {}", room_title(i as u64 + 1)))?);
        let ws = workspace.clone();
        header
            .button("Focus")?
            .on_action(move |_| ws.set_focus(i).unwrap())?;
        let ws = workspace.clone();
        header
            .button("Max")?
            .on_action(move |_| ws.maximize(i).unwrap())?;
        let ws = workspace.clone();
        header.button("×")?.on_action(move |_| {
            let _ = ws.close_pane(i);
        })?;
        let timeline = Timeline::new(&pane, theme.clone())?;
        timeline.set_messages(history())?;
        timeline.set_status("Ada is typing… · encrypted room")?;
        timelines.push(timeline);
        let composer = Composer::new(&pane, theme.clone())?;
        let ws = workspace.clone();
        let input = composer.input.clone();
        app.command(
            &format!("Focus pane {}", i + 1),
            ('1' as u32) + i as u32,
            sys::CUI_MOD_ALT as u32,
            move || {
                if ws.set_focus(i).is_ok() {
                    input.focus().unwrap();
                }
            },
        )?;
        composers.push(composer);
    }
    workspace.set_layout(if appearance == Appearance::Tiles {
        Layout::MainTwo
    } else {
        Layout::Single
    })?;
    let layouts = root.box_layout(sys::CUI_HORIZONTAL, 8)?;
    for (label, layout) in [
        ("Single", Layout::Single),
        ("Split", Layout::Split),
        ("Main + two", Layout::MainTwo),
        ("Quad", Layout::Quad),
    ] {
        let ws = workspace.clone();
        layouts
            .button(label)?
            .on_action(move |_| ws.set_layout(layout).unwrap())?;
    }
    let ws = workspace.clone();
    layouts
        .button("Restore")?
        .on_action(move |_| ws.restore().unwrap())?;
    let notice = root.label("Shift-click a room to open another pane · Alt 1–4 focuses panes")?;
    let thread = Panel::new(&app, "Conversation thread", 520, 820)?;
    thread
        .content
        .label("Thread")?
        .set_role(sys::CUI_ROLE_HEADING)?;
    let thread_history = Timeline::new(&thread.content, theme.clone())?;
    let thread_composer = Composer::new(&thread.content, theme)?;
    let t = thread.clone();
    thread
        .content
        .button("Close thread")?
        .on_action(move |_| t.close().unwrap())?;
    let demo = Rc::new(Demo {
        window: window.clone(),
        nav,
        workspace,
        timelines,
        composers,
        headers,
        notice,
        thread,
        thread_history,
        thread_composer,
    });
    let d = demo.clone();
    let p = palette.clone();
    palette
        .picker
        .on_action(move |w| match w.picker_last_event().unwrap() {
            sys::CUI_PICKER_SELECT => {
                d.open_room(w.picker_selected().unwrap(), false).unwrap();
                p.close().unwrap();
            }
            sys::CUI_PICKER_CANCEL => p.close().unwrap(),
            _ => (),
        })?;
    let mut ticks = 0;
    let smoke = std::env::var_os("CUI_SMOKE_TEST").is_some();
    app.every(80, move || {
        demo.refresh().unwrap();
        ticks += 1;
        if ticks == 2 {
            columns.split_set_position(0.22).unwrap();
        }
        if ticks == 5 {
            if smoke {
                contracts(&demo, &palette).unwrap();
                println!("Chat component contracts passed");
                demo.window.root().unwrap().quit().unwrap();
            } else {
                println!("READY");
            }
        }
    })?;
    window.show()?;
    app.run()
}
fn contracts(d: &Demo, palette: &Palette) -> Result<()> {
    let stack=d.window.root()?.stack()?;
    let _base=stack.stack_layer(sys::CUI_LAYER_FILL,0,0,0)?;
    let _backdrop=stack.stack_backdrop("Dismiss")?;
    assert!(!d.window.popup_at(&stack,[0.,0.,0.,20.])?);
    assert!(!d.window.popup_region(&stack,0)?);
    let overlay=stack.stack_layer(sys::CUI_LAYER_CENTER,220,100,12)?;
    assert!(overlay.button("Continue")?.set_icon_trailing(true)?);
    stack.set_visible(false)?;
    use cui::{chat_presentation, ChatCommand, ChatComponent, ChatKind};
    let configurable = ChatComponent::create(
        &d.window.root()?,
        ChatKind::Inspector,
        &Theme::new(Appearance::Nebula),
    )?;
    let mut p = chat_presentation(Appearance::Daylight)?;
    p.rooms = sys::CUI_CHAT_COMPACT;
    p.room_height = 37.0;
    configurable.set_presentation(&p)?;
    configurable.set_theme(&Theme::new(Appearance::Nebula))?;
    assert_eq!(configurable.presentation()?.messages, sys::CUI_CHAT_SOFT);
    assert_eq!(configurable.presentation()?.room_height, 37.0);
    configurable.set_commands(&[ChatCommand {
        id: 401,
        label: "Participants".into(),
        action: sys::CUI_CHAT_OPEN_ROOM,
        ..Default::default()
    }])?;
    configurable.select(401)?;
    assert!(configurable.select(99).is_err());
    configurable.root.set_visible(false)?;
    for kind in [
        ChatKind::Rooms,
        ChatKind::Timeline,
        ChatKind::Composer,
        ChatKind::Workspace,
        ChatKind::Header,
        ChatKind::Spaces,
        ChatKind::Inspector,
        ChatKind::Message,
        ChatKind::AttachmentCard,
        ChatKind::ReactionStrip,
        ChatKind::PollCard,
        ChatKind::ReplyPreview,
        ChatKind::ThreadSummary,
        ChatKind::Avatar,
    ] {
        let component =
            ChatComponent::create(&d.window.root()?, kind, &Theme::new(Appearance::Daylight))?;
        if matches!(
            kind,
            ChatKind::Message
                | ChatKind::AttachmentCard
                | ChatKind::ReactionStrip
                | ChatKind::PollCard
                | ChatKind::ReplyPreview
                | ChatKind::ThreadSummary
        ) {
            component.messages(&[Message {
                id: 1,
                body: "Reusable C component".into(),
                author_color: 0x185864ff,
                thread: Some(ThreadSummary {
                    count: 3,
                    preview: "Latest reply".into(),
                    participants: vec![avatar("Kai", 0x29cbbfff)],
                }),
                ..Message::default()
            }])?;
        }
        component.refresh(1.)?;
        component.root.set_visible(false)?;
    }
    let t = &d.timelines[0];
    let c = &d.composers[0];
    c.set_text("Persistent draft 日本語")?;
    d.workspace.set_layout(Layout::Quad)?;
    d.workspace.close_pane(1)?;
    assert_eq!(d.workspace.visible_panes(), vec![0, 2, 3]);
    d.workspace.maximize(2)?;
    assert_eq!(d.workspace.visible_panes(), vec![2]);
    d.workspace.restore()?;
    assert_eq!(d.workspace.visible_panes(), vec![0, 2, 3]);
    d.workspace.set_layout(Layout::Single)?;
    assert_eq!(c.text()?, "Persistent draft 日本語");
    c.set_busy(true)?;
    c.submit()?;
    assert!(!c
        .drain_events()
        .iter()
        .any(|e| matches!(e, ComposeEvent::Send { .. })));
    c.set_busy(false)?;
    c.submit()?;
    let events = c.drain_events();
    assert!(events
        .iter()
        .any(|e| matches!(e,ComposeEvent::Send{text,..} if text=="Persistent draft 日本語")));
    assert_eq!(c.text()?, "Persistent draft 日本語");
    c.clear()?;
    c.set_mode(ComposeMode::Reply {
        message: 7,
        author: "Ana".into(),
        preview: "Original".into(),
    })?;
    c.set_attachments(vec!["design.fig".into()])?;
    c.set_text("Queued reply")?;
    c.submit()?;
    c.clear()?;
    assert!(c.drain_events().iter().any(|e| matches!(e,
        ComposeEvent::Send {text, mode: ComposeMode::Reply {message:7,..}, attachments}
        if text == "Queued reply" && attachments == &["design.fig"])));
    let mut invalid = history();
    invalid[1].id = 1;
    assert!(t.set_messages(invalid).is_err());
    assert_eq!(t.messages().len(), 4);
    t.scroll_to(4)?;
    t.refresh(d.window.scale()?)?;
    let vote = t
        .action_region(&Action::Vote {
            message: 4,
            option: 1,
        })
        .expect("visible poll action");
    assert!(t.widget().canvas_activate_region(vote)?);
    for event in t.drain_events() {
        d.action(0, event)?;
    }
    assert_eq!(t.messages()[3].poll.as_ref().unwrap().selected, Some(1));
    let large = (1..=10_000)
        .map(|id| Message {
            id,
            author: avatar("History", 0),
            body: "A measured history row.".into(),
            ..Message::default()
        })
        .collect::<Vec<_>>();
    t.set_messages(large.clone())?;
    t.scroll_to(5000)?;
    let before = t.scroll_position();
    let mut appended = large;
    appended.push(Message {
        id: 10001,
        body: "New message".into(),
        ..Message::default()
    });
    t.set_messages(appended.clone())?;
    assert!((t.scroll_position() - before).abs() < 1.);
    appended.insert(
        0,
        Message {
            id: 10002,
            body: "Earlier message".into(),
            ..Message::default()
        },
    );
    t.set_messages(appended)?;
    assert!(t.scroll_position() > before);
    t.refresh(d.window.scale()?)?;
    for appearance in [Appearance::Nebula, Appearance::Daylight, Appearance::Tiles] {
        t.set_theme(Theme::new(appearance))?;
        t.refresh(d.window.scale()?)?;
    }
    assert!(t.refresh(f64::NAN).is_err());
    d.thread.show(&d.window, None, Some(c.input.clone()))?;
    assert!(d.thread.window.is_visible()?);
    d.thread.close()?;
    assert!(!d.thread.window.is_visible()?);
    palette.open(&d.window, Some(c.input.clone()))?;
    assert!(palette.picker.picker_set_query("release-planning")?);
    assert_eq!(palette.picker.picker_match_count()?, 1);
    palette
        .picker
        .picker_get_part(sys::CUI_PICKER_RESULTS)?
        .set_selected(0)?;
    assert!(palette.picker.picker_accept()?);
    assert_eq!(
        d.headers[d.workspace.focused()].text()?,
        "# release-planning"
    );
    assert!(!palette.panel.window.is_visible()?);
    Ok(())
}
