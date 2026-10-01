# Waypoint: Rust Simulator Studio

Status: implemented as a native phone window and an attached, initially hidden inspector on Linux. [Drawing API and binding guide](../guides/drawing.md). Native Windows/macOS execution remains deferred.

## Intended result

A polished desktop Rust application that resembles an iOS Simulator workspace: a detailed phone frame, an interactive map on its screen, and a compact floating inspector for device and location controls. All interactions operate on local sample data. It does not run iOS binaries, connect to Xcode or require a Mac, account, map service or API key. Native Windows/macOS verification remains a separate later phase.

The primary reference is the [SwiftLee simulator screenshot](https://www.avanderlee.com/wp-content/smush-webp/2023/03/simulator_gps_location_access-980x1024.jpg.webp), from [Location Simulation in Xcode’s Simulator](https://www.avanderlee.com/workflow/location-simulation-xcode-simulator/). It shows a dark iPhone frame and map, a detached toolbar above the phone, a translucent red/brown inspector on its right, and warm orange/purple wallpaper visible behind the panels. Additional interaction reference: [RocketSim location simulation](https://www.rocketsim.app/docs/features/app-actions/location-simulation/).

Use these for layout, proportions, hierarchy and material behavior. Draw original map geometry, wallpaper, icons and app identity; do not ship the reference screenshot or Apple Maps tiles as app content. App name: **Waypoint / Rust**.

## Visual and interaction design

- An undecorated iPhone-shaped window and a closable, anchored controls panel opened from Settings. The user’s desktop remains visible between them.
- Dark map artwork with crisp labeled streets, parks/water, a blue location marker and an accuracy ring. Map zoom/pan and position selection work against a small bundled fictional map.
- A compact inspector with independent background tint/alpha so text and controls stay fully legible. Sections cover device controls, saved locations, route playback, permissions and simulated notifications.
- Real icon buttons for play/pause, restart, rotate, zoom and screenshot-preview capture. Accessible names and keyboard focus are required.
- Saved sample locations change the marker and labels. A local route interpolates between waypoints with adjustable speed; pause/resume/reset update the same model. Add/remove locations, precision toggling and permission choices update the phone and inspector consistently.
- A permission sheet can be shown on the phone; allowing/denying location and switching precise/approximate position have visible consequences. A notification action displays an original in-phone notification mock.
- The portrait phone body is approximately 323×674 logical pixels. Its window is 347×774 including a draggable toolbar and transparent margins; the controls window is 382×696. Four corner grips resize the phone uniformly, and the scale button restores 100%. Rotation swaps the device dimensions. Both are tested at 2× and enlarged text.

## Existing pieces to reuse

`bindings/rust` already exposes the full C API with checked handles, closures, timers, commands, models, fonts and owned Icon assets. Reuse its lifecycle and callback ownership rather than adding a Rust renderer. `cui_icon_vector`, `cui_icon_rgba` and the SVG importer provide asset input. Existing layout primitives can build the inspector, but they do not expose general overlapping layers or rounded screen clipping.

The renderer is split between `src/cui_gtk.c` / `src/cui_gtk_controls.c`, `src/cui_macos.m`, and `src/cui_win32.c` / `src/cui_win32_icons.c`. The new `cui_draw.h` adds surfaces, canvas regions and opacity. The shared renderer in `src/cui_raster.c` handles composition; small native adapters rasterize text and icons.

## Rendering contract before demo implementation

Opacity and blur are different features. Reducing opacity blends foreground with background; it does not blur the background. Reducing an entire panel's opacity also fades its text. The reference needs a translucent background material behind opaque content, plus clipping and shadows.

1. Specify straight RGBA inputs and source-over compositing, converting to premultiplied pixels at native boundaries where required. Distinguish primitive paint alpha, group opacity and material background alpha.
2. Define widget/group opacity in the range 0..1, default 1. Reject NaN/infinity/out-of-range values without changing existing state. Define nested opacity multiplication and group compositing once per group, rather than fading each overlapping child independently. Opacity must not silently alter layout, enabled state, focus or hit testing.
3. Add a reusable canvas/surface interface for rectangles, rounded rectangles, paths, images and text; clipping, transforms, save/restore, logical coordinates and pointer/keyboard events are needed for the phone/map. Include a semantic/accessibility interface for interactive custom regions.
4. Add layered layout with explicit z-order, clipping and event targeting. A translucent inspector must not accidentally forward clicks through interactive controls. Transparent decoration should be explicitly noninteractive.
5. Define background material separately: tint, alpha and optional backdrop blur. Provide capability reporting for effects that cannot be rendered. An opaque or tinted fallback must be intentional and documented; do not claim that a transparent rectangle provides blur.
6. Render the phone and panel in separate native windows, each with its own surface. Close/reopen the controls without destroying state. Provide custom frame, resize, visibility, position and pointer-driven move APIs in every binding. Material blur samples app-owned pixels; it does not sample desktop wallpaper.

The low-level draw/context API should live in its own public header and implementation modules. Borrow paint contexts only during callbacks; retain images/icons explicitly; limit offscreen surface allocation dimensions; release caches when scale/size changes. No third-party rendering engine or bundled font is introduced.

## Ordered implementation

1. Finish the safety checks and retain regressions for allocation failure, asset ownership and tree-model teardown. Add equivalent resource-lifetime tests for new surface/context resources before exposing them.
2. Write the C drawing, layering and opacity contracts, then implement shared validation/state. Add the native backend hooks, with GTK as the first executed backend. Investigate AppKit/CoreGraphics and Win32 system composition paths before claiming equivalent support; individual Win32 child controls cannot simply be alpha-blended like canvas shapes.
3. Implement clipping, alpha composition, text/image drawing and hit testing. Add deterministic pixel tests for 0/0.5/1 opacity, overlapping children, nested groups, transparent pixels and rounded clipping at 1×/2×. Test invalid values, repeated resizing and teardown under sanitizers.
4. Update **all four bindings** in the same change, including Rust RAII for any owned resources, callback lifetimes and drawing-context lifetimes. Regenerate raw Rust declarations; extend ABI, coverage and examples checks.
5. Build `bindings/rust/examples/simulator.rs` around separate local device, location, route and permission state. First implement the complete interactive flow using the new public API, then refine typography, spacing, shadows and material appearance against the reference.
6. Add native interaction checks: saved-location selection, add/delete, zoom/pan, route tick interpolation, pause/resume/reset, permission outcomes, precision change, rotation and notification dismissal. Make automated playback deterministic and keep timing/network variability out of the checks.
7. Inspect light/dark appearances and 4K/large-text captures. Verify focus order, icon labels and a reduced-transparency presentation with readable text. Run lifecycle/ASan/UBSan/leak checks for repeated scene rebuilds.
8. Publish it as the fourth app demo in `docs/site/apps.json`, with original native screenshots, a walkthrough, runnable Rust source, build commands and honest mock/capability limits. Update the docs checker to derive expected languages/counts from the app manifest, then refresh the homepage, examples and agent reference.

## Completion criteria

A runnable Rust native mock with the described working controls, demonstrated compositing through a reusable C API, complete bindings and docs, sharp native captures, passing pixel/interaction/lifetime tests, and explicit platform evidence. A static screenshot, web imitation, global window fade or prerecorded map video does not satisfy the demo.

## Implementation boundaries

Waypoint now runs from `bindings/rust/examples/simulator.rs` and appears as the fourth native app in the website. Composition uses a shared CPU renderer with native font and icon rasterization, rather than introducing a GPU dependency. Group opacity, rounded clips, shadows and app-owned backdrop blur work on surfaces. GTK native widgets support opacity; AppKit leaf views have source support, and Win32 ordinary child controls reject fractional opacity. The capability guide documents limits and ownership.

Each surface is created at its window’s initial scale. Phone rotation recreates its surface and resizes the actual native window. Monitor scale changes still require surface recreation. Explicit placement works on X11/Win32/AppKit; Wayland leaves placement to the compositor. Drag the phone toolbar or panel title area; the backend handles native movement. The mock has a fixed layout, single route, up to 32 locations, local notification and in-memory preview. It does not run iOS code, obtain real GPS coordinates, save screenshots to disk or integrate with Apple Simulator.

## Device geometry

The iPhone 16 Pro frame derives its ratio from Apple’s [71.5 × 149.6 mm dimensions](https://support.apple.com/en-au/121031). At a 674 logical-pixel height its width is approximately 322.1, replacing the earlier 346-pixel frame. The display independently follows 1206 × 2622 pixels at 460 ppi, centered inside the body. Landscape swaps those proportions rather than stretching the portrait frame. Unit tests guard both ratios. This is proportional illustration, not a certified hardware/CAD model; small bezel and cutout details are stylized.

## Two-window verification

`tools/check_simulator_windows.py` uses system XTest to click the real native windows. It checks distinct window IDs, hardware aspect ratio, panel close/reopen, Ctrl+I and rotation in both directions at 1× and 2×. Run on a sufficiently large Xvfb screen (1600×1000 at 1×, 3840×2160 at 2×). The checks also drag the toolbar, resize from all four corners and reset to 100%. Screenshots include separate phone/panel captures and a real desktop capture showing both.
