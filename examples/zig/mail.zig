//! Postbox: a local mail mock. All messages and drafts live only in memory.
const std = @import("std");
const ui = @import("cui");
const c = ui.c;
extern "c" fn getenv([*:0]const u8) ?[*:0]const u8;
const Folder = enum { inbox, sent, archive, trash, drafts };
const folder_names = [_][:0]const u8{ "Inbox", "Sent", "Archive", "Trash", "Drafts" };

const Message = struct {
    contact: [128]u8 = @splat(0),
    subject: [128]u8 = @splat(0),
    body: [2048]u8 = @splat(0),
    folder: Folder = .inbox,
    unread: bool = true,
    starred: bool = false,
    attachment: bool = false,
};
fn copyZ(destination: []u8, source: []const u8) void {
    std.debug.assert(source.len < destination.len);
    @memcpy(destination[0..source.len], source);
    destination[source.len] = 0;
}
fn z(buffer: []const u8) [:0]const u8 {
    const length = std.mem.indexOfScalar(u8, buffer, 0) orelse unreachable;
    return buffer[0..length :0];
}
fn seed(contact: []const u8, subject: []const u8, body: []const u8) Message {
    var message = Message{};
    copyZ(&message.contact, contact);
    copyZ(&message.subject, subject);
    copyZ(&message.body, body);
    return message;
}

