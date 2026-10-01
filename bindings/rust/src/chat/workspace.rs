use crate::{
    chat::{Appearance, Theme},
    chat_native::NativeChat,
    sys, Result, Widget,
};
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Layout {
    Single,
    Split,
    MainTwo,
    Quad,
}
impl Layout {
    pub fn visible(self) -> &'static [usize] {
        match self {
            Self::Single => &[0],
            Self::Split => &[0, 1],
            Self::MainTwo => &[0, 1, 2],
            Self::Quad => &[0, 1, 2, 3],
        }
    }
}
/// Thin binding to the persistent native C workspace.
#[derive(Clone)]
pub struct Workspace {
    pub root: Widget,
    native: NativeChat,
}
impl Workspace {
    pub fn new(parent: &Widget) -> Result<Self> {
        let native = NativeChat::new(
            parent,
            sys::CUI_CHAT_WORKSPACE,
            &Theme::new(Appearance::Tiles),
        )?;
        Ok(Self {
            root: native.root.clone(),
            native,
        })
    }
    pub fn pane(&self, index: usize) -> Result<Widget> {
        self.native.part(index as u32)
    }
    pub fn layout(&self) -> Layout {
        match self.native.mask().count_ones() {
            1 => Layout::Single,
            2 => Layout::Split,
            3 => Layout::MainTwo,
            _ => Layout::Quad,
        }
    }
    pub fn visible_panes(&self) -> Vec<usize> {
        (0..4)
            .filter(|i| self.native.mask() & (1 << i) != 0)
            .collect()
    }
    pub fn focused(&self) -> usize {
        self.native.focused()
    }
    pub fn set_focus(&self, index: usize) -> Result<()> {
        self.native.focus(index as u32)
    }
    pub fn set_layout(&self, layout: Layout) -> Result<()> {
        self.native.layout(layout as u32 + 1)
    }
    pub fn close_pane(&self, index: usize) -> Result<()> {
        self.native.close(index as u32)
    }
    pub fn maximize(&self, index: usize) -> Result<()> {
        self.native.maximize(index as u32)
    }
    pub fn restore(&self) -> Result<()> {
        self.native.restore()
    }
}
