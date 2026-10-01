# Appearance, layout and 4K

CUI aims for native behavior with deliberate visual defaults. Design with system fonts, semantic roles, useful spacing and content-based sizing. The same application structure maps to each platform's conventions rather than requiring a bundled renderer or bitmap font atlas.

## Semantic appearance

Use TITLE, HEADING and CAPTION to express text hierarchy. PRIMARY identifies the main button; CARD groups content. SUCCESS, WARNING, DANGER and SUBTLE communicate status. Keep a text label alongside a status color so meaning survives high-contrast themes and color-vision differences.

`CUI_ROLE_AMBIENT` gives a container a decorative multicolor background that follows the app's light/dark theme. Put readable content in a nested CARD; the background carries no semantic information and yields to high contrast. Native gradients require no images, shaders or extra dependencies. C, Rust (`sys::CUI_ROLE_AMBIENT`), Go (`Ambient`), Python (`AMBIENT`) and Zig (`c.CUI_ROLE_AMBIENT`) expose the same role.

GTK application button/label styling is scoped to the content root. Native title-bar controls retain the desktop theme, dimensions and behavior; they are not application buttons.

Select system, light or dark appearance with `cui_app_set_theme`. Linux uses scoped GTK styling; macOS uses AppKit appearance and semantic colors; Windows uses system APIs with native controls and selected custom drawing. Windows is not a WinUI 3 backend and currently does not offer Mica or Acrylic.

## Custom-drawn surfaces and system appearance

Use `cui_app_resolved_theme(app)` to resolve the effective light/dark appearance on the UI thread. It returns `CUI_THEME_LIGHT` or `CUI_THEME_DARK`, including when the application follows `CUI_THEME_SYSTEM`. A null application returns light. It does not mutate the selected preference or override accessibility settings.

The convenience names are Rust `app.resolved_theme()`, Python `app.resolved_theme()`, Go `app.ResolvedTheme()` and Zig `app.resolvedTheme()`. Native widgets update themselves; a custom canvas or chat theme must update its own palette when this value changes. Compare the result in an existing UI timer, update component themes only on a change, and continue to refresh at the window's display scale. Keep the presentation preset unchanged when switching palettes if the layout should stay the same.

App commands also accept `CUI_KEY_ESCAPE`, `CUI_KEY_ENTER` and `CUI_KEY_TAB` in addition to ASCII letter/digit shortcuts. Escape can dismiss an application-owned modal while a pending operation guards against premature dismissal. Disable the background container while showing a modal, provide an accessible close button, and restore focus when it closes.

## Logical dimensions

Widths, heights, gaps and padding use logical units. Windows maps these to 96-DPI units, AppKit uses points, and GTK uses logical pixels. Do not multiply layout sizes by `cui_window_scale`; the backend already applies display scaling. The scale accessor is useful when supplying resolution-dependent content.

The root is a vertical box with default padding 24 and gap 12. A box's expanding children share remaining space along its axis. Native minimums keep controls legible; a window may need scrolling when a user increases font size. Use grid cells for aligned forms, wrapping rows for variable-width choices, and split panes for adjustable work areas.

## Typography

Point sizes are independent of display resolution. `cui_set_font` sets an installed family, 6…200 point size and weight 100…900. Empty family and zero size/weight inherit defaults. Set a container's font to propagate it to existing and future descendants. Use `cui_font_apply` when applying a picker result that includes italic style.

`cui_app_set_text_scale` accepts 0.5…4.0 and changes text scale independently from monitor scaling. At 2× display scale and 1.5× text scale, glyphs still use the platform's native shaping and rasterization. No font files are bundled. Missing scripts and emoji depend on fonts installed on the destination machine.

## Images and charts

Charts use native vector drawing. Images use copied RGBA8 source data, preserve aspect ratio and support sources up to 4096×4096. A low-resolution source image will still look soft when enlarged: supply enough pixels for its intended display size. CUI does not decode image files or download remote assets.

## What to test now

On Linux, resize narrow and wide windows, switch themes, use only the keyboard and increase application text scale. Existing native tests cover 1×/2× rendering, a 3840×2160 virtual display and 150% text at 2×. See the high-DPI guide for detailed evidence.

Windows and macOS native verification is deliberately deferred. Their implementations and documentation continue in parallel. This does not change the distinction between implemented platform code and behavior that has been observed on a native host.
