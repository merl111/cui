# A reference for coding agents

Start from the generated catalog and public headers. CUI is a developing API, so exact signatures and the current component limitations take precedence over examples remembered from an earlier revision.

## Discover the surface

`llms.txt` links the entry points. `catalog.json` contains stable component IDs, behavior, limitations, C APIs, example paths and native capture metadata. `api.json` contains every extracted public function declaration, its header and adjacent source comments. `llms-full.txt` concatenates the human documentation, component contracts and API declarations for offline retrieval.

The catalog schema is versioned independently from the C library version. Each component has a stable relative URL under `components/`. Each API symbol has an anchor under `api/`. Raw public headers are available under `sources/include/`. Structured support metadata separates implementation from verification: native Windows/macOS verification is deferred, not required to continue developing the API or this documentation.

## Work from a contract

Identify the constructor, model setter, event accessor and lifetime rule before writing code. All UI calls stay on the main thread. The app owns handles; callbacks borrow application userdata; strings and supported model arrays are copied. Ordinary setters are silent. Explicit action APIs can emit synchronously.

For composed controls, listen on the root. Do not overwrite internal part callbacks to observe changes. For sorted tables, use source-row mapping. For choice/tree navigation, use stable nonzero IDs. For text getters, allocate the required UTF-8 byte count plus a terminator.

## Verify an example

Component recipes come from `examples/showcase/cases.inc`, which is compiled into `cui_showcase`. Their surrounding includes, callbacks and app lifecycle are in `examples/showcase/main.c`. Launch the stable ID to reproduce the native example. A browser screenshot is evidence of a Linux render, not a browser implementation of the C control.

The C, Go, Python, Zig and Rust galleries are complete applications in the source tree. Their smoke modes exercise the event loop and callbacks. Start with the relevant language gallery when a concise recipe omits host application setup. Binding method names are language-specific; use the binding guide and source rather than mechanically translating C names.

## Keep documentation in sync

When adding a control, add its catalog entry and a compiled showcase case. When adding a public function, the generator indexes its declaration automatically. The documentation checker compares header exports with the API index, checks component constructor/enum coverage, verifies internal links and checks native image coverage and provenance.

Update behavior and limitations as implementation changes. Do not change a prototype to completed solely because it has a constructor or screenshot. Avoid describing unsupported floating overlays, syntax highlighting, lazy data providers or per-widget destruction as available features.

## Useful boundaries

No inference, networking, image decoding, speech capture or screen recording service is bundled. Applications integrate those services through data and events. GTK is an allowed system dependency. The documentation site has no third-party runtime packages. Keep those decisions explicit when proposing new features.

## Rust integration

Start from [the Rust guide](rust.md) and `bindings/rust/examples/hello.rs`. Use checked handles and owned model results from the convenience layer; use `cui::sys` only when prepared to enforce C's unsafe contracts. The crate adds no Cargo dependencies and links a prebuilt static CUI archive. `CUI_LIB_DIR` selects its directory. Main thread only, one App, quit in callbacks, drop App after run; handles are weak and !Send/!Sync. Closures remain retained until teardown. Icons own independent references.

When public headers change, run `python3 tools/generate_rust_bindings.py`, update convenience wrappers, then run binding coverage and C/Rust ABI checks. The checked-in raw file is generated without bindgen. Native hello, gallery and contracts are tested by CTest at normal and 4K scales. Do not infer cross-platform verification from these Linux checks.

## Safety and custom rendering

Use [the memory-safety workflow](memory-safety.md) when changing ownership, model replacement, parsers or native resources. Keep leak detection enabled and retain unsuppressed reports; a toolkit-only reproducer is required before classifying an allocation as external. The current Linux table leak and local Valgrind startup blocker are explicit limitations, not passing results.

[Waypoint / Rust](../plans/rust-simulator.md) is the fourth native demo. Use [the drawing guide](drawing.md) and `cui_draw.h` for surface ownership, command parameter meanings, group opacity, native text and labeled regions. Every binding exposes the new APIs. Check return values: native widget opacity varies by platform; the renderer capability flags describe surface composition only. Linux native tests and screenshots are available; Windows/macOS runtime verification remains deferred.

## Separate windows and measured performance

For a floating inspector or custom device frame, use [custom windows](windows.md). Waypoint creates two real native windows sharing one model. A hidden panel retains its state; closing the last visible window exits the loop. Placement is best effort and unavailable on Wayland.

Use [performance measurements](performance.md) for measured workloads, hardware and raw samples. Small changes can use `cui_surface_render_region`, but the application must supply correct damage and a complete CLEAR-first scene. Do not equate CPU texture submission time with GPU upload time or frame rate.
