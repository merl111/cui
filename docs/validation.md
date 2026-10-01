# Validation snapshot — September 30, 2026

This records copied pattern collections, configurable filters, insight datasets, desktop controls, pickers, breadcrumbs, searchable-tree Sidebar, autocomplete/command palette, tokens, feedback regions and filtered multiple-file/folder dialogs. It does not certify full completion of the catalog.

- GCC Release: 30/30 CTest cases passed, including native GTK controls and pickers at 1×/2×, a 3840×2160 virtual display, 150% text at 2×, all 21 pattern families, tree and table models with 10,000 rows, breadcrumb model replacement, ancestor-preserving Sidebar search, and Python/Go/Zig examples. New search/feedback cases cover disabled choices, native key controllers, IME preedit protection, Unicode queries, focus return/loss, sorted result IDs, 10,000 copied choices, token limits/removal/reuse and paused/reentrant notification expiry.
- Clang ASan/UBSan: the earlier 24/24 native suite passed; all five affected pattern/showcase/4K tests passed after this model update. Address and undefined-behavior checks are enabled; leak detection is disabled for toolkit-global caches. An additional run with leak detection enabled reported fontconfig/GTK allocations, so this is not a leak-freedom claim.
- Follow-up keyboard-scroll fix: both search/feedback tests and all three language examples passed again; both sanitizer search/feedback variants passed again. The GTK 4.6 scroll-action branch was separately compiled and exercised against the installed GTK 4.22.4 runtime. Actual GTK 4.6 execution remains untested.
- Native file tests cover copied filter options, multiple files/folders, Unicode paths, filter switching, uppercase suffixes, retained results, cancellation and save destinations without writing files, at 1× and 2× on a 3840×2160 virtual display. Both release and sanitizer variants passed again after the final save-name encoding fix, including a suggested non-Latin filename. The test includes a legacy GTK TreeView chooser fallback; execution on GTK 4.6 remains outstanding.
- Windows: the complete Zig example cross-compiled and linked for x86_64 Windows GNU, including the manifest and every C library source. Native execution/visual validation remains outstanding.
- macOS: backend source exists; compilation and native execution require a macOS SDK/host and remain outstanding.
- Linux screenshots were inspected for the control gallery, insight chart, enlarged typography, and the updated desktop gallery (breadcrumbs, tree, date/time fields and picker actions). The 4K typography capture is 2160×1800 physical pixels. The new Labels/empty-state capture is 1920×1520 physical pixels on a 3840×2160 display at 2× with 150% text; the expanded command palette was also inspected.

Measured Linux x86_64 GCC Release artifacts, excluding installed platform libraries:

| Artifact | Bytes |
| --- | ---: |
| Stripped C settings example | 128,088 |
| Stripped C component gallery | 173,432 |
| Stripped C desktop gallery | 161,176 |
| Stripped shared CUI library | 207,520 |
| Stripped native showcase | 201,992 |
| Unstripped static archive | 333,850 |

These are development snapshots, not size guarantees. Go/Python/Zig language runtimes and executable sizes are separate. No runtime-memory target has been established.

Ripwire's quality comparison identified increased complexity in platform dispatch, font handling, record filtering and layout, plus wrapper-duplication reports. Native command dispatch, indicator painting and macOS text-field creation were separated; checkbox creation now shares the checkable constructor. The latest delta reports 28 major regressions against the original prototype baseline, plus new-symbol findings. Grid slot allocation and Windows message routing were simplified; search matching, token model updates and feedback presentation were split into focused operations, with shared descendant-focus detection; file-option copying and platform result collection were also separated into focused operations. The new collection construction/search and insight copying helpers were also separated; none now exceed the complexity threshold. Dispatch, layout, FFI, test-state-machine complexity and new composition metrics remain documented technical debt. This is not a clean quality-gate result.

Reproduce the full Linux suite:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCUI_BUILD_TESTS=ON -DCUI_BUILD_BINDING_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The current sanitizer workflow is `python3 tools/check_memory.py sanitizer`. It enables ASan, UBSan and leak detection, preserves reports, and fails on unsuppressed findings. See the [memory-safety guide](guides/memory-safety.md) for documented toolkit exceptions and the newer audit below; earlier leak-disabled runs above are historical evidence only.

