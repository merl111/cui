//! Reusable desktop chat components for native CUI applications.
//!
//! Models are owned Rust values. Changes are silent; user actions are queued as
//! [`Action`] values for the application to handle (no networking is bundled).
//! Call `refresh` after layout changes, and drain events on the main thread.
//! Navigation and timelines draw only visible content and scroll independently.
mod composer;
mod model;
mod panels;
mod view;
mod workspace;
pub use composer::{ComposeEvent, ComposeMode, Composer};
pub use model::*;
pub use panels::{Palette, PaletteItem, Panel};
pub use view::{Navigation, Timeline};
pub use workspace::{Layout, Workspace};
