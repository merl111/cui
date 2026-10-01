# Custom windows and floating panels

Waypoint uses an iPhone-shaped native window and an attached controls panel. The panel starts hidden and opens from the labeled Settings button in the phone toolbar (or Ctrl/Command+I). Each has its own canvas and surface. Both read and update one application model. Closing the controls hides that window; showing it again preserves its widgets and callbacks.

## C contract

Call these APIs on the application thread. Configure the frame before showing the window. An undecorated window must supply its own close control and draggable area.

```c
cui_window *phone = cui_window_create(app, "Phone", 323, 674);
if (!phone || !cui_window_set_frame(phone, 0, 0, 50)) return;
cui_box_set_padding(cui_window_root(phone), 0);
/* Add your canvas, provide its surface and register input callbacks here. */
cui_window_show(phone);
cui_window_set_position(phone, 120, 90); /* best effort */
```

| API | Behavior |
| --- | --- |
| `cui_window_set_frame(w, decorated, resizable, radius)` | Controls native decorations/resizing and rounded custom frames. Radius is 0..2048 logical pixels; nonfinite values fail. Set radius to zero for ordinary decorated windows. |
| `cui_window_set_size(w, width, height)` | Requests a content size of 1..4096 logical pixels per axis. Native content minimums and window-manager constraints still apply. Resize the canvas minimum and recreate its surface when rotating a device. |
| `cui_window_set_position(w, x, y)` | Best-effort desktop position, with coordinates −32768..32767. Does not show a hidden window. Returns zero on Wayland. |
| `cui_window_get_size(w, &width, &height)` | Reads the actual native content size, in logical pixels. Both outputs must be nonnull. Hidden windows report their requested size. |
| `cui_window_begin_resize(w, corner)` | Starts a native pointer-driven resize on a resizable window. Corners: 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right. Call during a pointer-press callback. |
| `cui_window_set_anchor(panel, parent, x, y, width, height)` | Attaches an undecorated panel beside a parent content rectangle in logical pixels. Both windows must belong to the same app; cycles are rejected. Coordinates are 0..4096 and dimensions 1..4096. Does not open the panel; call `show` explicitly. Update the rectangle when content geometry changes. |
| `cui_window_begin_move(w)` | Call synchronously inside a canvas pointer-press callback to start native dragging. Requires an active pointer press and a window manager. |
| `cui_window_is_visible(w)` | Reports whether the window is shown. `close` hides it; `show` reopens the same window. |

Closing the last visible window quits the event loop. Keep the phone open while its controls are hidden. The application owns both windows until destruction; hiding a window does not release its resources.

## Bindings

| Operation | Rust | Python | Go | Zig |
| --- | --- | --- | --- | --- |
| Custom frame | `window.frame(false, false, 50.)?` | `window.frame(False, False, 50)` | `window.Frame(false, false, 50)` | `window.frame(false, false, 50)` |
| Resize | `window.set_size(674, 323)?` | `window.set_size(674, 323)` | `window.SetSize(674, 323)` | `window.setSize(674, 323)` |
| Position | `window.set_position(120, 90)?` | `window.set_position(120, 90)` | `window.SetPosition(120, 90)` | `window.setPosition(120, 90)` |
| Drag | `window.begin_move()?` | `window.begin_move()` | `window.BeginMove()` | `window.beginMove()` |
| Read size | `window.size()?` | `window.size` | `window.Size()` | `window.size()` |
| Resize drag | `window.begin_resize(3)?` | `window.begin_resize(3)` | `window.BeginResize(3)` | `window.beginResize(3)` |
| Anchor | `panel.anchor(&phone, [12, 88, 323, 674])?` | `panel.anchor(phone, (12, 88, 323, 674))` | `panel.Anchor(phone, [4]int{12, 88, 323, 674})` | `panel.anchor(phone, .{12, 88, 323, 674})` |
| Visibility | `window.is_visible()?` | `window.visible` | `window.Visible()` | `window.visible()` |

These operations report success; check the return value where support matters. Rust additionally validates application lifetime through `Result`. The complete runnable [Rust simulator](../../bindings/rust/examples/simulator.rs) demonstrates shared state, two surfaces, panel closure/reopening, drag regions, rotation and keyboard shortcuts.

## Platform and rendering limits

Linux builds use system `gtk4-x11` and `xext` development libraries. X11 supports explicit positioning and a rounded outer shape, including without a compositor. On Wayland the compositor controls placement; transparent corners depend on native composition. Windows uses native rounded regions; macOS uses an AppKit window with a clipped content layer. Windows/macOS source implementations are present; runtime verification is deferred.

The panel uses alpha transparency on composited Linux desktops, allowing the actual desktop behind it to show through. It does not blur that desktop. Uncomposited X11 has rounded binary clipping but cannot display translucent backgrounds. The demo keeps text and controls opaque, and provides a reduced-transparency preference. Native window shape, surface clipping and group opacity serve separate purposes.

For sharp output, create each surface using its window's scale. Recreate it when dimensions or monitor scale change. Waypoint recreates its surface when the native window size changes, keeps the phone body and display in proportion through uniform scaling, and supports initial 1×/2× scale. Automatic migration between differently scaled monitors remains application work.


## Phone toolbar and resize grips

