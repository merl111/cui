# Build with Rust

Use native CUI controls with checked handles, owned model results, Rust closures and automatically released icon assets. The crate covers all public C functions through `cui::sys` and convenience wrappers. It has no Cargo dependencies, downloads or runtime packages. The native library depends on GTK on Linux, AppKit on macOS, and WinUI/Windows App Runtime on Windows.

## Build and run

Rust 1.82 or later, Cargo, CMake and a platform C toolchain are required. Linux also needs GTK 4 development files with X11 support and Xext (`pkg-config gtk4-x11 xext`) and `pkg-config`. Build the C archive first, then run the complete hello example from the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cargo run --offline --manifest-path bindings/rust/Cargo.toml --example hello
```

The build script looks in `build` on Linux/macOS and `build/Release` on Windows. For another location, set `CUI_LIB_DIR` to the absolute directory containing `libcui.a` (Linux/macOS), `libcui.dll.a` (Windows GNU import library) or `cui.lib` (Windows MSVC import library):

```sh
CUI_LIB_DIR=/absolute/path/to/native/build cargo run --offline --manifest-path bindings/rust/Cargo.toml --example gallery
```

The crate does not build or bundle the C library. Build the archive for the same architecture, target ABI and compatible runtime as the Rust program. Cross-compiling also requires a matching system SDK; on Linux configure `PKG_CONFIG` to a target-aware wrapper if needed. The macOS link uses AppKit; Windows links `cui.dll`, which uses WinUI 3 and requires the Windows App SDK 1.8 runtime. Windows/macOS native verification remains deferred.

On Windows PowerShell with a multi-configuration CMake build:

```powershell
cmake -S . -B build
cmake --build build --config Release
$env:CUI_LIB_DIR = "$PWD/build/Release"
$env:PATH = "$env:CUI_LIB_DIR;$env:PATH"
cargo run --offline --manifest-path bindings/rust/Cargo.toml --example hello
```

For your own Cargo project, use a local path dependency. The crate is included in this repository and is not published to crates.io:

```toml
[dependencies]
cui = { path = "/absolute/path/to/cui/bindings/rust" }
```

For release builds on all three platforms, including Windows DLL deployment and Linux GTK requirements, see [static linking and packaging](packaging.md).

## Your first window

Create the app on the process main thread. This is the exact runnable example, including its optional automated smoke mode:

<!-- include-example: bindings/rust/examples/hello.rs rust -->

Run it without interaction using `CUI_SMOKE_TEST=1 cargo run --offline --manifest-path bindings/rust/Cargo.toml --example hello`.

## Ownership and errors

`App` owns the native application. `Window`, `Widget`, `Timer`, `Command`, `Menu` and `Dialog` are clonable handles with checked weak references to it. Cloning a handle refers to the same object; it neither clones a widget nor keeps the app alive. Dropping a handle does not destroy its native object. Dropping `App` releases native objects and registered closures. Subsequent handle calls return `Error::Closed`.

Keep one app, and perform all UI calls on the main thread. Linux and macOS initialization reject worker threads. Windows initialization must also occur on the main thread. The types are neither `Send` nor `Sync`, preventing transfers into background Rust threads. There is no background-to-UI dispatcher yet. Use a main-thread timer to poll messages produced by workers.

Constructors return `Result<T>`. Invalid Rust inputs, such as an interior NUL in a string or an incorrectly sized pixel buffer, return `Error::InvalidInput`. Native construction failure returns `Error::NativeFailure`; `app.error()` provides the native diagnostic. State/model setters returning `Result<bool>` distinguish a closed/invalid handle (`Err`) from native validation rejecting the value (`Ok(false)`). Check both levels when acceptance matters.

Strings and model buffers are copied before setters return. Text getters return owned `String`s. Selection getters return owned vectors. `pattern_item_at` copies every string into an owned `PatternItem`, so later native model changes cannot invalidate its text. Stable IDs remain the application-level identity; reacquire a pattern item's widget part after model changes because native slots can be reused.

## Callbacks and timers

`widget.on_action` accepts a `FnMut(Widget) + 'static` closure. Capture cloned handles and use `Rc<Cell<T>>` or `Rc<RefCell<T>>` for shared local state. `on_key` receives the key and modifiers and returns whether it consumed the event. `app.every` accepts a `FnMut()` closure; `Timer::stop` and `start` control it.

