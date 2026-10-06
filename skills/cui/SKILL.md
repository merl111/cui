---
name: cui
description: Build and integrate native desktop apps with the CUI C99 GUI framework and its Rust, Python, Go or Zig bindings. Use for CUI controls, layouts, desktop APIs, drawing, icons, chat components, build setup or capability questions. CUI here means this native GUI library, not a terminal UI or browser automation API.
---

# CUI native desktop applications

Use CUI's public API and native examples to implement the requested application.
Keep application services and state in the consumer. Shared rendering and reusable
widget behavior live in C, including chat; language bindings adapt the same ABI.

## Locate the source and select a starting point

Find the CUI checkout from the current repository or the consumer's dependency
configuration. Confirm it contains `include/cui.h` and `bindings/`. In this
workspace the usual location is `/home/mathias/source/cui`; treat that as a hint,
not a required path. All repository paths below are relative to the chosen checkout.
The installed skill's own reference links are relative to this skill folder.

Read `README.md` and the relevant public header for current behavior. Read only
the language guide and subsystem guide needed for the request. The library is a
prototype, and source declarations take precedence over stale summaries or plans.
Do not infer binding method names from C names; inspect the wrapper or an example.

For build commands, platform requirements and linking, read
[references/setup.md](references/setup.md). Start with a complete hello/gallery
example in the chosen language, then add the required controls. Bindings are local
packages in the checkout; do not assume a published pip, Cargo or other registry package.

## What CUI can do

| Need | API and repository reference |
| --- | --- |
| Windows, themes, semantic typography, buttons, text inputs, toggles, lists, tabs, charts, images and timers | `include/cui.h`; `docs/guides/getting-started.md`, `docs/high-dpi.md`; `examples/settings.c`, `examples/gallery.c` |
| Rows/columns, grid spans, wrapping, split panes and layered stacks | `include/cui.h`, `include/cui_layouts.h`; `docs/desktop.md`, `docs/guides/chat.md` for stack composition |
| Menus, shared commands, shortcuts, toolbars, dialogs, focus, accessibility labels and anchored popup windows | `include/cui_desktop.h`; `docs/desktop.md`; `examples/desktop.c` |
| Trees, breadcrumbs, numeric/date/time inputs and inline validation | `include/cui_navigation.h`, `include/cui_inputs.h`; `docs/desktop.md` |
| Editable multiselect tables, stable sorting and mapping displayed rows back to source rows | `include/cui_tables.h`; `docs/desktop.md` |
| Searchable pickers/command palettes, token fields, banners, toast regions and empty/error states | `include/cui_search.h`, `include/cui_tokens.h`, `include/cui_feedback.h`; `docs/desktop.md` |
| Higher-level collections, sidebars, filters and insight views | `include/cui_patterns.h`; `docs/components.md`, `docs/guides/pattern-models.md` |
| Vector/RGBA icons, compiled SVG assets, PNG/JPEG loading and icon buttons | Icon declarations in `include/cui.h`; `docs/guides/icons.md`; `tools/compile_icons.py` |
| Custom canvas drawing, text, clipping, layers, alpha, app-owned blur and input regions | `include/cui_draw.h`; `docs/guides/drawing.md`; drawing examples in each language |
| Room navigation, message timelines, composers, threads, reactions, polls, attachments, avatars, inspectors and multi-pane chat workspaces | `include/cui_chat.h`; `docs/guides/chat.md`; `examples/chat/main.c`, `bindings/rust/examples/daylight.rs` |

Use `./build/cui_showcase --list` to discover runnable component IDs and
`./build/cui_showcase button` to inspect one. `docs/components.md` distinguishes
implemented interactions from incomplete compositions; a component's presence
does not establish full parity with a design reference.

## Lifecycle and layout

1. Create one app on the main thread, create a window and get its vertical root.
2. Append controls to valid layout parents in visual/keyboard order. Use nested
   boxes, padding, gaps and expansion before custom drawing. Prefer semantic roles
   such as title, caption, primary and card, plus system/light/dark themes.
3. Register callbacks with context that outlives their use. Check constructors
   and success returns; native initialization, validation and allocation can fail.
4. Show the window and run the event loop. Request quit from a callback when
   necessary, then destroy the app after the loop returns.

The app owns windows, widgets and timers. Closing a window hides it; reopening
reuses its state. There is no per-widget destruction. Reuse widgets and stop/restart
timers instead of repeatedly allocating them for updates. Timer/callback storage
can remain retained until app teardown.

All UI calls belong on the main thread; there is no general cross-thread dispatch
API. Go must lock the main OS thread. Background work should deliver data through
an application queue drained on the UI thread, for example by an app timer.
Do not block event callbacks with network or disk work.