GUI tests require permission to start Xvfb. Go, Python and Zig must be installed for binding tests; no language packages are fetched by those builds. Optional pip packaging uses setuptools as build tooling.

## Documentation and showcase increment

Windows/macOS native verification is deferred by the project owner; it is not a prerequisite for framework or documentation development. The prior 27 release and 24 sanitizer tests cover the library increment above. The newly added `showcase` test separately passed in both configurations, creating and tearing down every catalog example.

The static site has 89 HTML pages, 67 catalog entries (including all 21 pattern families), 18 guides and all 194 public function declarations. All 67 native Linux captures have source/image hashes recorded in provenance. The documentation checker passes constructor/enum/API coverage, compiled recipe ID checks, PNG/hash validation, search URLs and every internal link/fragment. Eight new guides cover lifecycle, events, appearance, data, accessibility, packaging and agent use.

Browser checks passed for API search, category-plus-query filtering, empty results, copy-to-clipboard, keyboard language tabs, theme switching and mobile navigation. Desktop light/dark layouts and a 390×844 mobile viewport were inspected; tested mobile catalog, example and guide pages had no horizontal overflow. Native pattern screenshots were enlarged to include their controls. The site uses no runtime packages, CDN scripts or remote fonts. It is a local static preview, not a deployed public site.

The quality delta still reports the same 28 major findings against the original prototype baseline. New documentation tooling has additional parser/checker and example-CLI complexity findings; the Markdown parser and API comment reader were separated into focused operations. The quality gate is not clean and is not represented as passed.


## Pattern data models

`tests/pattern_models.c` exercises copied insight values, selected-ID preservation, point clamping, empty/invalid datasets, native navigation/scrubbing, Unicode predicates, all text/numeric filter operations, AND/OR queries, sorted source indices, item copying, per-item expansion/completion/selection, upsert/removal, disabled controls, bounded slot reuse and removal from inside an action callback. It runs at ordinary scale and on a 3840×2160 display with 2× scaling and 150% text. Go, Python and Zig galleries perform model round trips and exercise the same public ABI.

The collection, filtering and insight code is shared C composed from existing native controls. This does not establish Windows/macOS visual or runtime behavior; their native verification remains deferred.

## Binding parity and language tutorials

The binding-focused suite passes all eight checks: header coverage, Python/Go/Zig first-window programs, the three existing language galleries, and a native Zig convenience-wrapper integration test. The latter covers commands/menus, alerts, layout, state/focus, data models and the valid 4096-pixel image boundary. The coverage check verifies all 194 C functions have Python typed declarations and convenience helpers, Go wrappers/bridges, and Zig convenience wrappers; it also checks Python argument counts and Zig's raw header imports. Source coverage is a guard against omissions, not proof of every runtime combination.

Three new language guides embed complete source files and marked gallery sections during documentation generation. The homepage exposes language links, guide cards and copyable code tabs before the component catalog. Browser checks passed mouse/keyboard tabs, copying, guide navigation and mobile layout at 390×844 in light/dark themes. The documentation checker passes all 89 generated pages and local references. No C backend behavior changed in this binding/documentation increment; Windows/macOS native verification remains deferred.

## Native application examples

The app collection adds Relay (Go messenger), Cadence (Python music player) and Postbox (Zig mailbox). Six Linux tests exercise their actual native button callbacks and controller state, once at ordinary scale and once on a 3840×2160 display at 2× with 150% text. Assertions cover search/selection mapping, empty states, simulated replies and playback, likes, folder moves, and validated local message sending. Property changes are followed by explicit controller handlers where CUI setters intentionally emit no callback. This is interaction coverage, not pixel-perfect layout or screen-reader verification.

All three have real GTK window captures, recorded source/image hashes and generated source exports. The static site now has 93 pages, including the app collection and three individual walkthroughs; app metadata is also exported through `apps.json`, site search and `llms-full.txt`. Windows/macOS verification remains deferred. The examples are memory-only mocks and do not contact Telegram, Spotify or email services.

The targeted run passed all 14 app/binding checks (the six app variants plus the eight binding checks). After adding draft-preservation and capacity/field-length assertions, the four affected Go/Zig variants passed again. Documentation verification passes all 93 pages and app capture hashes. Browser inspection covered gallery-to-detail navigation, app search results, light/dark themes, and 390-pixel layouts without horizontal overflow. The quality comparison still reports the existing 28 major findings; it is not a clean gate. New example callback registrations are flagged as possible dead code by static analysis, although the native smoke checks execute them; the Go builder and Python controller also retain verbosity findings.

