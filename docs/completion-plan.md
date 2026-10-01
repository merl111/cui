# Desktop catalog completion

Requested scope: all recommended desktop components, plus behavioral completion of the original control catalog and 21 reference families. Existing composition prototypes are not treated as completed equivalents of the reference.

Completion criteria for a component: documented public API and ownership; actual stateful interaction; programmatic updates; keyboard/focus behavior; theme and high-DPI layout; accessibility labels/roles where exposed by the platform; Go/Python/Zig access; a runnable gallery example; meaningful native tests. Platform implementation and native verification are tracked separately.

## Ordered work

1. Shared foundations: focus, shortcut commands, lifetime-safe deferred UI work, dynamic data models and update APIs, accessibility metadata.
2. Dialogs and application actions: alerts/sheets, open/save/folder dialogs, menus/context menus, toolbars, shortcuts.
3. Layout/navigation: split panes, grids and wrapping containers, tree views, breadcrumbs.
4. Data/input: virtualized editable multiselect tables, numeric steppers, date/time/color/font pickers, validation, autocomplete and tokens.
5. Transient feedback: popovers, banners, toasts, empty/error states and command palette.
6. Content/workflows: rich text/Markdown, syntax highlighting, vector icons, drag-and-drop and undo/redo.
7. Complete the 21 reference families against `components.md`: dynamic tool/task/chat/source collections, streamed formatted content, interactive diff selection, arbitrary data filters, tree sidebar, editable flow graph, real insight datasets, property inspector and frame preview controls.
8. Linux native and binding validation, high-DPI/large-text/accessibility checks and small-build size measurement. Windows/macOS native verification is a later phase and does not block development.
9. Develop the polished documentation and component-showcase website alongside the framework, with newly written detailed documentation and agent-readable references. The user explicitly removed feature completion and Windows/macOS verification as prerequisites.

## Platform evidence

Linux: GTK native test and screenshot environment available.
Windows: compiler/linker available through Zig; native runtime/visual checks unavailable here.
macOS: implementation can be authored; no SDK/runtime available here.

AI inference, networking, dictation services and screen capture are application integrations. A complete component must deliver useful integration events and render supplied data; it does not require bundling those services in the GUI library.

## Implemented desktop increment

- Shared command state, menus/context menus, toolbar actions and shortcut routing; focus and accessibility names/help; native text undo/redo and read-only state.
- Native open/save/folder dialogs and alerts/sheets, with completion and cancellation. File dialogs support copied named extension filters, filter selection, multiple files/folders and retained UTF-8 local path results. Save uses native overwrite confirmation; non-filesystem resources remain outside the local-path API.
- Grid cells with spans and overlap rejection; wrapping rows; resizable split panes with programmatic positions and user events.
- Native tree controls with stable 64-bit IDs, copied models, expansion, selection, activation, keyboard navigation and model replacement. GTK row widgets are virtualized; a 10,000-row case is exercised.
- Native color/font dialogs, copied typed results, cancellation, and full family/size/weight/italic application.
- Wrapping breadcrumbs with stable IDs, reusable segments, and native keyboard-focusable ancestor actions.
- Sidebar composition upgraded to a searchable native tree, preserving matching ancestors and restoring expansion after search.
- Native numeric steppers, bounded precision, date/time values with Gregorian validation, and labeled fields with inline application-supplied validation errors.
- Autocomplete and in-layout command palettes with copied choice models, Unicode search, disabled options, virtualized results, keyboard selection, focus restoration and IME-aware navigation.
- Choice-backed token fields with ordered IDs, selection limits, duplicate rejection, wrapping reusable removal buttons, Backspace removal and application validation events.
- Reusable banners, in-app toast regions, empty/error states, semantic tones, action/dismiss/timeout events and paused/restartable expiry. These are in-layout regions; floating popovers remain outstanding.
- Go/Python/Zig wrappers and executable examples exercise these additions as well as tree selection, numeric values, civil dates/times, validation and table editing/selection/sorting.
- Virtualized native tables now support multiple selection, editable text columns, stable text/numeric sorting, source-row indices, and typed selection/edit/sort events. Header sorting on GTK requires 4.10+; programmatic sorting retains the 4.6 baseline.

- Copied stable-ID tool/task/chat/context collections, per-item actions, reusable slots and synchronous removal from callbacks.
- Application-supplied insight datasets, labeled data points, stable selection, bounded navigation and scrub events.
- Unicode record queries and copied text/numeric AND/OR predicates with source-row mapping; equivalent Go/Python/Zig APIs and executable integrations.

These additions do not complete the broader request. The original 21 compositions retain the explicit gaps in `components.md`; lazy table data and richer cells, floating popovers/overlays, rich content, drag/drop and dynamic widget deletion still need implementation. Windows native behavior and macOS compilation/native behavior also remain unverified here.


## Documentation and showcase website deliverable

Build alongside framework development; keep per-component completion and platform evidence explicit. The site should have a deliberate visual design, accessible navigation, searchable documentation and a dedicated showcase page for every primitive, desktop control and reference-family composition. Native screenshots and runnable examples must distinguish implemented behavior from application integrations and platform limitations; a web mock-up must not stand in for a native demonstration.

Write new, detailed documentation covering installation/builds, the C ABI, ownership and lifetime, thread rules, errors, state updates, callback ordering/reentrancy, keyboard/focus/IME, accessibility, theming, fonts/DPI, platform support, and integration recipes. Include equivalent working C, Go, Python and Zig examples wherever an API is exposed.

Make the documentation useful to coding agents: stable component/API identifiers and URLs, a versioned machine-readable catalog, text/Markdown references available alongside the site, discoverable indexes, explicit signatures/contracts, and accurate capability/verification status per platform. Derive catalog coverage checks from the public headers so new APIs and components cannot silently disappear from the reference or showcase. Examples should be compiled or executed by the existing validation workflow rather than copied into unverified snippets.


### Documentation implementation now available

`tools/build_docs.py` builds the repository-local static site with 67 component pages, 18 guides, all public C declarations, real Linux captures, runnable native recipes, searchable navigation, light/dark themes and responsive layouts. It exports a versioned JSON catalog/API index, original Markdown, `llms.txt` and `llms-full.txt`. `tools/check_docs.py` checks public constructor/enum coverage, native capture provenance, recipes, search targets and local links; Linux CI runs it. Windows/macOS native verification is deferred. The framework and these documents continue to evolve together; incomplete pattern behaviors remain explicitly labeled.

## Next Rust application and rendering APIs

[Waypoint / Rust](plans/rust-simulator.md) is now the fourth native app. The reusable surface/canvas API provides clipping, layers, group opacity, blur, native text/icons and input regions, with bindings and runnable examples in all four languages. Linux pixel, interaction, sanitizer and 4K checks accompany the demo. Windows/macOS native verification remains deferred; native child-control opacity limitations are documented in the drawing guide.

The [memory-safety workflow](guides/memory-safety.md) is now available. It found and fixed GTK tree-node ownership leaks. Two table leak tests remain red due to independently reproduced GTK column retention; Valgrind needs matching loader symbols on this host.