Ordinary property setters do not emit callbacks. `activate`, command invocation and other explicit actions can emit synchronously. Release any `RefCell` borrow before triggering a nested action. Reentering the same `FnMut` handler is rejected by its borrow guard and is treated as a callback panic.

The binding retains closures until app destruction, including replaced or cleared handlers and stopped timers. `clear_action` and `clear_key` disconnect dispatch; they do not reclaim that storage immediately. Avoid repeatedly replacing handlers in a long-running update loop. Capture weak GUI handles rather than owning the `App` inside one of its own closures.

With Rust's normal unwind panic strategy, a callback panic is caught before crossing the C ABI, requests event-loop exit and becomes `Error::CallbackPanic` from `app.run()`. The normal Rust panic hook still runs. With `panic = "abort"`, the process aborts as usual. Call `quit` from callbacks and let `App` drop after `run` returns.

## Tables and copied models

The [Rust gallery](../../bindings/rust/examples/gallery.rs) includes a tree, searchable project choices, a sortable table, a validated field, saved-state feedback, SVG-derived play/pause artwork, task rows and a chart:

```sh
cargo run --offline --manifest-path bindings/rust/Cargo.toml --example gallery
```

Create a table with its column headings, then pass rows with exactly that width:

```rust
let table = root.table(&["Project", "Status"])?;
assert!(table.table_set_rows(&[
    &["Orbit", "Active"],
    &["Harbor", "Review"],
])?);
assert!(table.table_sort(0, false, false)?);
let source_index = table.table_source_row(0)?;
let displayed_text = table.table_cell(0, 0)?;
```

The table constructor remembers the column count to validate row buffers. Clones preserve it. A table obtained as a composition part or callback argument has no recorded schema: update it through the original table handle, or use the composition's `pattern_set_records` API. Arbitrary row writes through an unknown schema are rejected.

Safe model types include `TreeItem`, `Breadcrumb`, `Choice`, `RecordFilter`, `InsightSeries` and `PatternItem`. Input strings can borrow application data for the call. Parallel label/value arrays are checked before FFI. Tree parents must precede children; item IDs are stable, unique and nonzero. Pattern records use rows of three cells. Refer to [pattern contracts](pattern-models.md) for each family's semantics.

## Reusable icons and 4K

`Icon` owns an independent reference-counted asset. `clone` retains and `drop` releases it. Controls retain their own reference, so the local asset may be dropped after assignment. `get_icon` returns a newly retained `Option<Icon>`. Clearing with `set_icon(None)` releases the control's reference.

```rust
let icon = cui::Icon::load("assets/play.cuiicon")?;
let play = root.icon_button(Some(&icon), "Play preview")?;
assert!(play.set_icon_size(24)?);
assert!(play.set_icon_only(true)?);
drop(icon); // The native button still owns its reference.
```

Use `Icon::decode` with `include_bytes!` to embed compiled vector data, `Icon::vector` for application-defined paths, `Icon::rgba` for copied pixels, `Icon::image` for system-decoded raster files or `Icon::symbol` for built-in symbols. SVG is compiled with the repository's build-time importer; supported geometry and explicit restrictions are in [the icon guide](icons.md).

Sizes are logical units. Vector artwork follows display scale. Use `app.theme(sys::CUI_THEME_SYSTEM)` for system appearance, `app.text_scale(1.5)` for enlarged text, `widget.font` or `font_apply` for typography, and `window.scale()` to inspect the display scale. All use the same native backend as C.

## Desktop controls

`App::command` creates a named action with a shortcut and a Rust closure. `App::menu`, `Menu::add`, `submenu`, `separator`, `popup`, `Window::set_menu` and `Widget::toolbar` compose it into native desktop UI. Operations involving several handles reject objects from different apps.

