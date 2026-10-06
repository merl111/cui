# Icons and vector assets

CUI icons are reusable assets, independent of windows and controls. Import SVG artwork, load PNG/JPEG files with the operating system, supply RGBA pixels, or build vector geometry directly. The small built-in symbol collection is optional. No icon font, browser, or bundled rendering library is required.

## Import an SVG icon

The build-time compiler uses Python's standard library. It converts SVG geometry into a portable binary asset; the application loads that asset without needing Python. Compile once and ship the same bytes on Linux, Windows and macOS. Keep the source SVG in your project so designers can edit it.

```sh
python tools/compile_icons.py examples/assets/icons/play.svg play.cuiicon
```

Use `viewBox` coordinates and `fill="currentColor"` or `stroke="currentColor"` when the icon should follow its control's foreground. Explicit colors preserve multicolor artwork. Vector geometry is rendered at the current display scale, including fractional scale and 4K displays. A requested size of 24 means 24 logical units, rather than 24 backing pixels.

The importer supports paths (`M L H V C S Q T A Z`, absolute and relative), rectangles with rounded corners, circles, ellipses, lines, polygons, polylines, nested groups, affine transforms, solid colors, per-paint opacity, nonzero/even-odd fill, and stroke width, caps and joins. Curves remain vector curves; elliptical arcs become cubic segments. Fractional opacity on `currentColor` paint is currently rejected; explicit RGBA colors support opacity. Native antialiasing is used by Cairo, AppKit and Windows GDI+.

This is an icon asset importer, not a general SVG document renderer. Text, CSS stylesheets, gradients, filters, clipping, masks, patterns, external resources, `<use>`, group opacity, dash arrays and nonuniformly transformed strokes are rejected with an error. Export text and strokes as paths and flatten unsupported artwork before importing. Nothing is silently omitted or rasterized. Runtime SVG parsing is not required; runtime asset loading uses the validated `CUIICON1` representation.

## C: lifetime and native controls

```c
cui_icon_asset *play = cui_icon_load("play.cuiicon");
if (!play) { /* Report an invalid asset or failed allocation. */ return; }
cui_widget *button = cui_icon_button(root, play, "Play track");
cui_set_icon_size(button, 24);
cui_set_role(button, CUI_ROLE_PRIMARY);
cui_on_action(button, play_track, state);
cui_icon_release(play); /* The button retains its own reference. */
```

Creation copies all source bytes and commands. Assets have their own reference count; they are not owned by an app. Every widget retains its assigned asset, releasing it when replaced or when the app is destroyed. Release your reference after attaching, or retain it to reuse later. `cui_get_icon` returns a borrowed reference; call `cui_icon_retain` to keep it independently. All calls belong on the UI thread. There are no asset callbacks or finalizers.

Use `cui_icon` for a standalone view and `cui_icon_button` for an icon-only native button. The latter requires a nonempty accessible label. `cui_get_text` and `cui_set_text` read and change that label; an empty replacement is rejected. Click, keyboard activation, disabled state, focus rings, tooltip and accessibility remain part of the native control.

Attach a leading icon to an ordinary button or toggle with `cui_set_icon`. Switch between an icon-only control and a labeled control with `cui_set_icon_only`. This also allows a checkable repeat/shuffle button to retain its native toggle semantics. `cui_set_icon(widget, NULL)` clears its artwork. Setters are silent and do not invoke the action callback.

Icon size is 1–512 logical units, default 20. The view box is fitted proportionally into that square. Icons do not scale with the separate text-scale preference; use a larger logical size if the application's accessibility settings require larger artwork. Controls remain independently focusable and retain their accessible names.

## Python

```python
with cui.Icon.load('play.cuiicon') as play:
    button = root.icon_button(play, 'Play track')
    button.icon_size(24)
    button.on_action(lambda _: start_playback())
# button still owns a reference

with cui.Icon.image('album.png') as cover:
    root.icon(cover).icon_size(128)

repeat = root.toggle('Repeat track', False)
repeat.set_icon(cui.ICON_REPEAT)  # optional symbol convenience
repeat.icon_only()
```

