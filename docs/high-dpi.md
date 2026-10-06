# High DPI and typography

4K is a monitor resolution, not a font size. CUI separates display scale from application text scale. Layout dimensions are logical units; text uses native shaping/rasterization rather than a low-resolution bitmap atlas.

```c
cui_window_set_scrollable(window, 1);
cui_app_set_text_scale(app, 1.5);              /* 150% text, independent of monitor DPI */
cui_set_font(title, NULL, 24, 600);            /* installed system font, 24 pt, semibold */
cui_set_font(code, "Consolas", 12, 400);       /* use an installed family; platform fallback */
cui_set_font(container, NULL, 14, 0);         /* descendants inherit */
double pixels_per_unit = cui_window_scale(window);
```

Font family strings are copied. NULL/empty family, zero size and zero weight inherit from ancestors or platform defaults. `cui_set_font(widget, NULL, 0, 0)` clears overrides. Font points must be 6–200, weights 100–900, application text scale 0.5–4. Invalid values leave the previous setting unchanged. Setting a container's font updates existing descendants; subsequently created controls inherit it too. Typography roles supply defaults when no explicit font size/weight is inherited.

| Platform | Implementation | Verified here |
| --- | --- | --- |
| Windows | PerMonitorV2 request/manifest, `WM_DPICHANGED`, WinUI scaling and native content measurement | WinUI migration requires Windows compilation, monitor movement and native rendering validation |
| macOS | AppKit logical points and backing-store scaling; system fonts and Cocoa text views | Source implemented; requires a macOS SDK and native runtime validation |
| Linux | GTK/Pango logical layout, native font fallback, desktop font settings, independent app text scale | Real GTK tests at 1×, 2× and a 3840×2160 virtual display; 150% text at 2× and screenshot inspection |

The platform mechanisms follow [Microsoft's DPI change contract](https://learn.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged), [AppKit backing scale](https://developer.apple.com/documentation/appkit/nswindow/backingscalefactor), and [Pango's point-based font units](https://docs.gtk.org/Pango/const.SCALE.html). `cui_window_scale` is informational; application layouts should remain in logical units.

The typography screenshot is a 2160×1800 native-resolution window capture on the 4K virtual display, at 200% display scale and 150% text scale. Tests compare label text extents with actual allocation and ensure the gallery navigation fits the viewport.

Remaining platform validation: mixed-DPI monitor moves, Windows 125/150/175% display scaling, fractional Wayland scaling, macOS Retina/non-Retina moves, fonts unavailable on the current host, complex IME sessions and screen readers. Xvfb coverage does not replace these hardware/desktop tests. Installed fonts determine script and emoji coverage; no universal glyph coverage is promised. Current AppKit custom font weights are mapped to native regular/medium/semibold/bold; not every numeric weight is distinct.

Large content scrolls instead of shrinking controls below their natural sizes. Layout uses rows/columns, grids, wrapping containers and split panes with native minimums. Images accept up to 4096×4096 RGBA pixels and preserve aspect ratio; supply sufficient source resolution for large displays. Charts render with native vector drawing.


Installed fonts can also be selected with `cui_font_dialog` and applied with `cui_font_apply`, including weight and italic style. The selected point size remains a logical size: app text scaling and monitor scaling are applied when rendering. Native picker tests include 2× display scaling and a 150% text preview; macOS and Windows still require verification on their native hosts.

Search results, token removal buttons, feedback text and actions use the same inherited native typography and logical layout units. Their native interaction test runs on a 3840×2160 virtual display at 2× with 150% application text scaling; no bitmap text or bundled fonts are introduced.
