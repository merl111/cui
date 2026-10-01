# Drawing and compositing

Use `cui_draw.h` when a custom control, editor, map or visualization needs its own pixels. A canvas is a native widget that displays an app-owned drawing surface and exposes labeled interaction regions. Ordinary native widgets remain useful for text editing, tables and forms.

[Waypoint, the Rust simulator](../plans/rust-simulator.md) is a complete example. It draws a phone, an interactive map and a translucent inspector using this API. The [C component recipe](../../examples/showcase/cases.inc) and smaller [Python](../../examples/python/drawing.py), [Go](../../bindings/go/cmd/drawing/main.go) and [Zig](../../examples/zig/drawing.zig) examples demonstrate an accessible custom action.

## Ownership and presentation

Create a surface with logical dimensions and a device scale, render a command array, then present it with `cui_canvas_set_surface`. All functions run on the main thread. Surfaces have independent reference counts; widgets retain their assigned surface. Release your reference when finished. App destruction releases the widget's reference.

Rendering changes surface pixels, but does not automatically invalidate widgets. Call `cui_canvas_set_surface` again to display the new frame. Multiple canvases can display the same surface independently. A canvas copies the presented pixels, so a failed subsequent render leaves its displayed frame intact.

Input arrays, strings and icon pointers must be valid until `cui_surface_render` returns. They are not retained by C. Region labels and arrays are copied by `cui_canvas_set_regions`. The event pointer is borrowed only during its callback. As with other CUI callbacks, request app quit in a callback and destroy the app after the event loop returns.

```c
#include "cui_draw.h"

cui_surface *surface = cui_surface_create(520, 300, cui_window_scale(window));
cui_widget *canvas = cui_canvas(cui_window_root(window));
const cui_draw_command commands[] = {
    {.op=CUI_DRAW_CLEAR, .color=0x24334aff},
    {.op=CUI_DRAW_LAYER, .p={0.8}},
    {.op=CUI_DRAW_RECT, .p={24,24,200,70,16}, .color=0x77aaffff},
    {.op=CUI_DRAW_TEXT, .p={42,46,170,18,600},
     .text="A custom control", .color=0x14223aff},
    {.op=CUI_DRAW_END_LAYER}
};
if (surface && canvas && cui_surface_render(surface, commands, 5))
    cui_canvas_set_surface(canvas, surface);
cui_surface_release(surface);
```

Check constructor pointers and integer success results in production code. Invalid parameters, unbalanced stacks and allocation failures return failure. Surface rendering is transactional: the entire previous pixel buffer remains unchanged on failure. Region replacement also preserves the previous list on validation or allocation failure.

## Command reference

Initialize every command field to zero, then set the fields used by its operation. Coordinates and sizes are logical pixels. Colors are straight `0xRRGGBBAA`; for example `0xffffff80` is half-transparent white. Composition is source-over with premultiplied internal storage. `cui_surface_read` exports straight RGBA bytes in top-left row order.

| Operation | `p` parameters | Behavior |
| --- | --- | --- |
| CLEAR | none | Replaces the entire target with `color`; use outside saves, clips and layers. |
| SAVE / RESTORE | none | Saves/restores transform and clipping state. Must balance. |
| TRANSLATE | x, y | Adds a translation in the current coordinate system. |
| SCALE | x, y | Multiplies positive X/Y scales; no rotation or negative scale. |
| CLIP | x, y, width, height, radius | Intersects a rounded rectangle with existing clips. Use SAVE/RESTORE to scope it. |
| LAYER / END_LAYER | opacity for LAYER | Renders children offscreen and composites them once at opacity 0–1. Must balance. |
| RECT | x, y, width, height, radius | Antialiased rounded rectangle using `color`. |
| ELLIPSE | x, y, width, height | Filled antialiased ellipse. |
| LINE | x1, y1, x2, y2, width | Line with round ends. |
| GRADIENT | x, y, width, height, radius | Vertical interpolation from `color` to `color2`. |
| TEXT | x, y, max_width, size, weight, alignment, box_height | UTF-8 single-line text; size in logical pixels, weight 100–900. Optional `font` family; null uses system sans. Truncates with ellipsis. Alignment is LEFT (0), CENTER (1) or RIGHT (2). Positive box_height centers visible glyphs vertically and clips to that box. |
| ICON | x, y, width, height | Fits an existing vector or RGBA icon asset; `color` tints symbolic paints. Arbitrary paths use vector icon commands. |
| SHADOW | x, y, width, height, radius, blur | Blurred rounded rectangle in `color`; offset the rectangle to offset its shadow. |
| MATERIAL | x, y, width, height, radius, blur | Blurs pixels already drawn into this surface, then applies `color` as a tint. |

