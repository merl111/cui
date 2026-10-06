use cui::{chat::*, chat_color};

/// Application data. Replace fixture loading with Archaic's Matrix model.
pub struct Room {
    pub key: String,
    pub space: String,
    pub topic: String,
    pub members: u32,
    pub pinned: bool,
    pub nav: NavItem,
    pub messages: Vec<Message>,
    pub draft: String,
}
pub fn color(name: &str) -> u32 {
    let hue: u32 = name
        .trim_start_matches(['#', ' '])
        .bytes()
        .map(|b| b as u32 * 7)
        .sum();
    chat_color(0.84, 0.12, (hue % 360) as f64, 1.)
}
pub fn sender_color(name: &str) -> u32 {
    let hue: u32 = name.bytes().map(|b| b as u32 * 7).sum();
    chat_color(0.45, 0.14, (hue % 360) as f64, 1.)
}
pub fn react(message: &mut Message, key: String, quick: bool) {
    if let Some(r) = message.reactions.iter_mut().find(|r| r.key == key) {
        if quick && r.mine {
            return;
        }
        r.mine = !r.mine;
        r.count = if r.mine {
            r.count + 1
        } else {
            r.count.saturating_sub(1)
        };
    } else {
        message.reactions.push(Reaction {
            tooltip: String::new(),
            key,
            count: 1,
            mine: true,
        });
    }
    message.reactions.retain(|r| r.count > 0);
}
pub fn vote(message: &mut Message, option: usize) {
    if let Some(p) = &mut message.poll {
        if !p.closed && option < p.options.len() {
            if let Some(old) = p.selected {
                p.options[old].votes = p.options[old].votes.saturating_sub(1);
            }
            p.options[option].votes += 1;
            p.selected = Some(option);
        }
    }
}
