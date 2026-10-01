const std = @import("std");
const ui = @import("cui");
extern "c" fn getenv([*:0]const u8) ?[*:0]const u8;

const Demo = struct { app: ui.App, status: ui.Widget, button: ui.Widget };
fn greet(_: ?*ui.c.cui_widget, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));
    demo.status.text("Hello from Zig!");
}
// Optional automated check; uses the same callback as a real click.
fn check(data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));
    std.debug.assert(demo.button.activate());
    var buffer: [64]u8 = undefined;
    const count = demo.status.getText(&buffer);
    std.debug.assert(std.mem.eql(u8, buffer[0..count], "Hello from Zig!"));
    demo.app.quit();
}

pub fn main() !void {
    const app = try ui.App.init();
    defer app.deinit();
    const window = try app.window("Hello from Zig", 480, 280);
    const root = window.root();
    root.padding(24);
    (try root.label("Your first native window")).role(ui.c.CUI_ROLE_TITLE);
    var demo = Demo{ .app = app, .status = try root.label("Ready"), .button = try root.button("Say hello") };
    demo.button.role(ui.c.CUI_ROLE_PRIMARY);
    demo.button.onAction(greet, &demo);
    if (getenv("CUI_SMOKE_TEST") != null) _ = try app.every(100, check, &demo);
    window.show();
    app.run();
}
