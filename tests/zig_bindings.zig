const std = @import("std");
const ui = @import("cui");
const c = ui.c;
const expect = std.debug.assert;
var commands: usize = 0;
var cancellations: usize = 0;
fn invoked(_: ?*anyopaque) callconv(.c) void { commands += 1; }
fn cancelled(_: ?*c.cui_dialog, result: c.cui_dialog_result, _: [*c]const u8, _: ?*anyopaque) callconv(.c) void {
    expect(result == c.CUI_DIALOG_CANCELLED); cancellations += 1;
}
const Session = struct { app: ui.App, entry: ui.Widget, menu: ui.Menu, button: ui.Widget };
fn mapped(data: ?*anyopaque) callconv(.c) void {
    const session: *Session = @ptrCast(@alignCast(data.?));
    expect(cancellations == 1);
    expect(session.entry.focus()); expect(session.entry.hasFocus());
    session.menu.popup(session.button);
    session.app.quit();
}
fn checkCommands(app: ui.App, window: ui.Window, root: ui.Widget) !ui.Menu {
    const command = try app.command("Update", 'U', c.CUI_MOD_PRIMARY, invoked, null);
    command.enabled(false); expect(!command.invoke()); command.enabled(true);
    command.checked(true); expect(command.invoke() and commands == 1);
    const menu = try app.menu(); expect(menu.add(command)); expect(menu.separator());
    const child = try app.menu(); expect(menu.submenu("More", child));
    const bar = try app.menu(); expect(bar.submenu("File", menu)); window.menu(bar); window.menu(null);
    _ = try root.toolbar(&.{command});
    const alert = try window.alert("Example", "No file is written.", "Continue", cancelled, null);
    alert.cancel();
    return menu;
}
fn checkLayout(root: ui.Widget) !void {
    const grid = try root.grid(2, 8);
    _ = try (try grid.gridCell(0, 0, 1, 2)).label("Spanning cell");
    const wrapping = try root.wrapping(8); _ = try wrapping.badge("Ready", c.CUI_ROLE_SUCCESS);
    const split = try root.split(c.CUI_HORIZONTAL, 0.4);
    _ = try (try split.splitPane(0)).label("Left"); _ = try (try split.splitPane(1)).label("Right");
    split.setSplitPosition(0.6); expect(@abs(split.splitPosition() - 0.6) < 0.001);
    const disclosure = try root.disclosure("Details", false);
    _ = try (try disclosure.disclosureContent()).label("Expanded content");
    disclosure.setExpanded(true); expect(disclosure.isExpanded());
    const image = try root.image(); expect(!image.rgba(&.{}, 4096, 4096));
    const pixels = [_]u8{255} ** (4096 * 4);
    expect(image.rgba(&pixels, 4096, 1)); expect(!image.rgba(&pixels, 4097, 1));
    const chart = try root.chart(&.{1, 3, 2}); expect(chart.chartValues(&.{4, 2, 6}));
}
fn checkModels(root: ui.Widget) !void {
    const select = try root.select(&.{"One", "Two"}); expect(select.setItems(&.{"Three"}));
    select.setSelected(0); expect(select.selected() == 0);
    const list = try root.list(&.{"One"}); expect(list.setItems(&.{}));
    const toggle = try root.switchControl("Enabled", false); toggle.setChecked(true); expect(toggle.isChecked());
    const number = try root.number(1, 0, 10, 1, 0); expect(number.numberConfigure(0, 20, 0.5, 1));
    expect(number.numberSet(2.5) and number.numberValue() == 2.5);
    const records = try root.pattern(c.CUI_RECORDS_TABLE, null);
    expect(!records.records(&.{"incomplete"})); expect(records.records(&.{"First", "Active", "Detail"}));
    const table = try root.table(&.{"Name", "State"}); expect(table.rows(&.{"First", "Ready"}, 2));
    expect(table.tableEvent().kind == c.CUI_TABLE_NONE);
    const tree = try root.tree(&.{.{.id=1,.parent=0,.text="Root",.expanded=0},.{.id=2,.parent=1,.text="Child",.expanded=0}});
    expect(tree.treeExpand(1, true) and tree.treeIsExpanded(1)); expect(tree.treeEvent().kind == c.CUI_TREE_NONE);
}
pub fn main() !void {
    const app = try ui.App.init(); defer app.deinit();
    expect(app.errorText().len == 0 and app.textScale(1.25));
    expect(ui.monotonicTime() > 0 and std.mem.eql(u8, ui.patternName(c.CUI_CHAT), "Chat"));
    const window = try app.window("Zig wrapper integration", 700, 700); window.scrollable(true);
    expect(window.scale() > 0); window.clipboard("Clipboard · 世界");
    const root = window.root();
    const entry = try root.entry("Hello · 世界"); entry.accessibility("Greeting", "Editable text");
    entry.placeholder("Write a greeting"); entry.tooltip("Your greeting");
    entry.readOnly(true); entry.readOnly(false); entry.undo(); entry.redo();
    var buffer: [64]u8 = undefined; expect(entry.selectedText(&buffer) == 0);
    entry.setEnabled(false); entry.setEnabled(true); entry.setVisible(false); entry.setVisible(true);
    const button = try root.button("Open menu"); const menu = try checkCommands(app, window, root);
    try checkLayout(root); try checkModels(root);
    var session = Session{.app=app,.entry=entry,.menu=menu,.button=button};
    _ = try app.every(100, mapped, &session); window.show(); app.run();
    std.debug.print("Zig convenience wrappers: native state, commands, layout, models and focus passed\n", .{});
}
