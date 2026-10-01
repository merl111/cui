const std = @import("std");
const ui = @import("cui");
extern "c" fn getenv([*:0]const u8) ?[*:0]const u8;
const Demo = struct {
    app: ui.App,
    canvas: ui.Widget,
    surface: ui.Surface,
    active: bool = false,
    fn paint(self: *Demo, partial: bool) void {
        var gradient = ui.paintRect(.{ 0, 0, 520, 300 }, 0, 0x273957ff);
        gradient.op = ui.c.CUI_DRAW_GRADIENT;
        gradient.color2 = 0x754333ff;
        var ellipse = ui.paintRect(.{ 180, 0, 280, 250 }, 0, 0xfb7749cc);
        ellipse.op = ui.c.CUI_DRAW_ELLIPSE;
        var shadow = ui.paintMaterial(.{ 40, 48, 440, 210 }, 24, 18, 0x00000080);
        shadow.op = ui.c.CUI_DRAW_SHADOW;
        var layer = std.mem.zeroes(ui.c.cui_draw_command);
        layer.op = ui.c.CUI_DRAW_LAYER;
        layer.p[0] = 0.8;
        var end = std.mem.zeroes(ui.c.cui_draw_command);
        end.op = ui.c.CUI_DRAW_END_LAYER;
        var clear = std.mem.zeroes(ui.c.cui_draw_command);
        clear.op = ui.c.CUI_DRAW_CLEAR;
        clear.color = 0x273957ff;
        const commands = [_]ui.c.cui_draw_command{ clear, gradient, ellipse, shadow, ui.paintMaterial(.{ 40, 40, 440, 210 }, 24, 16, 0x162234bb), ui.paintText(.{ 66, 67 }, 385, 24, 600, "A custom control", 0xffffffff), layer, ui.paintRect(.{ 66, 130, 220, 62 }, 16, if (self.active) 0x559cffff else 0xffffff22), ui.paintTextBox(.{ 66, 130, 220, 62 }, 18, 400, if (self.active) "Selected" else "Select this card", 0xffffffff, ui.c.CUI_DRAW_ALIGN_CENTER), end };
        std.debug.assert(if (partial) self.surface.renderRegion(&commands, .{ 65, 129, 222, 64 }) else self.surface.render(&commands));
        std.debug.assert(self.canvas.canvasSurface(self.surface));
    }
};
fn action(_: ?*ui.c.cui_widget, event: [*c]const ui.c.cui_canvas_event, data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));
    if (event.*.kind == ui.c.CUI_CANVAS_ACTIVATE and event.*.id == 1) {
        demo.active = !demo.active;
        demo.paint(true);
    }
}
fn check(data: ?*anyopaque) callconv(.c) void {
    const demo: *Demo = @ptrCast(@alignCast(data.?));
    std.debug.assert(demo.canvas.canvasActivateRegion(1) and demo.active);
    std.debug.assert(demo.canvas.canvasActivateRegion(1) and !demo.active);
    var width: c_int = 0;
    var height: c_int = 0;
    var bytes: [0]u8 = .{};
    std.debug.assert(demo.surface.read(&bytes, &width, &height) == @as(usize, @intCast(width * height)) * 4);
    demo.app.quit();
}
pub fn main() !void {
    const app = try ui.App.init();
    defer app.deinit();
    const window = try app.window("Drawing · Zig", 560, 360);
    const surface = try ui.Surface.init(520, 300, window.scale());
    defer surface.deinit();
    var demo = Demo{ .app = app, .canvas = try window.root().canvas(), .surface = surface };
    demo.canvas.expand(true);
    const regions = [_]ui.c.cui_canvas_region{.{ .id = 1, .x = 66, .y = 130, .width = 220, .height = 62, .label = "Toggle card selection", .enabled = 1 }};
    std.debug.assert(demo.canvas.canvasRegions(&regions));
    demo.canvas.onCanvasEvent(action, &demo);
    demo.paint(false);
    if (getenv("CUI_SMOKE_TEST") != null) _ = try app.every(100, check, &demo);
    window.show();
    app.run();
}