`LAYER` changes the render target, not transform state. Use balanced SAVE/RESTORE if children should not change subsequent transforms or clips. An already active clip applies while painting children. Blurred materials inside a layer sample that layer, not the outer target. Blur is a separable box blur; the radius is 0–32 logical pixels, capped at 128 device pixels. It samples only your surface: this API does not capture windows or desktop content behind your app.

For group opacity, draw opaque overlapping children within one layer. Their overlap is composited at the layer's opacity once. Painting every child separately at half alpha instead accumulates alpha in overlapping areas.

## Interaction and accessibility

Register up to 256 regions with unique nonzero 32-bit IDs, positive rectangles and nonempty accessible labels. Coordinates use the surface's logical coordinate system. When regions overlap, the last enabled region wins. The native image fits the canvas while preserving aspect ratio; pointer positions are converted through that same fit, including letterboxing.

```c
const cui_canvas_region action = {1,24,24,200,70,"Activate custom control",1};
cui_canvas_set_regions(canvas, &action, 1);
cui_canvas_on_event(canvas, on_canvas_event, userdata);
```

Events are PRESS, RELEASE, MOVE, SCROLL, ACTIVATE and FOCUS. Pointer coordinates are logical pixels, scroll deltas retain platform units, and modifier bits use `cui_modifiers` from `cui_desktop.h`. A press followed by release inside the same current enabled region activates it. Handle dragging in your model if it should suppress a click. Programmatic `cui_canvas_activate_region` and keyboard activation validate the current region and enabled ancestors. Replacing regions in a release callback cannot activate a removed region.

Regions expose native accessibility buttons with their labels. Handle FOCUS to paint your own focus indicator; Enter or Space activates a focused region. Complex custom controls still need app-level semantics: a map region is not automatically an accessible table, slider or editable text field. Use native controls where those richer semantics matter. Windows/macOS accessibility source is implemented but native verification remains deferred.

## DPI, fonts and limits

Use `cui_window_scale` when creating the surface. At 2×, a 520×300 logical surface stores 1040×600 pixels, and the native text rasterizer receives scaled font sizes. Text is rasterized through Pango/Cairo on Linux, AppKit on macOS and Windows' system text APIs. No font or renderer library is bundled.

Surface dimensions and scale are fixed. Recreate and redraw the surface when its logical size or monitor scale changes. The canvas aspect-fits the existing image on resize; it cannot create additional detail from old pixels. Custom text sizes are explicit and do not automatically inherit `cui_app_set_text_scale`: apply the user's text preference to your scene sizes and layout. Waypoint demonstrates 150% text through `CUI_LARGE_TEXT=1`.

Limits: scale 0.25–8, at most 4096 device pixels per axis and 16 million pixels total, 8192 commands, 32 saved states/clips and eight nested layers. Layer storage is additionally capped at 256 MB; transient render/readback and backend buffers consume additional memory. Command scalar values must be finite and within ±1,000,000; transformed scale is bounded to 0.001–1000. Text raster size cannot exceed 512 device pixels. Large full-surface blur and redraws cost CPU time. Cache static artwork and avoid repainting on idle pointer movement.

## Opacity and reduced transparency

`cui_set_opacity(widget, value)` accepts finite values from 0 to 1 and returns whether the operation is supported. It is separate from enabled state: invisible artwork can still have active input regions. Disable controls/regions when needed. `cui_get_opacity` returns the accepted value.