Strings are UTF-8 and geometry is in logical units. Do not multiply ordinary
widget coordinates or popup anchors by display scale. Native control minimums
can exceed requested sizes; use scrolling, flexible space and suitable containers.
Custom surfaces need the backing scale explicitly. Text scaling is independent
of display scaling.

## State, callbacks and ownership

- Ordinary programmatic setters suppress callbacks. User interaction and explicit
  action APIs can emit them; check the individual API (token add/remove is one example).
- Copy borrowed event text or getter results before retaining them beyond their
  documented lifetime. C copies many model inputs, but callback context remains
  caller-owned. Confirm each header's contract rather than assuming all data is copied.
- Tree IDs are unique and nonzero, parents precede children, and parent ID zero
  means root. Table sorting/replacement clears selection; use source-row lookup
  to map table edits back to application records.
- Listen on compound controls such as fields, sidebars and chat roots. Replacing
  callbacks on their internal parts can break forwarding and model management.
- Icon assets and drawing surfaces have independent reference counts. Widgets
  retain attached assets; release the caller's reference separately. Rust handles
  provide RAII; follow Python/Go/Zig close/deinit contracts explicitly.
- After rendering a new surface frame, present it again with
  `cui_canvas_set_surface`; changing pixels alone does not invalidate the canvas.
- Native menu calls can reenter application callbacks before returning. Release
  locks and Rust `RefCell` borrows before opening menus.

## Chat integration

Use the existing C chat components with Nebula, Daylight or Tiles appearance.
Presentation geometry, command sets and theme colors can be configured separately.
The Rust Daylight example is the starting composition for the Archaic consumer;
it uses the same C implementation as Python, Go and Zig.

Keep room/message IDs stable and nonzero. Set copied models, receive typed events,
apply accepted changes to application state, then update the CUI model. A send
event leaves the draft intact; clear it only after the application accepts it.
Gate history requests while loading or after history is exhausted. Resolve menu
actions against their original account/room/message identity before applying them.

Set `cui_chat_detail.image` to a decoded icon asset for an inline image card;
handle the attachment event to open an application-owned viewer. The copied model
retains the asset. Timeline bodies support mouse text selection and native copy.
Use `cui_insert_text` on the composer's editor to insert emoji at its caret or
replace its selection, then synchronize the application's draft state.

CUI supplies UI, not Matrix transport, encryption, persistence, account management,
file upload or call media. The concept demos use local fixtures. Do not interpret
verification/call controls as working security or communications services.

## Platform and capability limits

Linux uses installed GTK 4; macOS uses AppKit; Windows widgets use WinUI 3 hosted
with Win32 integration. There is no embedded browser or bundled font. System
libraries and platform runtimes remain deployment dependencies.

Native controls provide baseline keyboard, IME and accessibility behavior, but
full accessibility, RTL/bidirectional text and cross-platform input audits remain.
Windows/macOS implementation is not equivalent to verified runtime behavior there.
Do not use Linux captures as proof of another platform's rendering.

Canvas blur processes app-owned pixels, not the desktop behind a window. Native
widget opacity varies by backend; check success and drawing capabilities.
Layered stacks provide placement, while apps manage modal input/focus policy.
Feedback toasts are in-layout regions, not OS notifications. Best-effort window
positioning can fail on Wayland. Consult current headers before promising a
specific window, overlay or transparency behavior.

Dynamic widget deletion, lazy/richer table data and broad drag-and-drop support
remain limitations. API/model layout changes require rebuilding the library and
bindings together. Do not mix old native binaries with new wrapper declarations.

## Verify the integration

Build and run the smallest relevant example or test, exercising actual callbacks,
shutdown and model updates. On Linux, use Xvfb for headless native tests and check
which tests were registered. For visual work, inspect actual native output at
normal and enlarged text/display scales. Report the platform and tests actually
run, with any unverified behavior.

When changing CUI itself, follow the checkout's `AGENTS.md`, update affected
bindings/tests, and consult `docs/site/README.md` for generated docs and capture
provenance. Keep this versioned skill and any installed personal copy in sync when
intentionally updating the skill.

## Accessible, localized chat

Apply `cui_chat_set_label` translations to every component; localize custom
commands and model content in the app. The built-in plural pair supports one vs
other counts. Pass display scale alone to `cui_chat_refresh`; it also applies app
text scale. Make fixed-height chat hosts and navigation adaptive at 200% text.
Use `CUI_CANVAS_TEXT` for readable canvas content and preserve action regions
for controls. Save `cui_focused_descendant` before disabling a modal's background
and restore it after enabling the background. See the accessibility/chat guides
for ABI rebuild requirements and native verification limits.
