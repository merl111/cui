//! Marshalling for the C chat components. Layout, rendering, editing and pane
//! state live in libcui; this module only owns FFI inputs and Rust event queues.
use crate::{chat::*, string, sys, Error, Result, Widget};
use std::{
    cell::RefCell,
    collections::VecDeque,
    ffi::{CStr, CString},
    rc::Rc,
};

pub(crate) fn ok(value: i32) -> Result<()> {
    if value != 0 {
        Ok(())
    } else {
        Err(Error::NativeFailure)
    }
}
pub fn chat_color(l: f64, c: f64, hue: f64, alpha: f64) -> u32 {
    unsafe { sys::cui_chat_color(l, c, hue, alpha) }
}
pub(crate) fn appearance(a: Appearance) -> i32 {
    match a {
        Appearance::Nebula => 0,
        Appearance::Daylight => 1,
        Appearance::Tiles => 2,
    }
}
pub(crate) fn theme(a: Appearance) -> sys::cui_chat_theme {
    let mut t = sys::cui_chat_theme::default();
    unsafe {
        sys::cui_chat_theme_get(appearance(a), &mut t);
    }
    t
}
/// A reusable C chat component, available with every appearance preset.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum ChatKind {
    Rooms = sys::CUI_CHAT_ROOMS,
    Timeline = sys::CUI_CHAT_TIMELINE,
    Composer = sys::CUI_CHAT_COMPOSER,
    Workspace = sys::CUI_CHAT_WORKSPACE,
    Header = sys::CUI_CHAT_HEADER,
    Spaces = sys::CUI_CHAT_SPACES,
    Inspector = sys::CUI_CHAT_INSPECTOR,
    Message = sys::CUI_CHAT_MESSAGE,
    AttachmentCard = sys::CUI_CHAT_ATTACHMENT_CARD,
    ReactionStrip = sys::CUI_CHAT_REACTION_STRIP,
    PollCard = sys::CUI_CHAT_POLL_CARD,
    ReplyPreview = sys::CUI_CHAT_REPLY_PREVIEW,
    ThreadSummary = sys::CUI_CHAT_THREAD_SUMMARY,
    Avatar = sys::CUI_CHAT_AVATAR,
}
/// Independent C presentation values. Use a preset, then override individual fields.
pub type ChatPresentation = sys::cui_chat_presentation;
pub fn chat_presentation(preset: Appearance) -> Result<ChatPresentation> {
    let mut value = ChatPresentation::default();
    ok(unsafe { sys::cui_chat_presentation_preset(appearance(preset), &mut value) })?;
    Ok(value)
}
/// An application-owned command; strings are copied into C by `set_commands`.
#[derive(Clone, Debug, Default)]
pub struct ChatCommand {
    pub id: u64,
    pub label: String,
    pub text: String,
    pub symbol: i32,
    pub action: i32,
    pub flags: u32,
}
#[derive(Clone, Debug)]
pub struct Event {
    pub action: i32,
    pub id: u64,
    pub detail: u64,
    pub index: usize,
    pub modifiers: u32,
    pub text: String,
}
#[derive(Clone)]
pub struct NativeChat {
    pub root: Widget,
    events: Rc<RefCell<VecDeque<Event>>>,
}
impl NativeChat {
    /// Create a typed component backed entirely by libcui.
    pub fn create(parent: &Widget, kind: ChatKind, theme: &Theme) -> Result<Self> {
        Self::new(parent, kind as i32, theme)
    }
    pub fn new(parent: &Widget, kind: i32, theme: &Theme) -> Result<Self> {
        let rt = parent.handle.live()?;
        let root = Widget::from_native(&rt, unsafe {
            sys::cui_chat_create(
                parent.handle.ptr.as_ptr(),
                kind,
                appearance(theme.appearance),
            )
        })?;
        let events = Rc::new(RefCell::new(VecDeque::new()));
        let queue = events.clone();
        root.on_action(move |w| {
            let mut e = sys::cui_chat_event::default();
            if unsafe { sys::cui_chat_event_get(w.handle.ptr.as_ptr(), &mut e) } != 0 {
                let text = if e.text.is_null() {
                    String::new()
                } else {
                    unsafe { CStr::from_ptr(e.text) }
                        .to_string_lossy()
                        .into_owned()
                };
                let mut q = queue.borrow_mut();
                if q.len() < 1024 {
                    q.push_back(Event {
                        action: e.action,
                        id: e.id,
                        detail: e.detail_id,
                        index: e.index as usize,
                        modifiers: e.modifiers,
                        text,
                    });
                }
            }
        })?;
        let result = Self { root, events };
        result.set_theme(theme)?;
        Ok(result)
    }
    fn ptr(&self) -> Result<*mut sys::cui_widget> {
        self.root.handle.live()?;
        Ok(self.root.handle.ptr.as_ptr())
    }
    pub fn part(&self, index: u32) -> Result<Widget> {
        let rt = self.root.handle.live()?;
        Widget::from_native(&rt, unsafe { sys::cui_chat_part(self.ptr()?, index) })
    }
    pub fn drain(&self) -> Vec<Event> {
        self.events.borrow_mut().drain(..).collect()
    }
    pub fn set_theme(&self, t: &Theme) -> Result<()> {
        t.validate()?;
        let mut raw = theme(t.appearance);
        raw.background = t.background;
        raw.surface = t.surface;
        raw.foreground = t.foreground;
        raw.muted = t.muted;
        raw.border = t.border;
        raw.accent = t.accent;
        raw.on_accent = t.on_accent;
        raw.soft = t.selected;
        raw.danger = t.danger;
        raw.online = t.online;
        raw.font_size = t.font_size as f64;
        ok(unsafe { sys::cui_chat_set_theme(self.ptr()?, &raw) })?;
        self.root.set_font(&t.font, 0.0, 0).map(|_| ())
    }
    pub fn presentation(&self) -> Result<ChatPresentation> {
        let mut value = ChatPresentation::default();
        ok(unsafe { sys::cui_chat_presentation_get(self.ptr()?, &mut value) })?;
        Ok(value)
    }
    pub fn set_presentation(&self, value: &ChatPresentation) -> Result<()> {
        ok(unsafe { sys::cui_chat_set_presentation(self.ptr()?, value) })
    }
    pub fn set_commands(&self, commands: &[ChatCommand]) -> Result<()> {
        let mut arena = Arena::default();
        let raw = commands
            .iter()
            .map(|c| {
                Ok(sys::cui_chat_command {
                    id: c.id,
                    label: arena.text(&c.label)?,
                    text: arena.text(&c.text)?,
                    symbol: c.symbol,
                    action: c.action,
                    flags: c.flags,
                })
            })
            .collect::<Result<Vec<_>>>()?;
        ok(unsafe { sys::cui_chat_set_commands(self.ptr()?, raw.as_ptr(), raw.len()) })
    }
    pub fn messages(&self, items: &[Message]) -> Result<()> {
        validate_messages(items)?;
        let mut arena = Arena::default();
        let mut participant_storage: Vec<Vec<sys::cui_chat_room>> = Vec::new();
        let mut spans = Vec::new();
        let mut files = Vec::new();
        let mut reactions = Vec::new();
        let mut options = Vec::new();
        let mut raw = Vec::new();
        for m in items {
            spans.push(
                m.body
                    .0
                    .iter()
                    .map(|s| {
                        Ok(sys::cui_chat_span {
                            text: arena.text(&s.text)?,
                            link: arena.text(s.link.as_deref().unwrap_or(""))?,
                            style: s.style as i32,
                        })
                    })
                    .collect::<Result<Vec<_>>>()?,
            );
            files.push(
                m.attachments
                    .iter()
                    .map(|a| {
                        Ok(sys::cui_chat_detail {
                            id: a.id,
                            text: arena.text(&a.name)?,
                            detail: arena.text(&a.detail)?,
                            ..Default::default()
                        })
                    })
                    .collect::<Result<Vec<_>>>()?,
            );
            reactions.push(
                m.reactions
                    .iter()
                    .map(|r| {
                        Ok(sys::cui_chat_detail {
                            text: arena.text(&r.key)?,
                            count: r.count,
                            flags: if r.mine { sys::CUI_CHAT_MINE as u32 } else { 0 },
                            ..Default::default()
                        })
                    })
                    .collect::<Result<Vec<_>>>()?,
            );
            options.push(
                m.poll
                    .as_ref()
                    .map(|p| {
                        p.options
                            .iter()
                            .map(|o| {
                                Ok(sys::cui_chat_detail {
                                    text: arena.text(&o.label)?,
                                    count: o.votes,
                                    ..Default::default()
                                })
                            })
                            .collect::<Result<Vec<_>>>()
                    })
                    .transpose()?
                    .unwrap_or_default(),
            );
            let i = raw.len();
            let participants = m
                .thread
                .as_ref()
                .map(|t| t.participants.as_slice())
                .unwrap_or(&[]);
            let faces = participants
                .iter()
                .enumerate()
                .map(|(i, a)| {
                    Ok(sys::cui_chat_room {
                        id: i as u64 + 1,
                        title: arena.text(&a.name)?,
                        avatar_color: a.color,
                        avatar: a.image.as_ref().map_or(std::ptr::null_mut(), |i| i.ptr.as_ptr()),
                        flags: (if a.online {
                            sys::CUI_CHAT_ONLINE as u32
                        } else {
                            0
                        }) | (if a.square {
                            sys::CUI_CHAT_SQUARE as u32
                        } else {
                            0
                        }),
                        ..Default::default()
                    })
                })
                .collect::<Result<Vec<_>>>()?;
            participant_storage.push(faces);
            let faces = participant_storage.last().unwrap();
            raw.push(sys::cui_chat_message {
                id: m.id,
                author: arena.text(&m.author.name)?,
                time: arena.text(&m.time)?,
                date: arena.text(&m.date)?,
                avatar_color: m.author.color,
                avatar: m.author.image.as_ref().map_or(std::ptr::null_mut(), |i| i.ptr.as_ptr()),
                flags: (if m.outgoing { 1 } else { 0 })
                    | (if m.highlighted { 2 } else { 0 })
                    | (if i > 0 && items[i - 1].author.name == m.author.name && m.date.is_empty() {
                        4
                    } else {
                        0
                    })
                    | (if m.author.square { 16 } else { 0 })
                    | (if m.poll.as_ref().is_some_and(|p| p.closed) {
                        128
                    } else {
                        0
                    }),
                spans: spans[i].as_ptr(),
                span_count: spans[i].len(),
                attachments: files[i].as_ptr(),
                attachment_count: files[i].len(),
                reactions: reactions[i].as_ptr(),
                reaction_count: reactions[i].len(),
                options: options[i].as_ptr(),
                option_count: options[i].len(),
                reply_author: arena
                    .text(m.reply.as_ref().map(|r| r.author.as_str()).unwrap_or(""))?,
                reply_text: arena.text(m.reply.as_ref().map(|r| r.text.as_str()).unwrap_or(""))?,
                reply_id: m.reply.as_ref().map(|r| r.message).unwrap_or(0),
                poll_question: arena
                    .text(m.poll.as_ref().map(|p| p.question.as_str()).unwrap_or(""))?,
                selected_option: m
                    .poll
                    .as_ref()
                    .and_then(|p| p.selected)
                    .map(|i| i as i32)
                    .unwrap_or(-1),
                thread_preview: arena
                    .text(m.thread.as_ref().map(|t| t.preview.as_str()).unwrap_or(""))?,
                thread_count: m.thread.as_ref().map(|t| t.count).unwrap_or(0),
                thread_participants: faces.as_ptr(),
                thread_participant_count: faces.len(),
                author_color: m.author_color,
            });
        }
        ok(unsafe { sys::cui_chat_set_messages(self.ptr()?, raw.as_ptr(), raw.len()) })
    }
    pub fn rooms(&self, items: &[NavItem]) -> Result<()> {
        let mut arena = Arena::default();
        let raw = items
            .iter()
            .map(|r| {
                Ok(sys::cui_chat_room {
                    id: r.id,
                    group: arena.text(&r.group)?,
                    title: arena.text(&r.title)?,
                    detail: arena.text(&r.detail)?,
                    trailing: arena.text(&r.trailing)?,
                    avatar_color: r.avatar.color,
                    avatar: r.avatar.image.as_ref().map_or(std::ptr::null_mut(), |i| i.ptr.as_ptr()),
                    unread: r.unread,
                    symbol: r.symbol,
                    flags: (if r.mention { 2 } else { 0 })
                        | (if r.avatar.online { 8 } else { 0 })
                        | (if r.avatar.square { 16 } else { 0 })
                        | (if r.disabled { 32 } else { 0 })
                        | (if r.active { 64 } else { 0 }),
                })
            })
            .collect::<Result<Vec<_>>>()?;
        ok(unsafe { sys::cui_chat_set_rooms(self.ptr()?, raw.as_ptr(), raw.len()) })
    }
    pub fn select(&self, id: u64) -> Result<()> {
        ok(unsafe { sys::cui_chat_select(self.ptr()?, id) })
    }
    pub fn query(&self, value: &str) -> Result<()> {
        let value = string(value)?;
        ok(unsafe { sys::cui_chat_set_query(self.ptr()?, value.as_ptr()) })
    }
    pub fn status(&self, value: &str) -> Result<()> {
        let value = string(value)?;
        ok(unsafe { sys::cui_chat_set_status(self.ptr()?, value.as_ptr()) })
    }
    pub fn refresh(&self, scale: f64) -> Result<bool> {
        ok(unsafe { sys::cui_chat_refresh(self.ptr()?, scale) })?;
        Ok(true)
    }
    pub fn scroll(&self, offset: f64) -> Result<()> {
        ok(unsafe { sys::cui_chat_scroll(self.ptr()?, offset) })
    }
    pub fn scroll_to(&self, id: u64) -> Result<()> {
        ok(unsafe { sys::cui_chat_scroll_to(self.ptr()?, id) })
    }
    pub fn region(&self, e: &sys::cui_chat_event) -> Option<u32> {
        let id = unsafe { sys::cui_chat_action_region(self.ptr().ok()?, e) };
        (id != 0).then_some(id)
    }
    pub fn cancel(&self) -> Result<()> {
        ok(unsafe { sys::cui_chat_compose_cancel(self.ptr()?) })
    }
    pub fn offset(&self) -> Result<f64> {
        Ok(unsafe { sys::cui_chat_scroll_offset(self.ptr()?) })
    }
    pub fn context(&self, id: u64, author: &str, preview: &str, editing: bool) -> Result<()> {
        let author = string(author)?;
        let preview = string(preview)?;
        ok(unsafe {
            sys::cui_chat_compose_context(
                self.ptr()?,
                id,
                author.as_ptr(),
                preview.as_ptr(),
                editing.into(),
            )
        })
    }
    pub fn files(&self, files: &[String]) -> Result<()> {
        let mut arena = Arena::default();
        let raw = files
            .iter()
            .enumerate()
            .map(|(i, f)| {
                Ok(sys::cui_chat_detail {
                    id: i as u64 + 1,
                    text: arena.text(f)?,
                    ..Default::default()
                })
            })
            .collect::<Result<Vec<_>>>()?;
        ok(unsafe { sys::cui_chat_compose_files(self.ptr()?, raw.as_ptr(), raw.len()) })
    }
    pub fn busy(&self, busy: bool) -> Result<()> {
        ok(unsafe { sys::cui_chat_compose_busy(self.ptr()?, busy.into()) })
    }
    pub fn submit(&self) -> Result<bool> {
        Ok(unsafe { sys::cui_chat_compose_submit(self.ptr()?) } != 0)
    }
    pub fn layout(&self, layout: u32) -> Result<()> {
        ok(unsafe { sys::cui_chat_workspace_layout(self.ptr()?, layout) })
    }
    pub fn focus(&self, pane: u32) -> Result<()> {
        ok(unsafe { sys::cui_chat_workspace_focus(self.ptr()?, pane) })
    }
    pub fn close(&self, pane: u32) -> Result<()> {
        ok(unsafe { sys::cui_chat_workspace_close(self.ptr()?, pane) })
    }
    pub fn maximize(&self, pane: u32) -> Result<()> {
        ok(unsafe { sys::cui_chat_workspace_maximize(self.ptr()?, pane) })
    }
    pub fn restore(&self) -> Result<()> {
        ok(unsafe { sys::cui_chat_workspace_restore(self.ptr()?) })
    }
    pub fn mask(&self) -> u32 {
        self.ptr()
            .map(|p| unsafe { sys::cui_chat_workspace_mask(p) })
            .unwrap_or(0)
    }
    pub fn focused(&self) -> usize {
        self.ptr()
            .map(|p| unsafe { sys::cui_chat_workspace_focused(p) } as usize)
            .unwrap_or(0)
    }
}
#[derive(Default)]
struct Arena(Vec<CString>);
impl Arena {
    fn text(&mut self, s: &str) -> Result<*const std::ffi::c_char> {
        let s = string(s)?;
        let ptr = s.as_ptr();
        self.0.push(s);
        Ok(ptr)
    }
}
