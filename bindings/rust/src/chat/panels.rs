use super::model::check_text;
use crate::{sys, App, Choice, Error, Result, Widget, Window};
use std::{cell::RefCell, rc::Rc};
/// Reusable auxiliary surface for inspectors, call controls or custom dialogs.
/// Anchored panels use CUI's native attachment implementation (GTK popovers,
/// native auxiliary windows elsewhere); they do not pretend to be web modals.
#[derive(Clone)]
pub struct Panel {
    pub window: Window,
    pub content: Widget,
    return_focus: Rc<RefCell<Option<Widget>>>,
}
impl Panel {
    pub fn new(app: &App, title: &str, width: i32, height: i32) -> Result<Self> {
        let window = app.window(title, width, height)?;
        let content = window.root()?;
        content.box_set_padding(16)?;
        Ok(Self {
            window,
            content,
            return_focus: Rc::new(RefCell::new(None)),
        })
    }
    pub fn show(
        &self,
        parent: &Window,
        anchor: Option<[i32; 4]>,
        return_focus: Option<Widget>,
    ) -> Result<()> {
        if let Some(rect) = anchor {
            if !self.window.frame(false, false, 12.)? || !self.window.anchor(parent, rect)? {
                return Err(Error::NativeFailure);
            }
        }
        *self.return_focus.borrow_mut() = return_focus;
        self.window.show()
    }
    pub fn close(&self) -> Result<()> {
        self.window.close()?;
        if let Some(w) = self.return_focus.borrow_mut().take() {
            w.focus()?;
        }
        Ok(())
    }
}
#[derive(Clone, Debug)]
pub struct PaletteItem {
    pub id: u64,
    pub group: String,
    pub title: String,
    pub detail: String,
    pub keywords: String,
    pub disabled: bool,
}
/// Search engine and keyboard selection supplied by CUI's native picker. A
/// reusable auxiliary window makes the palette float without moving app content.
#[derive(Clone)]
pub struct Palette {
    pub panel: Panel,
    pub picker: Widget,
}
impl Palette {
    pub fn new(app: &App) -> Result<Self> {
        let panel = Panel::new(app, "Rooms and commands", 620, 460)?;
        let picker = panel.content.picker(
            sys::CUI_COMMAND_PALETTE,
            "Search rooms, people and commands",
        )?;
        picker.expand(true)?;
        Ok(Self { panel, picker })
    }
    pub fn set_items(&self, items: &[PaletteItem]) -> Result<bool> {
        let labels = items
            .iter()
            .map(|i| format!("{} · {}", i.group, i.title))
            .collect::<Vec<_>>();
        let choices = items
            .iter()
            .zip(&labels)
            .map(|(i, label)| {
                check_text(&i.title, 4096)?;
                check_text(&i.group, 1024)?;
                Ok(Choice {
                    id: i.id,
                    label,
                    detail: &i.detail,
                    keywords: &i.keywords,
                    disabled: i.disabled,
                })
            })
            .collect::<Result<Vec<_>>>()?;
        self.picker.picker_set_items(&choices)
    }
    pub fn open(&self, parent: &Window, return_focus: Option<Widget>) -> Result<()> {
        self.panel.show(parent, None, return_focus)?;
        self.picker.picker_set_query("")?;
        self.picker.picker_open(None)
    }
    pub fn close(&self) -> Result<()> {
        self.picker.picker_close()?;
        self.panel.close()
    }
}