`Icon.vector`, `Icon.rgba`, `Icon.decode` and `Icon.symbol` cover in-memory sources. Use a context manager or `close()` for every owned asset. `get_icon()` returns a new owned reference. Closed asset objects reject further use. The Cadence example keeps imported play/pause assets in an `ExitStack` for the application's lifetime and switches between them as playback changes.

## Go

```go
play, err := cui.LoadIcon("play.cuiicon")
if err != nil { return err }
defer play.Close()
button := root.IconButton(play, "Play track")
button.IconSize(24)
button.OnAction(func(cui.Widget) { startPlayback() })
```

`NewVectorIcon`, `NewRGBAIcon`, `LoadImageIcon`, `DecodeIcon` and `NewSymbolIcon` create owned assets. Call `Close` on the main UI thread. Widgets retain their references; `GetIcon` returns a new owned reference. `SymbolButton` and `SetSymbol` are conveniences for the built-in collection. Continue using `runtime.LockOSThread` before creating your app.

## Zig

```zig
const play = try ui.Icon.load("play.cuiicon");
defer play.deinit();
const button = try root.iconButton(play, "Play track");
_ = button.iconSize(24);
button.onAction(playTrack, state);
```

`ui.Icon.vector`, `rgba`, `image`, `decode` and `symbol` cover other sources. Assets use explicit `deinit`; avoid copying and independently freeing the same reference. Use `retain` when another owner needs a reference. `getIcon` returns an owned reference. `symbolButton` offers the built-in convenience. The complete C command structure is available as `ui.c.cui_icon_command`.

## Programmatic vectors and raster images

A vector asset contains a view-box width and height plus an array of `cui_icon_command`. Commands move, draw lines or cubic Bézier curves, close contours, and fill or stroke. Multiple contours followed by one fill support holes. Paint commands consume the path: repeat its geometry to both fill and stroke it. Each paint uses either the native foreground (`current_color = 1`) or `0xRRGGBBAA`.

`cui_icon_rgba` copies tightly packed, straight-alpha RGBA pixels. Dimensions must be 1–4096 pixels. `cui_icon_load_image` decodes image files through the platform's system loader; use PNG/JPEG for portable raster artwork. Raster icons fit proportionally but cannot invent detail: provide enough pixels for the largest intended display scale. Use vectors for interface symbols.

`cui_icon_vector`, `cui_icon_decode` and `cui_icon_load` validate command order, coordinate bounds, sizes and truncated input. Invalid input returns NULL; it does not change an already attached asset. Bindings turn creation failures into their usual errors. Limit imported assets to 65,536 commands and SVG source to 4 MiB.

## Validation and examples

The native icon integration tests cover ownership, source copying, replacing and clearing icons, accessible labels, normal/toggle/icon-only controls, disabled activation, raster rendering and 4K sizing. Compiler tests exercise curves, arcs, transforms, colors, decoding and rejection of unsupported features. Linux rendering is verified; Windows and macOS implementations are present, with native verification deferred.

Run the component examples with `./build/cui_showcase icon` and `./build/cui_showcase icon-button`. The three app demos use the same public API, without private drawing hooks.

## Rust

`cui::Icon` uses RAII ownership: Clone retains, Drop releases, and controls keep their own reference. `Widget::get_icon` returns an independently retained `Option<Icon>`. `load` reads compiled vector files; `decode` supports embedded bytes; `vector` accepts path commands; `rgba` copies a checked pixel slice; `image` uses the system raster decoder. No Cargo image or SVG package is required.

```rust
let icon = cui::Icon::load("assets/play.cuiicon")?;
let button = root.icon_button(Some(&icon), "Play preview")?;
assert!(button.set_icon_size(24)?);
assert!(button.set_icon_only(true)?);
drop(icon);
```

The [Rust gallery](../../bindings/rust/examples/gallery.rs) embeds compiled artwork with `include_bytes!` and switches its play/pause asset from a native callback. See [the Rust guide](rust.md) for setup and complete lifetime rules.

Settings/navigation symbols also include `CUI_SYMBOL_SETTINGS`,
`CUI_SYMBOL_PERSON`, `CUI_SYMBOL_SUN` and `CUI_SYMBOL_BELL`. These are scalable,
stroke-based vector assets with current-color tinting, available through the
same C, Rust, Go, Python and Zig icon APIs. Use explicit icon sizes and a larger
button hit area rather than enlarging the glyph to fill the button.