| Target | Linux | macOS source | Windows source |
| --- | --- | --- | --- |
| Surface primitive/group alpha and canvas opacity | Supported | Implemented, verification deferred | Implemented, verification deferred |
| Native widget opacity | GTK widget opacity | Native leaf views; container values other than 1 rejected | Values other than 1 rejected |

Do not ignore a failed native opacity call. Use canvas group composition for translucent custom content. `cui_draw_capabilities` describes renderer features (alpha, groups, blur, native text shaping and regions), not arbitrary native child-window transparency. No whole-desktop blur or WinUI material is implied.

Provide an opaque presentation for reduced transparency. Waypoint's Appearance tab substitutes a solid inspector; this is an explicit app preference, not automatic OS preference synchronization.

## Centered button labels

Use TEXT alignment and box height instead of estimating character widths or adjusting the baseline by eye. The renderer uses native rasterized glyph bounds, including Unicode glyphs, at the current scale. `Scene.text_box` (Rust/Python), `PaintTextBox` (Go) and `paintTextBox` (Zig) expose this directly. Waypoint uses it for centered action labels at normal and enlarged text sizes.

## Python

`Surface` and `Scene` are context managers. A Scene owns text and retained icons until closed; Surface remains independent of App.

```python
with cui.Surface(520, 300, window.scale) as surface:
    with cui.Scene() as scene:
        scene.clear(0x24334aff)
        scene.layer(0.8)
        scene.rect((24, 24, 200, 70), 0x77aaffff, radius=16)
        scene.text((42, 46), 170, 18, 'A custom control', color=0x14223aff, weight=600)
        scene.end_layer()
        surface.render(scene)
    assert canvas.canvas_set_surface(surface)
```

Run the [complete interactive example](../../examples/python/drawing.py):

```sh
PYTHONPATH=bindings/python python3 examples/python/drawing.py
```

## Go

Lock the main OS thread. `Surface.Close` is required. DrawCommand strings are copied to C storage for the render call; keep referenced IconAsset handles open through Render.

```go
surface, err := cui.NewSurface(520, 300, window.Scale())
if err != nil { panic(err) }
defer surface.Close()
err = surface.Render([]cui.DrawCommand{
    {Op: cui.DrawClear, Color: 0x24334aff},
    cui.PaintRect(24, 24, 200, 70, 16, 0x77aaffff),
    cui.PaintText(42, 46, 170, 18, 600, "A custom control", 0x14223aff),
})
if err != nil { panic(err) }
if !canvas.CanvasSetSurface(surface) { panic("canvas upload") }
```

Run the [complete interactive example](../../bindings/go/cmd/drawing/main.go), after building CUI:

```sh
cd bindings/go
go run -buildvcs=false ./cmd/drawing
```

## Zig

Surface is an owned reference. Use `deinit` once per owned handle and `retain` to acquire another. Plain struct assignment does not retain. Command strings are zero-terminated and remain borrowed until render returns.

```zig
const surface = try ui.Surface.init(520, 300, window.scale());
defer surface.deinit();
const commands = [_]ui.c.cui_draw_command{
    ui.paintRect(.{0, 0, 520, 300}, 0, 0x24334aff),
    ui.paintRect(.{24, 24, 200, 70}, 16, 0x77aaffff),
    ui.paintText(.{42, 46}, 170, 18, 600, "A custom control", 0x14223aff),
};
if (!surface.render(&commands)) return error.RenderFailed;
if (!canvas.canvasSurface(surface)) return error.UploadFailed;
```

Run the [complete interactive example](../../examples/zig/drawing.zig):

```sh
zig build -Dexample=drawing run
```

## Rust

Surface drops its native reference; Clone retains it. Scene owns strings and icon references. Widget methods check that App still exists, and callback panics follow the binding's existing error path.

