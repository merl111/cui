const std = @import("std");
pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const binding = b.createModule(.{ .root_source_file = b.path("bindings/zig/cui.zig"), .target = target, .optimize = optimize });
    binding.addIncludePath(b.path("include"));
    const example = b.option([]const u8, "example", "Entry point: gallery, hello, mail, or bindings") orelse "gallery";
    const source = if (std.mem.eql(u8, example, "gallery")) "examples/zig/gallery.zig" else if (std.mem.eql(u8, example, "hello")) "examples/zig/hello.zig" else if (std.mem.eql(u8, example, "drawing")) "examples/zig/drawing.zig" else if (std.mem.eql(u8, example, "mail")) "examples/zig/mail.zig" else if (std.mem.eql(u8, example, "bindings")) "tests/zig_bindings.zig" else @panic("Unknown example; use gallery, hello, mail, or bindings");
    const module = b.createModule(.{ .root_source_file = b.path(source), .target = target, .optimize = optimize, .link_libc = true });
    module.addImport("cui", binding);
    module.addIncludePath(b.path("include"));
    module.addCSourceFiles(.{ .files = &.{ "src/cui.c", "src/cui_layout.c", "src/cui_layouts.c", "src/cui_layout_algorithms.c", "src/cui_icons.c", "src/cui_draw.c", "src/cui_raster.c", "src/cui_controls.c", "src/cui_components.c", "src/cui_patterns.c", "src/cui_pattern_collections.c", "src/cui_pattern_filters.c", "src/cui_insights.c", "src/cui_desktop.c", "src/cui_files.c", "src/cui_keys.c", "src/cui_feedback.c", "src/cui_choices.c", "src/cui_search.c", "src/cui_tokens.c", "src/cui_navigation.c", "src/cui_breadcrumbs.c", "src/cui_inputs.c", "src/cui_datetime.c", "src/cui_tables.c" }, .flags = &.{"-std=c11"} });
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
            module.addCSourceFile(.{ .file = b.path("src/cui_win32_draw.c"), .flags = &.{ "-std=c11", "-DUNICODE", "-D_UNICODE" } });
            module.addCSourceFile(.{ .file = b.path("src/cui_win32_files.c"), .flags = &.{ "-std=c11", "-DUNICODE", "-D_UNICODE" } });
            module.addCSourceFile(.{ .file = b.path("src/cui_win32_keys.c"), .flags = &.{ "-std=c11", "-DUNICODE", "-D_UNICODE" } });
            module.addCSourceFile(.{ .file = b.path("src/cui_win32_pickers.c"), .flags = &.{ "-std=c11", "-DUNICODE", "-D_UNICODE" } });
            module.addCSourceFile(.{ .file = b.path("src/cui_win32_tables.c"), .flags = &.{ "-std=c11", "-DUNICODE", "-D_UNICODE" } });
            module.addCSourceFile(.{ .file = b.path("src/cui_win32_desktop.c"), .flags = &.{ "-std=c11", "-DUNICODE", "-D_UNICODE" } });
            module.addCSourceFile(.{ .file = b.path("src/cui_win32_icons.c"), .flags = &.{ "-std=c11", "-DUNICODE", "-D_UNICODE" } });
            module.addCSourceFile(.{ .file = b.path("src/cui_win32.c"), .flags = &.{ "-std=c11", "-DUNICODE", "-D_UNICODE" } });
            for ([_][]const u8{ "comctl32", "gdiplus", "user32", "gdi32", "dwmapi", "advapi32", "ole32", "oleacc", "shell32", "uuid", "comdlg32" }) |lib| module.linkSystemLibrary(lib, .{});
        },
        else => @panic("CUI supports Linux, macOS, and Windows"),
    }
    const exe = b.addExecutable(.{ .name = b.fmt("cui_zig_{s}", .{example}), .root_module = module });
    if (target.result.os.tag == .windows) exe.win32_manifest = b.path("examples/windows.manifest");
    b.installArtifact(exe);
    const run = b.addRunArtifact(exe);
    b.step("run", "Run the selected native Zig example").dependOn(&run.step);
}