File selection supports `FileFilter`, `FileOptions` and `Window::file_dialog_with_options`. File, alert, color and font callbacks receive `(Dialog, result, String)`. Inspect the result before reading selections. Use `dialog.paths()` for copied paths, `filter()` for an optional filter index, `color()` for an optional color and `font()` for an owned `Font`. Cancellation produces no accepted selection. Keep the app alive while a dialog is pending; `Dialog::cancel` dismisses it.

## Find the API

Most widget methods use the C function name without `cui_`: `set_text`, `set_role`, `table_sort`, `picker_set_items`, `tokens_set_selected`, `pattern_set_items` and `insights_set_series`. Exceptions include `box_layout` (Rust reserves `box`), `text` and `selected_text` (owned getters), and `tree_event`/`table_event` (return event data together). Enum constants retain their C names in `cui::sys`.

The complete raw ABI is in [sys.rs](../../bindings/rust/src/sys.rs). It is unsafe: callers must enforce native pointer, lifetime, threading and buffer contracts. Convenience wrappers are in [lib.rs](../../bindings/rust/src/lib.rs), [widgets.rs](../../bindings/rust/src/widgets.rs), [models.rs](../../bindings/rust/src/models.rs) and [desktop.rs](../../bindings/rust/src/desktop.rs). The raw namespace is available for direct C integration; safe handles deliberately do not expose unchecked raw pointer conversion.

Generate local Rust API documentation with:

```sh
cargo doc --offline --manifest-path bindings/rust/Cargo.toml --no-deps
```

## Verification and maintenance

`tools/generate_rust_bindings.py` regenerates the checked-in raw declarations from all public headers without bindgen or libclang. `--check` rejects stale output. `tools/check_bindings.py` checks declaration arity and raw/convenience coverage against every C function. `tools/check_rust_abi.py` independently compiles C and Rust probes and compares enum values, sizes, alignments and field offsets on the host ABI.

```sh
cmake -S . -B build -DCUI_BUILD_SHARED=ON -DCUI_BUILD_TESTS=ON -DCUI_BUILD_BINDING_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure -R '^(rust_|binding_coverage)'
cargo clippy --offline --manifest-path bindings/rust/Cargo.toml --all-targets -- -D warnings
```

The full binding suite needs Python, Go, Zig, Rust and Xvfb on Linux. Rust tests run hello, gallery and native contracts at 1× and on a 3840×2160 display at 2× with 150% text. They exercise real callbacks, models, UTF-8, asset ownership, app teardown and panic containment. Unit tests cover invalid inputs, and compile-fail doctests enforce thread confinement. These are Linux results; they do not establish Windows/macOS runtime behavior.

## Custom drawing

See [Drawing and compositing](drawing.md) for surfaces, group opacity, native text, icons, blur and accessible hit regions, with a runnable example in this language.


## Chat interfaces

The [chat component guide](chat.md) covers `cui::chat`: room navigation, measured
rich timelines, replies, reactions, polls, threads, a native composer, persistent
conversation panes and reusable panels. Explore Nebula, Daylight and Tiles through
native screenshots and the [runnable example](../../bindings/rust/examples/chat_components.rs).
The guide documents model ownership, event handling, integration and fidelity limits.


For the full Daylight application composition, run:

```sh
CUI_LIB_DIR="$PWD/build-chat" cargo run --offline \
  --manifest-path bindings/rust/Cargo.toml --example daylight
```

[Entry point](../../bindings/rust/examples/daylight.rs) ·
[Application state](../../bindings/rust/examples/daylight/state.rs) ·
[Composition](../../bindings/rust/examples/daylight/ui.rs).
These are application-level Rust models and event handlers over the reusable C
components. Navigation, per-room drafts, polls, reactions, inspector tabs,
threads and verification are exercised at 1× and 2× scale. The chat guide
includes native C/Rust comparison captures and the remaining HTML fidelity gaps.
