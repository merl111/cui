use super::Theme;
use crate::{chat_native::NativeChat, sys, Error, Result, Widget};
use std::{cell::RefCell, rc::Rc};
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub enum ComposeMode {
    #[default]
    Message,
    Reply {
        message: u64,
        author: String,
        preview: String,
    },
    Edit {
        message: u64,
    },
}
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum ComposeEvent {
    Send {
        text: String,
        mode: ComposeMode,
        attachments: Vec<String>,
    },
    Cancel,
    Attach,
    Emoji,
    Poll,
    Changed,
}
/// Binding to libcui's composer. Native code owns editing and submission rules.
#[derive(Clone)]
pub struct Composer {
    pub root: Widget,
    pub input: Widget,
    pub send: Widget,
    native: NativeChat,
    mode: Rc<RefCell<ComposeMode>>,
    files: Rc<RefCell<Vec<String>>>,
    pending: Rc<RefCell<Vec<ComposeEvent>>>,
}
impl Composer {
    pub fn new(parent: &Widget, theme: Theme) -> Result<Self> {
        let native = NativeChat::new(parent, sys::CUI_CHAT_COMPOSER, &theme)?;
        Ok(Self {
            root: native.root.clone(),
            input: native.part(0)?,
            send: native.part(1)?,
            native,
            mode: Rc::new(RefCell::new(ComposeMode::Message)),
            files: Rc::new(RefCell::new(Vec::new())),
            pending: Rc::new(RefCell::new(Vec::new())),
        })
    }
    pub fn set_text(&self, text: &str) -> Result<()> {
        super::model::check_text(text, 65536)?;
        self.input.set_text(text)?;
        self.update()
    }
    pub fn text(&self) -> Result<String> {
        self.input.text()
    }
    pub fn set_mode(&self, mode: ComposeMode) -> Result<()> {
        self.collect_events();
        match &mode {
            ComposeMode::Message => self.native.context(0, "", "", false)?,
            ComposeMode::Reply {
                message,
                author,
                preview,
            } => self.native.context(*message, author, preview, false)?,
            ComposeMode::Edit { message } => self.native.context(*message, "", "", true)?,
        };
        *self.mode.borrow_mut() = mode;
        Ok(())
    }
    pub fn mode(&self) -> ComposeMode {
        self.mode.borrow().clone()
    }
    pub fn set_attachments(&self, files: Vec<String>) -> Result<()> {
        self.collect_events();
        self.native.files(&files)?;
        *self.files.borrow_mut() = files;
        Ok(())
    }
    pub fn submit(&self) -> Result<()> {
        self.native.submit().map(|_| ())
    }
    pub fn cancel(&self) -> Result<()> {
        self.collect_events();
        self.native.cancel()?;
        *self.mode.borrow_mut() = ComposeMode::Message;
        self.files.borrow_mut().clear();
        Ok(())
    }
    pub fn clear(&self) -> Result<()> {
        self.set_text("")?;
        self.set_mode(ComposeMode::Message)?;
        self.set_attachments(Vec::new())
    }
    pub fn set_busy(&self, busy: bool) -> Result<()> {
        self.native.busy(busy)
    }
    pub fn set_theme(&self, theme: Theme) -> Result<()> {
        self.native.set_theme(&theme)
    }
    pub fn update(&self) -> Result<()> {
        self.native.refresh(1.).map(|_| ())
    }
    pub fn take_error(&self) -> Option<Error> {
        None
    }
    pub fn drain_events(&self) -> Vec<ComposeEvent> {
        self.collect_events();
        self.pending.borrow_mut().drain(..).collect()
    }
    fn collect_events(&self) {
        let events: Vec<_> = self
            .native
            .drain()
            .into_iter()
            .filter_map(|e| {
                Some(match e.action {
                    sys::CUI_CHAT_SEND => ComposeEvent::Send {
                        text: e.text,
                        mode: self.mode(),
                        attachments: self.files.borrow().clone(),
                    },
                    sys::CUI_CHAT_CANCEL => {
                        *self.mode.borrow_mut() = ComposeMode::Message;
                        self.files.borrow_mut().clear();
                        ComposeEvent::Cancel
                    }
                    sys::CUI_CHAT_ATTACH => ComposeEvent::Attach,
                    sys::CUI_CHAT_EMOJI => ComposeEvent::Emoji,
                    sys::CUI_CHAT_POLL => ComposeEvent::Poll,
                    sys::CUI_CHAT_CHANGED => ComposeEvent::Changed,
                    _ => return None,
                })
            })
            .collect();
        self.pending.borrow_mut().extend(events);
    }
}
