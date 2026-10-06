use crate::{Error, Result};
use std::collections::HashSet;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Appearance {
    Nebula,
    Daylight,
    Tiles,
}
/// RGBA tokens used by all custom components, independent of native roles.
#[derive(Clone, Debug)]
pub struct Theme {
    pub appearance: Appearance,
    pub background: u32,
    pub surface: u32,
    pub foreground: u32,
    pub muted: u32,
    pub border: u32,
    pub accent: u32,
    pub on_accent: u32,
    pub selected: u32,
    pub hover: u32,
    pub rail: u32,
    pub danger: u32,
    pub online: u32,
    pub font: String,
    pub font_size: f32,
    pub radius: f32,
}
impl Theme {
    pub fn new(appearance: Appearance) -> Self {
        let t = crate::chat_native::theme(appearance);
        Self {
            appearance,
            background: t.background,
            surface: t.surface,
            foreground: t.foreground,
            muted: t.muted,
            border: t.border,
            accent: t.accent,
            on_accent: t.on_accent,
            selected: t.soft,
            hover: t.hover,
            rail: t.rail,
            danger: t.danger,
            online: t.online,
            font: String::new(),
            font_size: t.font_size as f32,
            radius: match appearance {
                Appearance::Nebula => 9.,
                Appearance::Daylight => 18.,
                Appearance::Tiles => 7.,
            },
        }
    }
    pub(crate) fn validate(&self) -> Result<()> {
        if !self.font_size.is_finite()
            || !(8.0..=48.0).contains(&self.font_size)
            || !self.radius.is_finite()
            || !(0.0..=64.0).contains(&self.radius)
        {
            return Err(Error::InvalidInput("theme dimensions"));
        }
        check_text(&self.font, 128)
    }
}
#[derive(Clone, Debug, Default)]
pub struct Avatar {
    /// Retained raster/vector image; None uses the name initial.
    pub image: Option<crate::Icon>,
    pub name: String,
    pub color: u32,
    pub online: bool,
    pub square: bool,
}
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum TextStyle {
    #[default]
    Body,
    Strong,
    Muted,
    Mention,
    Code,
}
#[derive(Clone, Debug)]
pub struct Span {
    pub text: String,
    pub style: TextStyle,
    pub link: Option<String>,
}
impl Span {
    pub fn plain(text: impl Into<String>) -> Self {
        Self {
            text: text.into(),
            style: TextStyle::Body,
            link: None,
        }
    }
}
#[derive(Clone, Debug, Default)]
pub struct RichText(pub Vec<Span>);
impl From<&str> for RichText {
    fn from(s: &str) -> Self {
        Self(vec![Span::plain(s)])
    }
}
impl From<String> for RichText {
    fn from(s: String) -> Self {
        Self(vec![Span::plain(s)])
    }
}
impl RichText {
    pub fn plain(&self) -> String {
        self.0.iter().map(|s| s.text.as_str()).collect()
    }
}
#[derive(Clone, Debug, Default)]
pub struct Attachment {
    pub id: u64,
    pub name: String,
    pub detail: String,
    pub image: Option<crate::Icon>,
}
#[derive(Clone, Debug)]
pub struct Reaction {
    /// Native hover tooltip listing the people who reacted.
    pub tooltip: String,
    pub key: String,
    pub count: u32,
    pub mine: bool,
}
#[derive(Clone, Debug)]
pub struct PollOption {
    pub label: String,
    pub votes: u32,
}
#[derive(Clone, Debug)]
pub struct Poll {
    pub question: String,
    pub options: Vec<PollOption>,
    pub selected: Option<usize>,
    pub closed: bool,
}
#[derive(Clone, Debug)]
pub struct Reply {
    pub message: u64,
    pub author: String,
    pub text: String,
}
#[derive(Clone, Debug)]
pub struct ThreadSummary {
    pub count: u32,
    pub preview: String,
    pub participants: Vec<Avatar>,
}
#[derive(Clone, Debug, Default)]
pub struct Message {
    /// Optional sender-label color. Zero selects the appearance default.
    pub author_color: u32,
    pub id: u64,
    pub author: Avatar,
    pub time: String,
    pub date: String,
    pub body: RichText,
    pub outgoing: bool,
    pub highlighted: bool,
    pub reply: Option<Reply>,
    pub attachments: Vec<Attachment>,
    pub reactions: Vec<Reaction>,
    pub poll: Option<Poll>,
    pub thread: Option<ThreadSummary>,
    /// Latest readers: title = name, detail = localized tooltip, trailing = user key.
    pub read_by: Vec<NavItem>,
    pub delivery: i32,
    pub delivery_label: String,
}
#[derive(Clone, Debug, Default)]
pub struct NavItem {
    pub id: u64,
    pub group: String,
    pub title: String,
    pub detail: String,
    pub avatar: Avatar,
    pub unread: u32,
    pub mention: bool,
    pub trailing: String,
    pub symbol: i32,
    pub disabled: bool,
    /// Marks an additional active/open row, independently of keyboard selection.
    pub active: bool,
}
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum Action {
    OpenRoom {
        id: u64,
        new_pane: bool,
    },
    Reply(u64),
    Delivery(u64),
    OpenReply {
        message: u64,
        original: u64,
    },
    Thread(u64),
    More(u64),
    Copy(u64),
    React {
        message: u64,
        key: String,
    },
    Vote {
        message: u64,
        option: usize,
    },
    Attachment {
        message: u64,
        attachment: u64,
    },
    Link(String),
    OpenProfile(u64),
    OpenReader {
        message: u64,
        reader: u64,
        user: String,
    },
    ComposeMore,
    LoadOlder,
    Focus,
}
pub(crate) fn check_text(s: &str, max: usize) -> Result<()> {
    if s.len() > max || s.contains('\0') {
        Err(Error::InvalidInput("text length or interior NUL"))
    } else {
        Ok(())
    }
}
pub(crate) fn validate_messages(items: &[Message]) -> Result<()> {
    if items.len() > 100_000 {
        return Err(Error::InvalidInput("100000 message limit"));
    }
    let mut ids = HashSet::new();
    for m in items {
        if m.id == 0 || !ids.insert(m.id) {
            return Err(Error::InvalidInput("unique nonzero message IDs required"));
        }
        for s in [&m.author.name, &m.time, &m.date] {
            check_text(s, 1024)?;
        }
        if m.body.0.len() > 128 || m.reactions.len() > 32 || m.attachments.len() > 16 {
            return Err(Error::InvalidInput("message content limit"));
        }
        if m.read_by.len() > 128 {
            return Err(Error::InvalidInput("128 reader limit"));
        }
        for reader in &m.read_by {
            if reader.id == 0 {
                return Err(Error::InvalidInput("reader ID"));
            }
            for s in [&reader.title, &reader.detail, &reader.trailing] {
                check_text(s, 4096)?;
            }
        }
        check_text(&m.body.plain(), 65536)?;
        for s in &m.body.0 {
            if let Some(link) = &s.link {
                check_text(link, 4096)?;
            }
        }
        for r in &m.reactions {
            check_text(&r.key, 64)?;
        }
        let mut files = HashSet::new();
        for a in &m.attachments {
            if a.id == 0 || !files.insert(a.id) {
                return Err(Error::InvalidInput("attachment IDs"));
            }
            check_text(&a.name, 1024)?;
            check_text(&a.detail, 1024)?;
        }
        if let Some(r) = &m.reply {
            check_text(&r.author, 1024)?;
            check_text(&r.text, 4096)?;
        }
        if let Some(t) = &m.thread {
            check_text(&t.preview, 1024)?;
            if t.participants.len() > 16 {
                return Err(Error::InvalidInput("thread avatars"));
            }
            for a in &t.participants {
                check_text(&a.name, 1024)?;
            }
        }
        if let Some(p) = &m.poll {
            check_text(&p.question, 1024)?;
            if p.options.is_empty()
                || p.options.len() > 16
                || p.selected.is_some_and(|i| i >= p.options.len())
            {
                return Err(Error::InvalidInput("poll options"));
            }
            for o in &p.options {
                check_text(&o.label, 1024)?;
            }
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    fn message() -> Message {
        Message {
            id: 1,
            body: "Valid 日本語".into(),
            ..Message::default()
        }
    }
    #[test]
    fn rejects_ambiguous_message_and_attachment_ids() {
        assert!(validate_messages(&[message(), message()]).is_err());
        let mut m = message();
        m.id = 0;
        assert!(validate_messages(&[m.clone()]).is_err());
        m.id = 1;
        m.attachments = vec![
            Attachment {
                image: None,
                id: 1,
                name: "a".into(),
                detail: String::new(),
            },
            Attachment {
                image: None,
                id: 1,
                name: "b".into(),
                detail: String::new(),
            },
        ];
        assert!(validate_messages(&[m]).is_err());
    }
    #[test]
    fn validates_poll_selection_and_payload_limits() {
        let mut m = message();
        m.poll = Some(Poll {
            question: "Pick".into(),
            options: vec![PollOption {
                label: "One".into(),
                votes: 0,
            }],
            selected: Some(1),
            closed: false,
        });
        assert!(validate_messages(&[m.clone()]).is_err());
        m.poll.as_mut().unwrap().selected = Some(0);
        assert!(validate_messages(&[m.clone()]).is_ok());
        m.body = RichText::from("x".repeat(65537));
        assert!(validate_messages(&[m.clone()]).is_err());
        m.body = RichText::from("interior\0nul");
        assert!(validate_messages(&[m]).is_err());
    }
    #[test]
    fn theme_rejects_nonfinite_and_unbounded_dimensions() {
        let mut theme = Theme::new(Appearance::Tiles);
        assert!(theme.validate().is_ok());
        theme.font_size = f32::NAN;
        assert!(theme.validate().is_err());
        theme.font_size = 14.;
        theme.radius = -1.;
        assert!(theme.validate().is_err());
    }
}