## Advanced demo workflows

The native app examples now exercise larger local models and multi-step workflows. Relay adds conversation creation, pins, archive/unread/group filters, message history paging/search, reactions, quoted replies, edits and fictional attachment metadata. Cadence expands to 12 tracks and adds named playlists with membership editing, a bounded reorderable queue, playback history, shuffle and volume restoration. Postbox adds a multi-select table, unread/starred/attachment filters, bulk moves with one-step undo, forward, saved drafts and resuming/sending a draft without duplicating its record. These features remain offline and memory-only.

The six app test variants cover these workflows at normal and 4K/large-text settings. The tests use real native action callbacks and explicit controller calls for deliberately silent setters. The site manifest documents model ownership, bounds and simulated behavior; the same descriptions are exported to agent-readable documentation.

## Icon assets and app presentation

The icon API adds independently owned vector/RGBA assets, validated portable asset decoding, build-time SVG importing, system PNG/JPEG loading, native icon views and native button/toggle integration. Optional built-in symbols are convenience assets. All 208 C functions have Python, Go and Zig coverage. The static docs contain 69 component recipes and native captures, 19 guides and three app demos.

The 44-case Linux suite passed, including icon ownership/action/rendering tests at 1× and 2×, a pixel-level raster replay regression, compiler input validation, all six app scale variants and language bindings. PNG decoding was tested outside the execution sandbox because this GTK distribution invokes its system decoder through D-Bus. The SVG compiler uses only the Python standard library and does not fetch resources.

Relay uses separate incoming/outgoing bubbles, a compact chat header and optional action/inspector regions. Cadence uses imported SVG play/pause artwork and native vector transport/toggle controls. Postbox uses icon actions in its reading toolbar. Screenshots are native Linux captures; Windows/macOS native execution remains deferred. The GDI+ and AppKit icon backends have been implemented but not validated on those platforms.

The quality scan still reports debt against the original prototype baseline, including existing platform dispatch and new SVG geometry parsing. Path parsing was separated into token parsing and geometry operations; icon command validation was split into per-command and path-sequence checks. This is not a clean quality-gate claim.

## Rust bindings — 2026-09-30

Rust now covers all 208 current public C functions in raw declarations and convenience wrappers, without Cargo dependencies. Linux validation used Rust 1.98.1. Windows/macOS native verification remains deferred.

- The complete CTest suite passed **52/52** after the Rust integration, including all existing bindings and three mock apps.
- Rust hello, gallery and native contract examples passed at normal scale and on a 3840×2160 virtual display at 2× with 150% text.
- Native contracts cover UTF-8/NUL validation, silent setters, disabled activation, callback replacement, owned icon lifetimes, table row validation/sorting/selection, tree/token/pattern models, font application, commands, copied file-dialog options, cancellation, callback panic containment and stale handles after app destruction.
- The Rust gallery checks native save actions, model selection and SVG-derived play/pause switching.
- Three unit tests and three compile-fail doctests passed. The latter enforce non-Send/non-Sync GUI ownership.
- All **304** independently compiled C/Rust enum, size, alignment and field-offset comparisons matched the Linux host ABI.
- Generated ABI freshness, all-language 208-function coverage, Python/Rust arities, Cargo formatting, strict Clippy and rustdoc generation passed offline.
- Documentation checks passed for **97 pages**, **20 guides**, **69 native component recipes/captures**, **3 app demos** and **208 C APIs**. Rust homepage tabs, quickstart navigation and examples were inspected in the browser.

The Rust API is in `bindings/rust`; runnable examples are under `bindings/rust/examples`. Setup, models, errors, ownership and desktop/icon APIs are documented in [the Rust guide](guides/rust.md). The local crate is not published to a package registry.

## Memory-safety audit and Rust demo plan

The new Linux workflow runs ASan/UBSan/LSan, a separate Valgrind build, Clang static analysis and three libFuzzer harnesses. Safety CI and a standard-library Python runner preserve logs, analyzer reports and crash inputs. No shipping dependencies were added.