const Postbox = struct {
    app: ui.App,
    compose_window: ui.Window = undefined,
    folders: ui.Widget = undefined,
    search: ui.Widget = undefined,
    list: ui.Widget = undefined,
    title: ui.Widget = undefined,
    sender: ui.Widget = undefined,
    body: ui.Widget = undefined,
    status: ui.Widget = undefined,
    star: ui.Widget = undefined,
    reply: ui.Widget = undefined,
    archive: ui.Widget = undefined,
    trash: ui.Widget = undefined,
    restore: ui.Widget = undefined,
    compose: ui.Widget = undefined,
    to: ui.Widget = undefined,
    subject: ui.Widget = undefined,
    draft: ui.Widget = undefined,
    send: ui.Widget = undefined,
    draft_status: ui.Widget = undefined,
    messages: [24]Message = @splat(.{}),
    count: usize = 8,
    filter: ui.Widget = undefined,
    selection_info: ui.Widget = undefined,
    undo: ui.Widget = undefined,
    bulk_archive: ui.Widget = undefined,
    bulk_trash: ui.Widget = undefined,
    mark_read: ui.Widget = undefined,
    resume_button: ui.Widget = undefined,
    save_draft: ui.Widget = undefined,
    attachment: ui.Widget = undefined,
    attachment_info: ui.Widget = undefined,
    forward: ui.Widget = undefined,
    undo_messages: [24]Message = undefined,
    undo_folder: Folder = .inbox,
    undo_current: ?usize = null,
    can_undo: bool = false,
    editing_draft: ?usize = null,
    folder: Folder = .inbox,
    current: ?usize = 0,
    visible: [24]usize = undefined,
    visible_count: usize = 0,
    ready: bool = false,

    fn build(self: *Postbox) !void {
        self.messages[0] = seed("maya@studio.example", "A fresh start for the studio", "Hi Alex,\n\nI've been thinking about the next chapter for the studio.\nA little more space, warmer colors, and room to experiment.\n\nThe first sketches are ready for your thoughts.\nCould we take a look together over coffee on Thursday?\n\nNo rush — good things take a little time.\n\nMaya");
        self.messages[1] = seed("team@fieldnotes.example", "Your weekly field notes", "This week's collection:\n\nSmall spaces. Big ideas.\nA new way to think about your daily routine.\n\nEnjoy the read!");
        self.messages[2] = seed("sam@weekend.example", "Saturday by the lake?", "Hi!\n\nThe forecast looks good for a walk this weekend.\nMeet at the trailhead at ten?\n\nSam");
        self.messages[3] = seed("hello@makers.example", "Welcome to the makers club", "A space for people who like to make things.\n\nYou are in good company. Welcome aboard!");
        self.messages[0].attachment = true;
        self.messages[1].starred = true;
        self.messages[4] = seed("nora@studio.example", "Thursday review · agenda", "Hello team,\n\nThree things for our Thursday review:\n1. Brand direction\n2. Landing page typography\n3. Next month's launch\n\nBring your notes and a fresh perspective.\n\nNora");
        self.messages[4].attachment = true;
        self.messages[5] = seed("orders@paper.example", "Your paper samples are ready", "Your warm white and textured paper samples are ready.\n\nCollect them at the studio reception any time this week.");
        self.messages[6] = seed("alex@local.example", "Project outline", "A first draft of the autumn project.\n\nGoals, milestones and a little room to experiment.");
        self.messages[6].folder = .drafts;
        self.messages[7] = seed("maya@studio.example", "Last week's decisions", "Thanks for the thoughtful conversation last week.\n\nEverything we agreed on is recorded here for later.");
        self.messages[7].folder = .archive;
        const window = try self.app.window("Postbox - Zig mail", 1480, 940);
        window.scrollable(true);
        const root = window.root();
        root.padding(24);
        const header = try root.box(c.CUI_HORIZONTAL, 14);
        (try header.label("Postbox")).role(c.CUI_ROLE_TITLE);
        _ = try header.badge("MAIL / ZIG", c.CUI_ROLE_SUBTLE);
        const layout = try root.box(c.CUI_HORIZONTAL, 22);
        layout.expand(true);
        const side = try layout.box(c.CUI_VERTICAL, 14);
        side.minSize(170, 0);
        (try side.label("YOUR SPACE")).role(c.CUI_ROLE_CAPTION);
        self.compose = try side.button("Compose message");
        self.compose.role(c.CUI_ROLE_PRIMARY);
        self.compose.onAction(composeAction, self);
        self.folders = try side.list(&.{ "Inbox", "Sent", "Archive", "Trash", "Drafts" });
        self.folders.expand(true);
        _ = try side.badge("LOCAL WORKSPACE", c.CUI_ROLE_SUCCESS);
        (try side.label("Studio mailbox\n8 sample conversations")).role(c.CUI_ROLE_CAPTION);
        self.folders.setSelected(0);
        self.folders.onAction(folderAction, self);
        (try side.label("Alex Morgan\nalex@local.example")).role(c.CUI_ROLE_CAPTION);
        const middle = try layout.box(c.CUI_VERTICAL, 12);
        middle.minSize(550, 0);
        (try middle.label("Your messages")).role(c.CUI_ROLE_HEADING);
        self.search = try middle.search("");
        self.search.placeholder("Search this folder");
        self.search.onAction(searchAction, self);
        self.filter = try middle.select(&.{ "All messages", "Unread", "Starred", "With attachments" });
        self.filter.setSelected(0);
        self.filter.onAction(searchAction, self);
        const bulk = try middle.box(c.CUI_HORIZONTAL, 8);
        const all = try bulk.button("Select visible");
        all.onAction(selectAllAction, self);
        self.bulk_archive = try bulk.button("Archive selected");
        self.bulk_archive.onAction(bulkArchiveAction, self);
        self.bulk_trash = try bulk.button("Trash selected");
        self.bulk_trash.onAction(bulkTrashAction, self);
        self.selection_info = try middle.label("Select messages with Ctrl / Shift");
        self.selection_info.role(c.CUI_ROLE_CAPTION);
        self.list = try middle.table(&.{ "Subject", "From", "State" });
        self.list.tableMultiple(true);
        self.list.expand(true);
        self.list.onAction(selectAction, self);
        const reader = try layout.box(c.CUI_VERTICAL, 16);
        reader.expand(true);
        reader.minSize(460, 0);
        self.title = try reader.label("");
        self.title.role(c.CUI_ROLE_HEADING);
        self.sender = try reader.label("");
        self.sender.role(c.CUI_ROLE_CAPTION);
        const actions = try reader.box(c.CUI_HORIZONTAL, 8);
        self.reply = try actions.symbolButton(ui.c.CUI_SYMBOL_REPLY, "Reply");
        self.reply.onAction(replyAction, self);
        self.star = try actions.symbolButton(ui.c.CUI_SYMBOL_HEART, "Star");
        self.star.onAction(starAction, self);
        self.archive = try actions.symbolButton(ui.c.CUI_SYMBOL_ARCHIVE, "Archive");
        self.archive.onAction(archiveAction, self);
        self.trash = try actions.symbolButton(ui.c.CUI_SYMBOL_CLOSE, "Trash");
        self.trash.onAction(trashAction, self);
        self.restore = try actions.symbolButton(ui.c.CUI_SYMBOL_MAIL, "Move to inbox");
        self.restore.onAction(restoreAction, self);
        const secondary = try reader.box(c.CUI_HORIZONTAL, 8);
        self.forward = try secondary.button("Forward");
        self.forward.onAction(forwardAction, self);
        self.mark_read = try secondary.button("Mark unread");
        self.mark_read.onAction(markReadAction, self);
        self.resume_button = try secondary.button("Resume draft");
        self.resume_button.onAction(resumeAction, self);
        self.attachment_info = try reader.label("");
        self.attachment_info.role(c.CUI_ROLE_CAPTION);
        self.body = try reader.textarea("");
        self.body.readOnly(true);
        self.body.expand(true);
        self.undo = try root.button("Undo last mailbox change");
        self.undo.onAction(undoAction, self);
        self.undo.setEnabled(false);
        self.status = try root.label("Local demo · nothing is sent · changes reset when you close the app");
        self.status.role(c.CUI_ROLE_CAPTION);
        try self.buildComposer();
        self.messages[0].unread = false;
        self.refresh();
        window.show();
    }
    fn buildComposer(self: *Postbox) !void {
        self.compose_window = try self.app.window("Postbox - Compose", 680, 620);
        self.compose_window.scrollable(true);
        const root = self.compose_window.root();
        (try root.label("A little note.")).role(c.CUI_ROLE_TITLE);
        _ = try root.label("To (one email address, up to 127 bytes)");
        self.to = try root.entry("");
        _ = try root.label("Subject (up to 127 bytes)");
        self.subject = try root.entry("");
        _ = try root.label("Message (up to 2047 bytes)");
        self.draft = try root.textarea("");
        self.draft.expand(true);
        self.draft_status = try root.label("Saved in this window until you send or quit.");
        self.send = try root.button("Send locally");
        self.send.role(c.CUI_ROLE_PRIMARY);
        self.send.onAction(sendAction, self);
        self.attachment = try root.checkbox("Attach sample: Studio brief.pdf (metadata only)", false);
        self.save_draft = try root.button("Save to Drafts");
        self.save_draft.onAction(saveDraftAction, self);
        const close = try root.button("Keep editing later & close");
        close.onAction(closeComposer, self);
    }
    fn refreshFolders(self: *Postbox) void {
        var labels: [5][64]u8 = undefined;
        var pointers: [5][*:0]const u8 = undefined;
        for (folder_names, 0..) |name, i| {
            var total: usize = 0;
            for (self.messages[0..self.count]) |message| {
                if (@intFromEnum(message.folder) == i) total += 1;
            }
            pointers[i] = (std.fmt.bufPrintZ(&labels[i], "{s}   {d}", .{ name, total }) catch unreachable).ptr;
        }
        std.debug.assert(self.folders.setItems(&pointers));
        self.folders.setSelected(@intCast(@intFromEnum(self.folder)));
    }
    fn matchesFilter(self: *Postbox, message: *const Message) bool {
        return switch (self.filter.selected()) {
            1 => message.unread,
            2 => message.starred,
            3 => message.attachment,
            else => true,
        };
    }
    fn refresh(self: *Postbox) void {
        var query_buffer: [256]u8 = undefined;
        const query_len = self.search.getText(&query_buffer);
        const query = if (query_len < query_buffer.len) query_buffer[0..query_len] else "\x01";
        var pointers: [72][*:0]const u8 = undefined;
        self.visible_count = 0;
        var selection: c_int = -1;
        for (self.messages[0..self.count], 0..) |*message, i| {
            if (message.folder != self.folder or !self.matchesFilter(message)) continue;
            if (std.ascii.indexOfIgnoreCase(z(&message.subject), query) == null and std.ascii.indexOfIgnoreCase(z(&message.contact), query) == null) continue;
            const row = self.visible_count;
            self.visible[row] = i;
            pointers[row * 3] = z(&message.subject).ptr;
            pointers[row * 3 + 1] = z(&message.contact).ptr;
            pointers[row * 3 + 2] = messageState(message).ptr;
            if (self.current == i) selection = @intCast(row);
            self.visible_count += 1;
        }
        if (selection < 0) {
            self.current = null;
            if (self.visible_count > 0) {
                self.current = self.visible[0];
                selection = 0;
            }
        }
        std.debug.assert(self.list.rows(pointers[0 .. self.visible_count * 3], 3));
        self.list.setSelected(selection);
        self.refreshFolders();
        self.updateSelection();
        self.render();
    }
    fn selectedMessages(self: *Postbox, ids: *[24]usize) usize {
        var rows: [24]usize = undefined;
        const count = self.list.tableSelectedRows(&rows);
        var length: usize = 0;
        for (rows[0..@min(count, rows.len)]) |row| {
            const source = self.list.tableSourceRow(row);
            if (source < self.visible_count) {
                ids[length] = self.visible[source];
                length += 1;
            }
        }
        return length;
    }
    fn updateSelection(self: *Postbox) void {
        var ids: [24]usize = undefined;
        const count = self.selectedMessages(&ids);
        var buffer: [80]u8 = undefined;
        self.selection_info.text(std.fmt.bufPrintZ(&buffer, "{d} messages · {d} selected (Ctrl / Shift)", .{ self.visible_count, count }) catch unreachable);
        self.bulk_archive.setEnabled(count > 0 and self.folder != .drafts);
        self.bulk_trash.setEnabled(count > 0);
    }
    fn remember(self: *Postbox) void {
        self.undo_messages = self.messages;
        self.undo_folder = self.folder;
        self.undo_current = self.current;
        self.can_undo = true;
        self.undo.setEnabled(true);
    }
    fn bulkMove(self: *Postbox, folder: Folder) void {
        var ids: [24]usize = undefined;
        const count = self.selectedMessages(&ids);
        if (count == 0) return;
        self.remember();
        for (ids[0..count]) |index| self.messages[index].folder = folder;
        self.current = null;
        self.refresh();
        self.status.text("Selected messages moved locally. Undo restores the whole batch.");
    }
    fn render(self: *Postbox) void {
        const valid = self.current != null;
        for ([_]ui.Widget{ self.reply, self.star, self.archive, self.trash, self.restore, self.forward, self.mark_read, self.resume_button }) |button| button.setEnabled(valid);
        self.archive.setVisible(self.folder != .archive and self.folder != .trash and self.folder != .drafts);
        self.resume_button.setVisible(self.folder == .drafts);
        self.reply.setVisible(self.folder != .drafts);
        self.forward.setVisible(self.folder != .drafts);
        self.trash.setVisible(self.folder != .trash);
        self.restore.setVisible(self.folder == .archive or self.folder == .trash);
        if (self.current) |index| {
            const message = &self.messages[index];
            self.title.text(z(&message.subject));
            self.sender.text(z(&message.contact));
            self.body.text(z(&message.body));
            self.star.text(if (message.starred) "Unstar" else "Star");
            self.mark_read.text(if (message.unread) "Mark read" else "Mark unread");
            self.attachment_info.text(if (message.attachment) "Attachment · Studio brief.pdf · 2.4 MB (sample metadata)" else "No attachments · local conversation");
        } else {
            self.title.text("A little breathing room.");
            self.sender.text("No messages match this folder and search.");
            self.body.text("Choose another folder or clear the search.");
            self.attachment_info.text("");
        }
    }
    fn move(self: *Postbox, folder: Folder) void {
        if (self.current) |index| {
            self.remember();
            self.messages[index].folder = folder;
            self.current = null;
            self.refresh();
            self.status.text("Message moved locally. Use Archive or Trash to find it again.");
        }
    }
    fn sendDraft(self: *Postbox) void {
        var message = Message{ .folder = .sent, .unread = false, .attachment = self.attachment.isChecked() };
        const to_len = self.to.getText(&message.contact);
        const subject_len = self.subject.getText(&message.subject);
        const body_len = self.draft.getText(&message.body);
        if (to_len >= message.contact.len or subject_len >= message.subject.len or body_len >= message.body.len) {
            self.draft_status.text("A field is too long. Shorten it; your draft has been kept.");
            return;
        }
        const to = std.mem.trim(u8, z(&message.contact), " \t\r\n");
        const at = std.mem.indexOfScalar(u8, to, '@');
        if (at == null or at.? == 0 or at.? + 1 == to.len or std.mem.indexOfAny(u8, to, " ,;\t\r\n") != null or std.mem.indexOfScalar(u8, to[at.? + 1 ..], '@') != null) {
            self.draft_status.text("Enter a single email address such as maya@studio.example.");
            return;
        }
        if (std.mem.trim(u8, z(&message.subject), " \t\r\n").len == 0 or std.mem.trim(u8, z(&message.body), " \t\r\n").len == 0) {
            self.draft_status.text("Add both a subject and a message before sending.");
            return;
        }
        if (self.count == self.messages.len and self.editing_draft == null) {
            self.draft_status.text("Demo mailbox is full (24 messages). Restart to reset.");
            return;
        }
        const index = self.editing_draft orelse self.count;
        self.messages[index] = message;
        if (self.editing_draft == null) self.count += 1;
        self.current = index;
        self.editing_draft = null;
        self.can_undo = false;
        self.undo.setEnabled(false);
        self.folder = .sent;
        self.filter.setSelected(0);
        self.search.text("");
        self.refresh();
        self.clearComposer();
        self.draft_status.text("Saved in this window until you send or quit.");
        self.compose_window.close();
        self.status.text("Sent locally. This message appears in Sent; no email was delivered.");
    }
    fn clearComposer(self: *Postbox) void {
        self.to.text("");
        self.subject.text("");
        self.draft.text("");
        self.attachment.setChecked(false);
        self.editing_draft = null;
    }
    fn hasDraft(self: *Postbox) bool {
        var buffer: [2]u8 = undefined;
        return self.to.getText(&buffer) > 0 or self.subject.getText(&buffer) > 0 or self.draft.getText(&buffer) > 0 or self.attachment.isChecked();
    }
    fn saveDraft(self: *Postbox) void {
        if (!self.hasDraft()) {
            self.draft_status.text("Write something before saving a draft.");
            return;
        }
        var message = Message{ .folder = .drafts, .unread = false, .attachment = self.attachment.isChecked() };
        const to_len = self.to.getText(&message.contact);
        const subject_len = self.subject.getText(&message.subject);
        const body_len = self.draft.getText(&message.body);
        if (to_len >= message.contact.len or subject_len >= message.subject.len or body_len >= message.body.len) {
            self.draft_status.text("A field is too long. Shorten it before saving.");
            return;
        }
        if (self.count == self.messages.len and self.editing_draft == null) {
            self.draft_status.text("Mailbox full. Resume an existing draft instead.");
            return;
        }
        if (subject_len == 0) copyZ(&message.subject, "(Untitled draft)");
        const index = self.editing_draft orelse self.count;
        self.messages[index] = message;
        if (self.editing_draft == null) self.count += 1;
        self.current = index;
        self.folder = .drafts;
        self.search.text("");
        self.filter.setSelected(0);
        self.clearComposer();
        self.refresh();
        self.compose_window.close();
        self.can_undo = false;
        self.undo.setEnabled(false);
        self.status.text("Saved in Drafts. Select it and choose Resume draft to continue.");
    }
    fn resumeDraft(self: *Postbox) void {
        if (self.current) |index| {
            if (self.hasDraft()) {
                self.draft_status.text("Save your current draft first, then resume the other message.");
                self.compose_window.show();
                return;
            }
            const message = &self.messages[index];
            self.to.text(z(&message.contact));
            self.subject.text(z(&message.subject));
            self.draft.text(z(&message.body));
            self.attachment.setChecked(message.attachment);
            self.editing_draft = index;
            self.draft_status.text("Editing saved draft. Sending moves this message to Sent.");
            self.compose_window.show();
        }
    }
};
fn demo(data: ?*anyopaque) *Postbox {
    return @ptrCast(@alignCast(data.?));
}
fn composeAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    demo(data).compose_window.show();
}
fn closeComposer(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    demo(data).compose_window.close();
}
fn searchAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    demo(data).refresh();
}
fn folderAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const self = demo(data);
    const row = self.folders.selected();
    if (row < 0 or row >= 5) return;
    self.folder = @enumFromInt(row);
    self.current = null;
    self.refresh();
}
fn selectAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const self = demo(data);
    self.updateSelection();
    const row = self.list.selected();
    if (row < 0) return;
    const source = self.list.tableSourceRow(@intCast(row));
    if (source >= self.visible_count) return;
    self.current = self.visible[source];
    const message = &self.messages[self.current.?];
    message.unread = false;
    _ = self.list.tableSetCell(@intCast(row), 2, messageState(message));
    self.render();
}
fn messageState(message: *const Message) [:0]const u8 {
    if (message.unread) return "Unread";
    if (message.starred) return "Starred";
    if (message.attachment) return "File";
    return "Read";
}
fn selectAllAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const self = demo(data);
    for (0..self.visible_count) |row| _ = self.list.tableSelectRow(row, true);
    self.updateSelection();
}
fn bulkArchiveAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    demo(data).bulkMove(.archive);
}
fn bulkTrashAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    demo(data).bulkMove(.trash);
}
fn saveDraftAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    demo(data).saveDraft();
}
fn resumeAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    demo(data).resumeDraft();
}
fn undoAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const self = demo(data);
    if (!self.can_undo) return;
    self.messages = self.undo_messages;
    self.folder = self.undo_folder;
    self.current = self.undo_current;
    self.can_undo = false;
    self.undo.setEnabled(false);
    self.refresh();
    self.status.text("Restored the previous message state.");
}
fn markReadAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const self = demo(data);
    if (self.current) |index| {
        self.remember();
        self.messages[index].unread = !self.messages[index].unread;
        self.refresh();
    }
}
fn forwardAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const self = demo(data);
    if (self.current) |index| {
        if (self.hasDraft()) {
            self.draft_status.text("Your existing draft is preserved. Save it before forwarding.");
        } else {
            const message = &self.messages[index];
            var subject: [140]u8 = undefined;
            self.subject.text(std.fmt.bufPrintZ(&subject, "Fwd: {s}", .{z(&message.subject)}) catch unreachable);
            self.draft.text(z(&message.body));
            self.attachment.setChecked(message.attachment);
            self.draft_status.text("Add a recipient to forward this sample message.");
        }
        self.compose_window.show();
    }
}
fn starAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const self = demo(data);
    if (self.current) |i| {
        self.remember();
        self.messages[i].starred = !self.messages[i].starred;
        self.refresh();
    }
}
fn archiveAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    demo(data).move(.archive);
}
fn trashAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    demo(data).move(.trash);
}
fn restoreAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    demo(data).move(.inbox);
}
fn sendAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    demo(data).sendDraft();
}
fn replyAction(_: ?*c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const self = demo(data);
    if (self.current) |i| {
        if (self.hasDraft()) {
            self.draft_status.text("Your earlier draft was kept. Send it or clear all fields to start a reply.");
        } else {
            const message = &self.messages[i];
            self.to.text(z(&message.contact));
            var subject: [140]u8 = undefined;
            self.subject.text(std.fmt.bufPrintZ(&subject, "Re: {s}", .{z(&message.subject)}) catch unreachable);
            self.draft.text("Thanks for your note!\n\n");
        }
        self.compose_window.show();
    }
}
fn check(data: ?*anyopaque) callconv(.c) void {
    const self = demo(data);
    std.debug.assert(self.star.activate() and self.messages[0].starred);
    std.debug.assert(self.archive.activate() and self.messages[0].folder == .archive);
    self.folders.setSelected(2);
    folderAction(null, self);
    std.debug.assert(self.current == 0 and self.visible_count == 2);
    std.debug.assert(self.restore.activate() and self.messages[0].folder == .inbox);
    self.folders.setSelected(0);
    folderAction(null, self);
    self.search.text("SATURDAY");
    self.refresh();
    selectAction(null, self);
    std.debug.assert(self.current == 2 and !self.messages[2].unread);
    std.debug.assert(self.trash.activate() and self.current == null);
    self.search.text("");
    self.folders.setSelected(3);
    folderAction(null, self);
    std.debug.assert(self.current == 2);
    std.debug.assert(self.reply.activate());
    self.draft.text("Keep my draft");
    std.debug.assert(self.reply.activate());
    var draft_buffer: [64]u8 = undefined;
    const draft_size = self.draft.getText(&draft_buffer);
    std.debug.assert(std.mem.eql(u8, draft_buffer[0..draft_size], "Keep my draft"));
    const oversized: [128:0]u8 = @splat('x');
    self.subject.text(&oversized);
    std.debug.assert(self.send.activate() and self.count == 8);
    self.subject.text("Re: Saturday by the lake?");
    self.to.text("invalid");
    std.debug.assert(self.send.activate() and self.count == 8);
    self.to.text("sam@weekend.example");
    self.draft.text("See you there!");
    std.debug.assert(self.send.activate() and self.count == 9 and self.folder == .sent);
    std.debug.assert(std.mem.eql(u8, z(&self.messages[8].body), "See you there!"));
    self.search.text("no such message");
    self.refresh();
    std.debug.assert(self.current == null and self.visible_count == 0);
    advancedCheck(self);
    std.debug.assert(self.app.errorText().len == 0);
    std.debug.print("Postbox smoke passed\n", .{});
    self.app.quit();
}
fn advancedCheck(self: *Postbox) void {
    self.search.text("");
    self.folders.setSelected(0);
    folderAction(null, self);
    self.filter.setSelected(3);
    self.refresh();
    std.debug.assert(self.visible_count == 2);
    selectAllAction(null, self);
    std.debug.assert(self.bulk_archive.activate() and self.visible_count == 0);
    std.debug.assert(self.undo.activate() and self.visible_count == 2);
    self.filter.setSelected(0);
    self.refresh();
    self.to.text("friend@local.example");
    self.subject.text("A saved idea");
    self.draft.text("Keep this for tomorrow.");
    self.attachment.setChecked(true);
    std.debug.assert(self.save_draft.activate() and self.folder == .drafts and self.count == 10);
    std.debug.assert(self.resume_button.activate() and self.editing_draft == 9);
    self.draft.text("Ready to share.");
    std.debug.assert(self.send.activate() and self.count == 10 and self.messages[9].folder == .sent);
    std.debug.assert(self.messages[9].attachment);
    std.debug.assert(self.forward.activate());
    var buffer: [2048]u8 = undefined;
    const len = self.draft.getText(&buffer);
    std.debug.assert(std.mem.eql(u8, buffer[0..len], "Ready to share."));
    self.clearComposer();
    self.compose_window.close();
}
fn ready(data: ?*anyopaque) callconv(.c) void {
    const self = demo(data);
    if (!self.ready) {
        self.ready = true;
        std.debug.print("READY\n", .{});
    }
}
pub fn main() !void {
    const app = try ui.App.init();
    defer app.deinit();
    app.theme(c.CUI_THEME_LIGHT);
    if (getenv("CUI_LARGE_TEXT") != null) _ = app.textScale(1.5);
    var self = Postbox{ .app = app };
    try self.build();
    if (getenv("CUI_SMOKE_TEST") != null) _ = try app.every(150, check, &self);
    if (getenv("CUI_CAPTURE") != null) _ = try app.every(700, ready, &self);
    app.run();
}
