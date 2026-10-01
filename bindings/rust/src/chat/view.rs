use super::model::*;
use crate::{
    chat_native::{Event, NativeChat},
    sys, Result, Widget,
};
use std::{cell::RefCell, rc::Rc};

fn action(e: Event) -> Option<Action> {
    Some(match e.action {
        sys::CUI_CHAT_OPEN_ROOM => Action::OpenRoom {
            id: e.id,
            new_pane: e.modifiers & sys::CUI_MOD_SHIFT as u32 != 0,
        },
        sys::CUI_CHAT_REPLY => Action::Reply(e.id),
        sys::CUI_CHAT_THREAD => Action::Thread(e.id),
        sys::CUI_CHAT_MORE => Action::More(e.id),
        sys::CUI_CHAT_COPY => Action::Copy(e.id),
        sys::CUI_CHAT_REACT => Action::React {
            message: e.id,
            key: e.text,
        },
        sys::CUI_CHAT_VOTE => Action::Vote {
            message: e.id,
            option: e.index,
        },
        sys::CUI_CHAT_ATTACHMENT => Action::Attachment {
            message: e.id,
            attachment: e.detail,
        },
        sys::CUI_CHAT_LINK => Action::Link(e.text),
        sys::CUI_CHAT_LOAD_OLDER => Action::LoadOlder,
        sys::CUI_CHAT_FOCUS => Action::Focus,
        _ => return None,
    })
}
fn region(native: &NativeChat, a: &Action) -> Option<u32> {
    let mut e = sys::cui_chat_event::default();
    let mut value = None;
    match a {
        Action::OpenRoom { id, .. } => {
            e.action = sys::CUI_CHAT_OPEN_ROOM;
            e.id = *id;
        }
        Action::Reply(id) => {
            e.action = sys::CUI_CHAT_REPLY;
            e.id = *id;
        }
        Action::Thread(id) => {
            e.action = sys::CUI_CHAT_THREAD;
            e.id = *id;
        }
        Action::More(id) => {
            e.action = sys::CUI_CHAT_MORE;
            e.id = *id;
        }
        Action::Copy(id) => {
            e.action = sys::CUI_CHAT_COPY;
            e.id = *id;
        }
        Action::React { message, key } => {
            e.action = sys::CUI_CHAT_REACT;
            e.id = *message;
            value = crate::string(key).ok();
        }
        Action::Vote { message, option } => {
            e.action = sys::CUI_CHAT_VOTE;
            e.id = *message;
            e.index = *option as u32;
        }
        Action::Attachment {
            message,
            attachment,
        } => {
            e.action = sys::CUI_CHAT_ATTACHMENT;
            e.id = *message;
            e.detail_id = *attachment;
        }
        _ => return None,
    }
    if let Some(value) = &value {
        e.text = value.as_ptr();
    }
    native.region(&e)
}
#[derive(Clone)]
pub struct Navigation {
    native: NativeChat,
}
impl Navigation {
    pub fn new(parent: &Widget, theme: Theme) -> Result<Self> {
        Ok(Self {
            native: NativeChat::new(parent, sys::CUI_CHAT_ROOMS, &theme)?,
        })
    }
    pub fn set_items(&self, items: Vec<NavItem>) -> Result<()> {
        self.native.rooms(&items)
    }
    pub fn select(&self, id: Option<u64>) -> Result<()> {
        self.native.select(id.unwrap_or(0))
    }
    pub fn set_query(&self, query: &str) -> Result<()> {
        self.native.query(query)
    }
}
#[derive(Clone)]
pub struct Timeline {
    native: NativeChat,
    items: Rc<RefCell<Vec<Message>>>,
}
impl Timeline {
    pub fn new(parent: &Widget, theme: Theme) -> Result<Self> {
        Ok(Self {
            native: NativeChat::new(parent, sys::CUI_CHAT_TIMELINE, &theme)?,
            items: Rc::new(RefCell::new(Vec::new())),
        })
    }
    pub fn set_messages(&self, items: Vec<Message>) -> Result<()> {
        self.native.messages(&items)?;
        *self.items.borrow_mut() = items;
        Ok(())
    }
    pub fn messages(&self) -> Vec<Message> {
        self.items.borrow().clone()
    }
    pub fn scroll_to(&self, id: u64) -> Result<()> {
        self.native.scroll_to(id)
    }
    pub fn scroll_to_end(&self) {
        let _ = self.native.scroll(-1.);
    }
    pub fn scroll_position(&self) -> f32 {
        self.native.offset().unwrap_or(0.) as f32
    }
    pub fn set_status(&self, status: &str) -> Result<()> {
        self.native.status(status)
    }
}
macro_rules! view_methods {
    ($name:ident) => {
        impl $name {
            /// The native canvas, for assistive activation and keyboard focus.
            pub fn widget(&self) -> Widget {
                self.native
                    .part(0)
                    .unwrap_or_else(|_| self.native.root.clone())
            }
            pub fn refresh(&self, scale: f64) -> Result<bool> {
                self.native.refresh(scale)
            }
            pub fn drain_events(&self) -> Vec<Action> {
                self.native.drain().into_iter().filter_map(action).collect()
            }
            pub fn set_theme(&self, theme: Theme) -> Result<()> {
                self.native.set_theme(&theme)
            }
            pub fn action_region(&self, a: &Action) -> Option<u32> {
                region(&self.native, a)
            }
        }
    };
}
view_methods!(Navigation);
view_methods!(Timeline);