- Fixed two leaked owned GTK tree-node references; a native finalization regression checks model replacement. Native table columns are now reused across data/sort updates, with factory replacement only for changed editability. Added guarded picker model/mapping bounds checks and removed a dead assignment identified by static analysis.
- Lifecycle/OOM regression passed: 131 injected construction failures, seven table replacement failures, 300 stable tree/picker replacements and twelve full pattern construction/teardown cycles. Direct CUI allocation tracking returns to zero. Release tests now retain their assertions.
- Full Linux Release regression: **53/53 tests pass**, including C controls, Go/Python/Zig/Rust examples and contracts, ABI/binding checks, app mocks and 4K cases.
- Clang ASan/UBSan with LSan enabled: **28/30 native cases pass** after documented font/parser exceptions. Both table tests still report **429 bytes in seven allocations** on GTK 4.22.4. A standalone GTK table reproducer, linked without CUI, reports native column retention as well. This finding remains unsuppressed and the safety command correctly exits nonzero; the leak gate is not clean.
- Clang static analysis: complete rescan after fixes reports no bugs. GCC analyzer diagnostics were reviewed but its warning/bailout output is not presented as a clean result.
- Three 30-second fuzz campaigns: 25,310,636 icon inputs, 2,755,762 model inputs and 12,105,547 layout inputs; no reported sanitizer findings. Bounded harnesses are smoke campaigns, not exhaustive coverage.
- Valgrind 3.25.1: blocked at startup by missing matching loader debug symbols; the configured debuginfod service did not provide them. No Memcheck pass is claimed. CI installs libc6-dbg and retains diagnostics, but has not been executed on the hosted runner here.
- Windows/macOS native memory checks remain deferred.

See [Memory safety and leak checks](guides/memory-safety.md) for exact commands, suppression scope, standalone toolkit reproducers and limitations. The [Rust simulator plan](plans/rust-simulator.md) describes the next mock app and its required drawing, layering, alpha and background-material work; these are planned features, not completed capabilities.

Raw logs and the audit summary are retained in `build-memory/audit-2026-09-30/`. Ripwire still reports 34 major regressions against the original prototype baseline. The new safety-runner class and standalone GTK reproducer also trigger size/complexity heuristics; their grouped workflow/reproduction steps are retained for readability. This is not a clean structural quality gate.

## Drawing and Waypoint — 2026-10-01

The Linux release suite passed all 60 tests after adding surface/canvas tests, three small language drawing examples and Waypoint at normal and 4K scale. Subsequent iPhone-ratio, native focus and centered-text changes passed the affected drawing, simulator, ABI, coverage and binding-example tests. A separate Rust unit test now guards portrait/landscape body and display proportions against Apple’s iPhone 16 Pro dimensions; it is registered as `rust_simulator_geometry`.

The final drawing tests passed ASan, UBSan and LSan at 1× and 2× using the existing narrow system-library suppressions. They include allocation-failure rollback, nested alpha/clip pixel assertions, native glyph-centering bounds, current-region activation, native GTK focus/gesture callbacks and repeated retained-surface replacement. The CUI allocation tracker reports no live owned allocations after teardown. Clang static analysis and strict Rust Clippy checks passed. The bounded drawing-command fuzzer completed 7,647,586 inputs in 61 seconds without a sanitizer finding; font/icon pointer ownership is exercised separately by native tests.

Native captures were refreshed for 70 components and four app demos. The website checker passes 102 pages and all 222 C declarations, source exports, capture provenance, links and anchors. The 4K Waypoint capture uses 2560×1800 surface pixels on a 3840×2160 virtual display, device scale 2 and 150% scene text. Body/display ratios, button centering and large-text spacing were visually inspected. Repository quality-delta still reports 34 older baseline findings; it is not a clean quality gate. This work does not resolve the previously documented GTK table leak or Valgrind loader-symbol infrastructure limitation, and does not verify native Windows/macOS execution.

Logs for this pass are under `build-memory/drawing-audit/` (local build evidence, not shipped runtime content).


## October 1: drawing performance and independent simulator windows

All 63 Linux CTest cases passed in three sequential batches (1–23, 24–43, 44–63), including two new native XTest mouse/keyboard tests at 1× and 2× with enlarged text. These verify distinct phone/control window IDs, phone proportions, close/reopen, keyboard panel toggling and native rotation in both directions. Bare Xvfb has no window manager, so drag completion is not covered by these checks. Windows/macOS runtime verification remains deferred.

