const std = @import("std");
pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const binding = b.createModule(.{ .root_source_file = b.path("bindings/zig/cui.zig"), .target = target, .optimize = optimize });
    binding.addIncludePath(b.path("include"));
    const winui_dir = b.option([]const u8, "cui-lib-dir", "Directory containing the MSVC-built WinUI cui.lib and cui.dll") orelse "build/Release";
    const example = b.option([]const u8, "example", "Entry point: gallery, hello, mail, or bindings") orelse "gallery";
    const source = if (std.mem.eql(u8, example, "gallery")) "examples/zig/gallery.zig" else if (std.mem.eql(u8, example, "hello")) "examples/zig/hello.zig" else if (std.mem.eql(u8, example, "drawing")) "examples/zig/drawing.zig" else if (std.mem.eql(u8, example, "mail")) "examples/zig/mail.zig" else if (std.mem.eql(u8, example, "chat")) "tests/chat_bindings.zig" else if (std.mem.eql(u8, example, "bindings")) "tests/zig_bindings.zig" else @panic("Unknown example; use gallery, hello, mail, or bindings");
    const module = b.createModule(.{ .root_source_file = b.path(source), .target = target, .optimize = optimize, .link_libc = true });
    module.addImport("cui", binding);
    module.addIncludePath(b.path("include"));
    if (target.result.os.tag != .windows) {
        module.addCSourceFiles(.{ .files = &.{ "src/cui.c", "src/cui_layout.c", "src/cui_layouts.c", "src/cui_layout_algorithms.c", "src/cui_icons.c", "src/cui_draw.c", "src/cui_chat.c", "src/cui_chat_paint.c", "src/cui_chat_selection.c", "src/cui_chat_compose.c", "src/cui_raster.c", "src/cui_controls.c", "src/cui_components.c", "src/cui_patterns.c", "src/cui_pattern_collections.c", "src/cui_pattern_filters.c", "src/cui_insights.c", "src/cui_desktop.c", "src/cui_files.c", "src/cui_keys.c", "src/cui_feedback.c", "src/cui_choices.c", "src/cui_search.c", "src/cui_tokens.c", "src/cui_navigation.c", "src/cui_breadcrumbs.c", "src/cui_inputs.c", "src/cui_datetime.c", "src/cui_tables.c" }, .flags = &.{"-std=c11"} });
    }
    switch (target.result.os.tag) {
        .linux => {
            module.linkSystemLibrary("m", .{});
            module.addCSourceFiles(.{ .files = &.{ "src/cui_gtk.c", "src/cui_gtk_controls.c", "src/cui_gtk_desktop.c", "src/cui_gtk_files.c", "src/cui_gtk_layouts.c", "src/cui_gtk_navigation.c", "src/cui_gtk_tables.c", "src/cui_gtk_pickers.c", "src/cui_gtk_keys.c", "src/cui_gtk_draw.c" }, .flags = &.{"-std=c11"} });
            module.linkSystemLibrary("gtk4-x11", .{ .use_pkg_config = .force });
            module.linkSystemLibrary("Xext", .{});
        },
        .macos => {
            module.addCSourceFile(.{ .file = b.path("src/cui_macos_draw.m"), .flags = &.{"-fno-objc-arc"} });
            module.addCSourceFile(.{ .file = b.path("src/cui_macos_files.m"), .flags = &.{"-fno-objc-arc"} });
            module.addCSourceFile(.{ .file = b.path("src/cui_macos_pickers.m"), .flags = &.{"-fno-objc-arc"} });
            module.addCSourceFile(.{ .file = b.path("src/cui_macos_desktop.m"), .flags = &.{"-fno-objc-arc"} });
            module.addCSourceFile(.{ .file = b.path("src/cui_macos.m"), .flags = &.{"-fno-objc-arc"} });
            module.linkFramework("AppKit", .{});
            module.linkFramework("QuartzCore", .{});
        },
        .windows => {
            module.addObjectFile(.{ .cwd_relative = b.pathJoin(&.{ winui_dir, "cui.lib" }) });
        },
        else => @panic("CUI supports Linux, macOS, and Windows"),
    }
    const exe = b.addExecutable(.{ .name = b.fmt("cui_zig_{s}", .{example}), .root_module = module });
    if (target.result.os.tag == .windows) exe.win32_manifest = b.path("examples/windows.manifest");
    b.installArtifact(exe);
    if (target.result.os.tag == .windows) {
        for ([_][]const u8{ "cui.dll", "Microsoft.WindowsAppRuntime.Bootstrap.dll" }) |dll| {
            const copy = b.addInstallFileWithDir(.{ .cwd_relative = b.pathJoin(&.{ winui_dir, dll }) }, .bin, dll);
            b.getInstallStep().dependOn(&copy.step);
        }
    }
    const run = if (target.result.os.tag == .windows)
        b.addSystemCommand(&.{b.getInstallPath(.bin, b.fmt("cui_zig_{s}.exe", .{example}))})
    else
        b.addRunArtifact(exe);
    if (target.result.os.tag == .windows) run.step.dependOn(b.getInstallStep());
    b.step("run", "Run the selected native Zig example").dependOn(&run.step);
}