```rust
let surface = Surface::new(520, 300, window.scale()?)?;
let mut scene = Scene::new();
scene.clear(0x24334aff);
scene.layer(0.8);
scene.rect([24., 24., 200., 70.], 16., 0x77aaffff);
scene.text([42., 46.], 170., 18., 600, 0x14223aff, "A custom control")?;
scene.end_layer();
surface.render(&scene)?;
assert!(canvas.canvas_set_surface(&surface)?);
```

Run [Waypoint](../../bindings/rust/examples/simulator.rs):

```sh
CUI_LIB_DIR="$PWD/build" cargo run --offline --manifest-path bindings/rust/Cargo.toml --example simulator
```

## Verification and agent checklist

The drawing C tests check exact group-alpha pixels, nested clips/layers, blur, native text/icons, transactional failure, every allocation failure in a layered blur, repeated surface replacement and native GTK gesture callbacks. The smaller binding examples activate the same callbacks used by real input. Waypoint exercises route controls, location saves/deletion, permission outcomes, orientation, themes and preview closure at 1× and 2×/150% text.

For agents: include `cui_draw.h`; never infer drawing support from the older `cui_image` widget. Do not keep borrowed event pointers or use widget handles after App destruction. Check successful render before presentation. Keep scene construction bounded, give every action an accessible label, recreate surfaces for a new scale, and provide opaque fallback styling. Linux evidence does not establish Windows/macOS runtime correctness.

## Partial redraws and caching

`cui_surface_render_region` replays a **complete scene starting with CLEAR**, then commits only the supplied damage rectangle. Coordinates are logical pixels; bounds round outward to physical pixels. The rectangle must lie inside the surface. Include the old and new bounds of moved content, antialiased edges, shadows, and any backdrop materials affected by the change. The renderer reconstructs neighboring blur inputs automatically; it does not infer damage for your application.

Use a full render for initial presentation, resizing, a new theme/font/scale, or whenever the changed area is uncertain. Failed renders leave the surface unchanged. Damage rendering currently uses a full-size transactional buffer and preserves its exterior, so it reduces raster work but is not a tiled-memory renderer.

```c
/* Initial frame: complete scene, commands[0].op == CUI_DRAW_CLEAR. */
cui_surface_render(surface, commands, count);
/* A button changes; its old/new artwork fits in this logical rectangle. */
if (cui_surface_render_region(surface, commands, count, 65, 129, 222, 64))
    cui_canvas_set_surface(canvas, surface);
```

All language bindings expose the same operation:

```python
surface.render_region(scene, (65, 129, 222, 64))
```

```go
err := surface.RenderRegion(commands, [4]float64{65, 129, 222, 64})
```

```zig
if (!surface.renderRegion(&commands, .{65, 129, 222, 64})) return error.RenderFailed;
```

```rust
surface.render_region(&scene, [65., 129., 222., 64.])?;
```

The complete Python, Go and Zig drawing examples above use partial redraws on activation. Waypoint renders two smaller surfaces for its independent phone and controls windows; see [custom windows](windows.md).

Backdrop blur reuse is automatic. A surface keeps at most **32 MiB of cached blur inputs/results**; cache misses, oversized entries, and optional cache-allocation failures still render correctly. A pending cache may temporarily require another 32 MiB during a render, in addition to frame/layer/scratch buffers. Exact input comparisons prevent stale backdrop reuse. Shadows also check geometry and tint. Cache storage is released with the surface.

GTK canvas submission shares an immutable premultiplied pixel buffer with the native texture. A later render creates a new buffer, so a texture still being displayed cannot observe partially updated pixels. Texture references are thread-safe even if GTK releases them outside the application thread; public CUI calls remain main-thread-only. Windows/macOS use an owned straight-RGBA buffer without the previous extra CUI-side copy. Their runtime verification remains deferred.

Canvas interaction regions retain native controls by ID on updates. Changing a label, rectangle, enabled state, or order does not rebuild unrelated controls. Removed IDs lose focus/press state and cannot be activated.

See [performance measurements](performance.md) for hardware, OS, before/after results, and reproduction commands.