Binding checks cover all 228 exported C functions in Rust, Python, Go and Zig; their runnable examples build and pass. The drawing oracle compared 128 deterministic before/after frames byte-for-byte. A 30-second Clang/libFuzzer drawing run completed 1,887,184 inputs with leak detection enabled and no report. An initial sandboxed run completed its inputs but its final LeakSanitizer process inspection failed; the successful run was outside that sandbox.

Clang static analysis reports no findings. Targeted drawing/window allocation-failure, lifetime and pixel tests run under ASan/UBSan with leak detection and the existing narrowly scoped Fontconfig/Pango suppressions. This is evidence for these exercised paths, not a claim that all C code or external libraries are leak-free; the earlier documented table-library leak remains separate.

The ripwire structural check uses a September 30 sidecar baseline predating much of the library. It reports historical complexity/size growth and is not a clean change-specific pass. Rendering kernels and demo scene builders remain complexity hotspots; no broad baseline reset or suppression was applied.

See [performance and benchmarks](guides/performance.md) for machine details, measured workloads and raw samples, and [custom windows](guides/windows.md) for the new API contracts.

## October 1: attached Waypoint inspector and proportional toolbar

The phone toolbar now follows the fitted device body rather than the entire native window allocation. The inspector starts hidden, opens from Settings (or Ctrl/Command+I), and uses a parent-relative anchor to follow the phone. GTK presents it as a non-modal popup with a translucent background and opaque foreground. The window guide links RocketSim's side-window documentation, explains the native popup/owned-window approach, and distinguishes background alpha from desktop blur.

Linux native XTest checks cover hidden startup, Settings and keyboard toggling, close/reopen, proportional rotation, all four corner grips, toolbar dragging and inspector attachment at 1× and 2× on a 3840×2160 display. A separate XComposite/XRender run verified these interactions on a composited display and captured a deliberately wide phone allocation: the toolbar remains phone-width. The showcase capture is real native output over a synthetic gradient desktop; its provenance records that environment. A headless Mutter Wayland run also passed the Rust simulator's state/alpha smoke checks; pointer-driven Wayland interactions were not automated.

The full examples build includes Rust, Python, Go and Zig coverage for all **231 C APIs**. Host C/Rust ABI comparison and simulator geometry/state tests pass. The C drawing/window tests exercise anchor validation, cycle rejection, theme propagation, native popup closure and both window creation/destruction orders. ASan/UBSan with leak detection passes these tests at 1× and 2× using the existing Fontconfig/Pango suppressions and `GDK_DISABLE=gl,vulkan`, matching the memory-check runner. An initial invocation omitted that setting and reported Mesa/EGL initialization leaks; it was rerun in the established software-rendering configuration, without adding suppressions. Clang static analysis reports no findings.

Ripwire's old repository baseline still reports historical debt, so it is not a clean change-specific quality pass. The new silhouette logic was split into pixel-region generation, native bounding-shape application and canvas coordination; these helpers no longer appear among its complexity/size findings. Windows/macOS backend source is updated, but runtime verification remains deferred.

## October 1: Wayland popup dismissal and rotated toolbar follow-up

A user-reported intermittent inspector failure reproduced under headless Mutter: the popup initially reported visible, then the compositor dismissed it because its native buffer was separated from the parent's buffer. The previous Wayland smoke test toggled state within one callback and did not wait for that response. This was a test-coverage gap.

The GTK backend now bounds the decorative popup gap by available parent padding, clamps anchor rectangles during pending resizes, and presents attached canvas panels during native allocation. New `window_panels_wayland_1x` and `_2x` tests wait across compositor frames and assert real mapped state, logical size, reopening, and resizing larger and smaller with the panel open. Both pass. The X11 mouse/keyboard checks and Rust simulator tests also pass at 1×/2×.

The toolbar now moves vertically with the fitted phone body, retaining its gap after landscape rotation even if the native window stays tall. A Rust regression test checks both orientations in a tall allocation and confirms the labeled Settings hit region follows the toolbar. Settings now has a visible text label and a side-panel icon. Both the CMake and Cargo-local simulator executables have been rebuilt; existing processes must be restarted to use the changes.