Waypoint's phone window contains a rounded draggable toolbar above the device, separated by transparent space. The toolbar provides Close, Rotate, Capture, Settings and a scale percentage button that restores 100%. Four small corner grips initiate native resize operations. The phone drawing scales uniformly inside the available space; the toolbar remains legible. The toolbar follows the fitted phone body horizontally and vertically, preserving a 20-logical-pixel gap even after rotation inside a taller native window. Its labeled Settings button opens the inspector. Dragging the toolbar moves the phone and its open inspector together. Reopening the inspector anchors it beside the current phone position.

On GTK, a sole canvas directly inside an unpadded, undecorated window supplies the window silhouette from its alpha channel. Transparent toolbar gaps and outer corners do not receive pointer input. On a composited desktop the fractional pixel alpha is retained for smooth edges; uncomposited X11 falls back to a binary shape. [GDK documents this compositor distinction](https://docs.gtk.org/gdk4/method.Display.is_composited.html). This alpha silhouette integration is currently Linux-specific; Windows/macOS runtime validation remains deferred. The corner-resize and current-size APIs have backend implementations and convenience wrappers for all four languages.

The simulator clips the complete inspector scene to its outer rounded rectangle. Canvas widgets suppress the toolkit's rectangular outer focus outline, while semantic action regions retain their own focus feedback. Map and window-drag regions do not paint a phone-sized border.

## Attached inspector behavior

The reference is RocketSim's [optional side window](https://www.rocketsim.app/docs/settings/side-window/), which supports dynamic and sticky placement beside Apple's Simulator. Its visual attachment does not imply that both are one rectangular OS window. CUI implements the same interaction pattern with its own mock device; it does not integrate with RocketSim or Apple Simulator.

On GTK, `set_anchor` converts the auxiliary window's content into a non-modal `GtkPopover`. This gives Wayland a parent-relative popup instead of relying on global coordinates, which Wayland disallows. Win32 assigns an owner window and tracks movement; AppKit uses a child window. The backend may flip a popup to keep it on screen. The parent and panel are app-owned and must remain in the same application; hide/show retains content. Attached panels use their requested logical content size, independent of texture pixel density. Independent dragging of an attached panel is not supported.

```c
cui_window *panel = cui_window_create(app, "Inspector", 382, 696);
cui_window_set_frame(panel, 0, 0, 23);
cui_box_set_padding(cui_window_root(panel), 0);
/* Add the panel canvas and draw its translucent background and opaque text. */
cui_window_set_anchor(panel, phone, 12, 88, 323, 674);
/* In the Settings action: */
if (cui_window_is_visible(panel)) cui_window_close(panel);
else cui_window_show(panel);
```

### Wayland popup placement

The native popup must remain adjacent to or overlap its parent's native buffer. [GNOME enforces this constraint](https://github.com/GNOME/mutter/blob/main/src/wayland/meta-wayland-xdg-shell.c). The visible gap is bounded by the phone window's transparent padding. CUI clamps anchor rectangles during pending size changes and updates attached canvas panels during native allocation, before committing the resized parent surface. A later timer update alone can allow the compositor to dismiss the popup.

`window_panels_wayland_1x` and `window_panels_wayland_2x` run when system Mutter, Python and `dbus-run-session` are installed. They use an isolated headless compositor and private session bus, wait for the popup to actually map, and verify close/reopen plus growth and shrinkage while open. Run with `ctest --test-dir build --output-on-failure -R window_panels_wayland`. These are development tools, not runtime library dependencies.


## Dialog backdrops and attached popups

For an in-window modal, create a stack base, then `cui_stack_backdrop(stack,
"Close dialog")`, then the dialog's `cui_stack_layer`. The backdrop is a native
button with no visible label; `cui_on_action` receives outside activations.
Disable the base while open, and hide both the backdrop and dialog when closing.
Clicks inside the later dialog layer do not activate the backdrop. Applications
own Escape handling, initial focus, focus restoration, and any pending-operation
dismissal policy. The backdrop uses a translucent black fill by default and accepts
`cui_set_style`. Explicit widget colors and radii use locale-independent CSS on GTK.

For a non-modal picker, use an undecorated CUI window with ordinary child widgets:

```c
cui_window *picker = cui_window_create(app, "Reactions", 404, 126);
cui_window_set_frame(picker, 0, 0, 20);
/* Populate cui_window_root(picker), then show next to the clicked region: */
cui_window_popup_region(picker, timeline_canvas, reaction_region);
```

`cui_window_popup_at(picker, anchor, x, y, width, height)` accepts widget-local
logical coordinates. `cui_window_popup_region` resolves a current enabled canvas
hit region. Invalid, hidden, or disabled anchors are rejected. Native placement
keeps the popup on screen. Outside clicks and Escape dismiss it;
`cui_window_is_visible` observes dismissal. Parent closure also closes attached
children. Reuse the same picker window and call the popup API again to reopen.
Use `cui_window_set_size` followed by the popup API when expanding its content.
Do not use the persistent `cui_window_set_anchor` companion-panel API for a
transient picker that should dismiss on outside interaction.

Convenience wrappers: Rust `Widget::stack_backdrop`, `Window::popup_at` and
`popup_region`; Go `StackBackdrop`, `PopupAt`, `PopupRegion`; Python
`stack_backdrop`, `popup_at`, `popup_region`; Zig `stackBackdrop`, `popupAt`,
`popupRegion`. All languages call the same C implementation.
