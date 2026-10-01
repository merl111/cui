use super::{
    fixture,
    model::{self, Room},
    ui::{Intent, Ui},
};
use cui::{chat::*, sys, ChatCommand, Command, Result};

pub struct State {
    pub ui: Ui,
    pub escape: Command,
    pub rooms: Vec<Room>,
    pub room: usize,
    pub space: usize,
    pub tab: u64,
    pub thread_open: bool,
    pub inspector_open: bool,
    pub modal: bool,
    pub verified: bool,
    replies: Vec<Message>,
    reply: Option<Reply>,
    toast_until: f64,
}
impl State {
    pub fn new(ui: Ui, escape: Command) -> Result<Self> {
        escape.set_enabled(false)?;
        let mut s = Self {
            ui,
            escape,
            rooms: fixture::rooms(),
            room: 0,
            space: 0,
            tab: 1,
            thread_open: false,
            inspector_open: true,
            modal: false,
            verified: false,
            replies: fixture::thread(),
            reply: None,
            toast_until: 0.,
        };
        if let Ok(key) = std::env::var("CUI_CHAT_ROOM") {
            s.room = s.rooms.iter().position(|r| r.key == key).unwrap_or(0);
        }
        s.open_room(s.room)?;
        Ok(s)
    }
    pub fn room_list(&self) -> Result<()> {
        let keys = ["", "work", "matrix", "fosdem", "games"];
        let mut rows = Vec::new();
        for group in 0..3 {
            for r in &self.rooms {
                let category = if r.pinned {
                    0
                } else if r.nav.avatar.square {
                    1
                } else {
                    2
                };
                if category != group
                    || (self.space > 0 && !r.space.is_empty() && r.space != keys[self.space])
                {
                    continue;
                }
                let mut row = r.nav.clone();
                row.group = ["PINNED", "ROOMS", "PEOPLE"][group].into();
                rows.push(row);
            }
        }
        self.ui.nav.rooms(&rows)?;
        if rows.iter().any(|r| r.id == self.rooms[self.room].nav.id) {
            self.ui.nav.select(self.rooms[self.room].nav.id)?;
        }
        self.ui.title.set_text(&fixture::spaces()[self.space].title)
    }
    fn header(&self) -> Result<()> {
        let r = &self.rooms[self.room];
        let mut row = r.nav.clone();
        row.detail = if row.avatar.square {
            format!("{} members · encrypted · {}", r.members, r.topic)
        } else {
            format!(
                "{} · {} · encrypted",
                r.topic,
                if row.avatar.online { "Online" } else { "Away" }
            )
        };
        row.trailing.clear();
        self.ui.header.rooms(&[row])?;
        self.ui.header.set_commands(&[
            ChatCommand {
                id: 2,
                label: "Video call".into(),
                symbol: sys::CUI_SYMBOL_VIDEO,
                action: sys::CUI_CHAT_MORE,
                ..Default::default()
            },
            ChatCommand {
                id: 6,
                label: "Search".into(),
                symbol: sys::CUI_SYMBOL_SEARCH,
                action: sys::CUI_CHAT_MORE,
                ..Default::default()
            },
            ChatCommand {
                id: 5,
                label: "Room info".into(),
                symbol: sys::CUI_SYMBOL_PANEL,
                action: sys::CUI_CHAT_MORE,
                flags: if self.inspector_open && !self.thread_open {
                    sys::CUI_CHAT_MINE as u32
                } else {
                    0
                },
                ..Default::default()
            },
        ])
    }
    pub fn open_room(&mut self, index: usize) -> Result<()> {
        self.rooms[self.room].draft = self.ui.composer.part(0)?.text()?;
        self.room = index;
        self.reply = None;
        self.ui.composer.context(0, "", "", false)?;
        self.header()?;
        let r = &self.rooms[index];
        let mut p = self.ui.timeline.presentation()?;
        p.show_sender = r.nav.avatar.square.into();
        self.ui.timeline.set_presentation(&p)?;
        self.ui.timeline.messages(&r.messages)?;
        self.ui.timeline.scroll(0.)?;
        self.ui
            .composer
            .part(0)?
            .set_placeholder(&format!("Message {}", r.nav.title))?;
        self.ui.composer.part(0)?.set_text(&r.draft)?;
        self.room_list()?;
        self.inspector()?;
        self.thread()
    }
    pub fn inspector(&self) -> Result<()> {
        let r = &self.rooms[self.room];
        let mut hero = r.nav.clone();
        hero.detail = r.topic.clone();
        let mut rows = vec![hero];
        let mut p = self.ui.inspector.presentation()?;
        p.inspector = match self.tab {
            2 => sys::CUI_CHAT_PEOPLE_LIST,
            3 => sys::CUI_CHAT_MEDIA_GRID,
            _ => sys::CUI_CHAT_PROFILE,
        };
        self.ui.inspector.set_presentation(&p)?;
        self.ui.inspector.status(if self.tab == 3 {
            "Shared files in this room (sample swatches stand in for image thumbnails)."
        } else {
            ""
        })?;
        match self.tab {
            2 => {
                let people = fixture::people();
                if r.nav.avatar.square {
                    rows.extend(people);
                } else {
                    let mut person = r.nav.clone();
                    person.detail = r.topic.clone();
                    person.trailing.clear();
                    rows.push(person);
                    rows.push(people[5].clone());
                }
                rows.push(NavItem {
                    id: 2000,
                    title: "Invite people".into(),
                    symbol: sys::CUI_SYMBOL_PLUS,
                    ..Default::default()
                });
            }
            3 => {
                for (i, name) in [
                    "call-pip-v3",
                    "composer-states",
                    "room-list",
                    "verify-flow",
                    "tokens",
                    "whiteboard",
                ]
                .iter()
                .enumerate()
                {
                    rows.push(NavItem {
                        id: 1000 + i as u64,
                        title: (*name).into(),
                        avatar: Avatar {
                            color: cui::chat_color(0.86, 0.1, (i * 55 + 20) as f64, 1.),
                            ..Default::default()
                        },
                        ..Default::default()
                    });
                }
            }
            _ => {
                let members = if r.members >= 1000 {
                    format!("{},{:03}", r.members / 1000, r.members % 1000)
                } else {
                    r.members.to_string()
                };
                for (i, (title, detail)) in [
                    ("Members", members.as_str()),
                    ("Encryption", "E2EE"),
                    ("Notifications", "Mentions & keywords"),
                    ("Pinned messages", "2"),
                    ("Threads", if self.room == 0 { "1 active" } else { "None" }),
                    ("Room settings", ""),
                    ("Leave room", ""),
                ]
                .iter()
                .enumerate()
                {
                    rows.push(NavItem {
                        id: 20 + i as u64,
                        title: (*title).into(),
                        detail: (*detail).into(),
                        ..Default::default()
                    });
                }
            }
        }
        self.ui.inspector.rooms(&rows)?;
        self.ui.inspector.select(self.tab)
    }
    pub fn thread(&self) -> Result<()> {
        self.ui.thread.set_visible(self.thread_open)?;
        self.ui
            .inspector
            .root
            .set_visible(self.inspector_open && !self.thread_open)?;
        self.header()?;
        if !self.thread_open {
            return Ok(());
        }
        if self.room != 0 {
            self.ui.thread_view.messages(&[])?;
            self.ui.thread_view.status(
                "No threads in this room yet. Hover a message\nand choose “Reply in thread”.",
            )?;
            self.ui.thread_composer.root.set_visible(false)?;
        } else {
            let mut messages = vec![self.rooms[0].messages[2].clone()];
            messages.extend(self.replies.clone());
            for (i, m) in messages.iter_mut().enumerate() {
                m.id = 100 + i as u64;
                m.date.clear();
                m.thread = None;
            }
            self.ui.thread_view.messages(&messages)?;
            self.ui.thread_view.status("")?;
            self.ui.thread_composer.root.set_visible(true)?;
        }
        Ok(())
    }
    pub fn notice(&mut self, text: &str) -> Result<()> {
        self.ui.toast_text.set_text(text)?;
        self.ui.toast.set_visible(true)?;
        self.toast_until = cui::monotonic_time() + 3.;
        Ok(())
    }
    pub fn verify(&mut self, visible: bool) -> Result<()> {
        self.escape.set_enabled(visible)?;
        self.modal = visible;
        self.ui.base.set_enabled(!visible)?;
        self.ui.scrim.set_visible(visible)?;
        self.ui.verify.set_visible(visible)?;
        if visible {
            self.ui.accept.focus()?;
        } else {
            self.ui.verify_button.focus()?;
        }
        Ok(())
    }
    fn intent(&mut self, intent: Intent) -> Result<()> {
        match intent {
            Intent::Verify => {
                if self.verified {
                    self.notice("This session is verified")?;
                } else {
                    self.verify(true)?;
                }
            }
            Intent::Dismiss => self.verify(false)?,
            Intent::Answer(matched) => {
                self.verify(false)?;
                if matched {
                    self.verified = true;
                    self.ui.verify_button.set_text("Session verified")?;
                    self.ui
                        .verify_button
                        .set_style(Some(&sys::cui_widget_style {
                            background: self.ui.theme.surface,
                            foreground: self.ui.theme.foreground,
                            border: self.ui.theme.online,
                            border_width: 1.5,
                            radius: 18.,
                            padding: 8,
                        }))?;
                }
                self.notice(if matched {
                    "Session verified — messages are end-to-end encrypted"
                } else {
                    "Verification cancelled — try again when both devices are ready"
                })?;
            }
            Intent::CloseThread => {
                self.thread_open = false;
                self.thread()?;
            }
            Intent::Search(query) => self.ui.nav.query(&query)?,
            Intent::Notice(text) => self.notice(text)?,
        }
        Ok(())
    }
    fn timeline_event(&mut self, e: cui::ChatEvent) -> Result<()> {
        let index = self.rooms[self.room]
            .messages
            .iter()
            .position(|m| m.id == e.id);
        match e.action {
            sys::CUI_CHAT_THREAD => {
                self.thread_open = true;
                self.thread()?;
            }
            sys::CUI_CHAT_REPLY => {
                if let Some(i) = index {
                    let m = &self.rooms[self.room].messages[i];
                    self.reply = Some(Reply {
                        message: m.id,
                        author: m.author.name.clone(),
                        text: m.body.plain(),
                    });
                    self.ui
                        .composer
                        .context(m.id, &m.author.name, &m.body.plain(), false)?;
                    self.ui.composer.part(0)?.focus()?;
                }
            }
            sys::CUI_CHAT_REACT => {
                if let Some(i) = index {
                    model::react(
                        &mut self.rooms[self.room].messages[i],
                        e.text,
                        e.index == u32::MAX as usize,
                    );
                    self.ui.timeline.messages(&self.rooms[self.room].messages)?;
                }
            }
            sys::CUI_CHAT_VOTE => {
                if let Some(i) = index {
                    model::vote(&mut self.rooms[self.room].messages[i], e.index);
                    self.ui.timeline.messages(&self.rooms[self.room].messages)?;
                }
            }
            sys::CUI_CHAT_COPY => {
                if let Some(i) = index {
                    self.ui
                        .window
                        .clipboard_text(&self.rooms[self.room].messages[i].body.plain())?;
                }
            }
            sys::CUI_CHAT_ATTACHMENT => self.notice("Downloading attachment…")?,
            sys::CUI_CHAT_MORE => self.notice("Message actions: edit, copy link, pin, delete")?,
            sys::CUI_CHAT_LINK => self.notice(&e.text)?,
            _ => {}
        }
        Ok(())
    }
    fn send(&mut self, text: String, thread: bool) -> Result<()> {
        let messages = if thread {
            &mut self.replies
        } else {
            &mut self.rooms[self.room].messages
        };
        let id = messages.iter().map(|m| m.id).max().unwrap_or(0) + 1;
        messages.push(Message {
            id,
            author: Avatar {
                name: "Mathias".into(),
                color: model::color("Mathias"),
                ..Default::default()
            },
            author_color: model::sender_color("Mathias"),
            time: "Now".into(),
            body: text.clone().into(),
            outgoing: true,
            reply: if thread { None } else { self.reply.take() },
            ..Default::default()
        });
        if thread {
            if let Some(t) = &mut self.rooms[0].messages[2].thread {
                t.count = self.replies.len() as u32;
            }
            self.thread()?;
            self.ui.thread_composer.part(0)?.set_text("")?;
            self.ui.thread_view.scroll(-1.)?;
        } else {
            self.rooms[self.room].nav.detail = format!("You: {text}");
            self.rooms[self.room].nav.trailing = "Now".into();
            self.ui.composer.part(0)?.set_text("")?;
            self.ui.composer.context(0, "", "", false)?;
            self.room_list()?;
        }
        self.ui.timeline.messages(&self.rooms[self.room].messages)?;
        self.ui.timeline.scroll(-1.)
    }
    pub fn tick(&mut self) -> Result<()> {
        let intents = std::mem::take(&mut *self.ui.queue.borrow_mut());
        for intent in intents {
            self.intent(intent)?;
        }
        for e in self.ui.spaces.drain() {
            if e.action == sys::CUI_CHAT_OPEN_ROOM {
                self.space = e.id as usize - 1;
                self.ui.spaces.select(e.id)?;
                self.room_list()?;
            }
        }
        for e in self.ui.nav.drain() {
            if e.action == sys::CUI_CHAT_OPEN_ROOM {
                let i = e.id as usize - 1;
                self.rooms[i].nav.unread = 0;
                self.open_room(i)?;
                self.ui.composer.part(0)?.focus()?;
            }
        }
        for e in self.ui.header.drain() {
            match e.detail {
                5 => {
                    self.inspector_open = !self.inspector_open;
                    self.thread_open = false;
                    self.thread()?;
                }
                2 => self.notice("Starting a video call…")?,
                6 => self.notice("Search in this conversation")?,
                _ => {}
            }
        }
        for e in self.ui.inspector.drain() {
            if e.action == sys::CUI_CHAT_OPEN_ROOM {
                self.tab = e.id;
                self.inspector()?;
            } else if e.id == 24 {
                self.thread_open = true;
                self.thread()?;
            } else {
                self.notice("Room settings and actions are supplied by the application")?;
            }
        }
        for e in self.ui.timeline.drain() {
            self.timeline_event(e)?;
        }
        for e in self.ui.composer.drain() {
            match e.action {
                sys::CUI_CHAT_SEND => self.send(e.text, false)?,
                sys::CUI_CHAT_CANCEL => self.reply = None,
                sys::CUI_CHAT_ATTACH => self.notice("Attach files — drop here or choose a file")?,
                sys::CUI_CHAT_EMOJI => self.notice("Emoji picker")?,
                sys::CUI_CHAT_POLL => self.notice("Create a poll")?,
                _ => {}
            }
        }
        for e in self.ui.thread_composer.drain() {
            if e.action == sys::CUI_CHAT_SEND {
                self.send(e.text, true)?;
            }
        }
        if self.toast_until > 0. && cui::monotonic_time() > self.toast_until {
            self.ui.toast.set_visible(false)?;
            self.toast_until = 0.;
        }
        let panel = if self.thread_open {
            340
        } else if self.inspector_open {
            300
        } else {
            0
        };
        self.ui.refresh(panel)
    }
}
