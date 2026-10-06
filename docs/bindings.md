# Go, Python, Zig and Rust

All bindings use the same C ABI and native platform backend. No web runtime or third-party language runtime packages are required. Examples are runnable local applications, not just declarations. Packages are included in this repository; none has been published to a package registry yet.

Start with a complete runnable tutorial: [Python](guides/python.md), [Go](guides/go.md), [Zig](guides/zig.md), or [Rust](guides/rust.md). Each includes a first window, native button callback, setup commands, tested model examples and ownership rules. The homepage links directly to all four.

All 222 current C functions have typed Python declarations, Go wrappers/bridges Zig convenience wrappers, and Rust raw declarations plus convenience wrappers. `tools/check_bindings.py` guards coverage against the headers. Zig also exposes the full raw ABI through `ui.c`; wrappers now include command/menu objects, layout, focus, state and native diagnostics.

Build the C library first for Python, Go and Rust:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Python

Uses the standard library's `ctypes`, including retained callbacks, main-thread checks, app ownership and callback exception propagation. Python 3.9+.

```sh
PYTHONPATH=bindings/python python3 examples/python/gallery.py
# Exercise the event loop and quit automatically:
PYTHONPATH=bindings/python python3 examples/python/gallery.py --smoke-test
```

Alternatively install the local package with `python3 -m pip install ./bindings/python`. Installation uses setuptools as build tooling; the installed package has no Python dependencies. Set `CUI_LIBRARY` to the absolute path of `libcui.so`, `libcui.dylib`, or `cui.dll` when the shared library is outside the repository's default build directory. The Python package does not bundle that native library or GTK.

On Windows PowerShell:

```powershell
$env:PYTHONPATH = "bindings/python"
$env:CUI_LIBRARY = "$PWD/build/Release/cui.dll"
python examples/python/gallery.py
```

```python
import cui
with cui.App() as app:
    window = app.window("Example")
    root = window.root
    root.label("A native application").font(points=24, weight=600)
    status = root.label("Ready")
    root.button("Continue").on_action(lambda _: setattr(status, "text", "Done"))
    window.show()
    app.run()
```

## Go

Uses cgo and `runtime/cgo.Handle`: C retains integer handles, never pointers into the Go heap. Callbacks are released on `App.Close`; a callback panic stops the event loop and is returned by `Run`. **Lock the main OS thread before creating the app and keep every UI call on it.** Background work must arrange its updates on that thread (there is no cross-thread dispatch API yet).

```sh
cd bindings/go
go run -buildvcs=false ./cmd/gallery
# Automated mode:
go run -buildvcs=false ./cmd/gallery --smoke-test
```

`-buildvcs=false` is useful for this checkout's nonstandard/missing Git metadata; ordinary repositories can omit it. On Linux/macOS the default binding links `build/libcui.a`; Windows links the WinUI DLL from `build/Release`. For another location, use the `cui_external` build tag and supply the archive via `CGO_LDFLAGS`; system platform libraries remain linked by the binding.

```sh
CGO_LDFLAGS=/absolute/path/libcui.a go build -tags cui_external ./cmd/gallery
```

A consuming Go project can use a local `replace cui.local/cui => /absolute/path/to/cui/bindings/go`. Windows cgo consumes the MSVC-built WinUI DLL through a GNU import library generated from `windows/cui.def`; see [Windows packaging](guides/packaging.md#windows). Embed the PerMonitorV2 application manifest when packaging a Windows executable.

## Zig

The allocation-free wrapper provides convenience methods for **the entire C API**, including all 21 component families; raw C functions also remain available as `ui.c`. Keep callback data alive until callbacks stop/app destruction. Call `deinit` only after `run` returns. Zig wrappers do not perform runtime use-after-destroy checks.

Tested with Zig 0.16.0. The supplied build compiles the C backend directly, so no separate CMake build is needed:

```sh
zig build run
# Automated mode:
CUI_SMOKE_TEST=1 zig build run
```

`build.zig` links installed GTK on Linux and AppKit on macOS. Windows consumes a prebuilt WinUI `cui.lib` with `zig build -Dtarget=x86_64-windows-gnu -Dcui-lib-dir=build/Release`. It copies the two required DLLs to the install directory; the matching Windows App Runtime must be installed separately. There are no fetched Zig packages.

## Rust

The zero-dependency Cargo crate provides the complete raw ABI as `cui::sys` and checked convenience wrappers for every C function. Handles are neither Send nor Sync; cloned handles refer to the same app-owned object, and calls after app destruction return `Error::Closed`. Strings/model results are owned; setters copy their input. `Icon` uses automatic retain/release through Clone/Drop. Rust closures are retained until app teardown and callback panics return from `App::run` as errors with the normal unwind strategy.

```sh
cargo run --offline --manifest-path bindings/rust/Cargo.toml --example hello
cargo run --offline --manifest-path bindings/rust/Cargo.toml --example gallery
```

Build the C archive first. Set `CUI_LIB_DIR` for a different archive directory. Rust 1.82+ and native platform development libraries are required; Linux uses pkg-config for GTK. Add `cui = { path = "/absolute/path/to/cui/bindings/rust" }` to your Cargo dependencies. The [Rust guide](guides/rust.md) covers setup, the complete first window, errors, tables/trees, icons, dialogs, menus, fonts, testing and unsafe ABI access.

## Common contract

- One app, main thread only; app owns widgets, windows and timers.
- Stop timers as needed; stopped timers/registered callbacks are released at app destruction. Repeatedly registering callbacks or creating timers retains their storage until then.
- C copies strings, table data, chart series and RGBA images. Widget handles and callback context remain app-owned/application-owned respectively.
- Setters do not trigger actions. Native edits and `activate` do.
- `quit` is safe in callbacks. Destroy/close the **app** after the event loop returns.
- Fonts and application text scaling use the same C APIs in all languages. Zig's complete ABI is available even where no convenience method exists.

For Linux automated validation, install Go, Python, Zig, Rust and Xvfb, then configure with `-DCUI_BUILD_TESTS=ON -DCUI_BUILD_BINDING_TESTS=ON`. Build and run CTest. The language examples verify UTF-8, selection, real timers/event loops, native library loading and callbacks.

## Desktop APIs

All bindings expose the additional desktop, layout, navigation and input APIs. Python and Go have `Command`, `Menu` and `Dialog` handles. Widgets provide grid/cell, wrap/split, tree, number, date/time and field methods. Zig exposes every C declaration through `ui.c`, with convenience wrappers for tree and input values.

Python uses `datetime.date` / `datetime.time` for picker values and `cui.TreeItem(id, parent, text, expanded)` for copied tree models. Go uses `cui.DateValue`, `cui.TimeValue` and `cui.TreeItem`. Zig accepts the corresponding C structs. Tree parents must precede children and IDs must be unique and nonzero; invalid models are rejected without replacing existing data.

Numeric values use dedicated `number_value` / `NumberValue()` / `numberValue()` methods; the original normalized slider `value` API is unchanged. Listen on a field wrapper for edits and set its inline error; replacing the internal entry callback bypasses this forwarding. All programmatic input setters suppress callbacks.

Table wrappers expose multiple selection, editable columns, cell getters/setters, text/numeric sorting, source-row lookup and typed events. Python names use `table_*`, Go `Table*`, and Zig `table*`. The original single-index selection setter still clears other selected rows.


Color/font pickers and breadcrumb/Sidebar models are available in every binding. Python uses `FontValue`, `Window.color_dialog`, `Window.font_dialog`, `Widget.font_apply`, `BreadcrumbItem`, `Widget.breadcrumbs`, and `Widget.sidebar_items`. Picker callbacks receive `(result, value)` and `None` on cancellation/failure. Go provides `FontValue`, `Window.ColorDialog`, `Window.FontDialog`, `Widget.FontApply`, `BreadcrumbItem`, `Widget.Breadcrumbs`, and `Widget.SidebarItems`; inspect the result before using a callback's value. Zig offers `Window.colorDialog`, `Window.fontDialog`, `Widget.fontApply`, `Widget.breadcrumbs`, and `Widget.sidebarItems`, plus the complete C ABI. The Go, Python and Zig galleries include working picker buttons and a searchable tree Sidebar connected to breadcrumbs. Smoke tests exercise cancellation and copied models without requiring manual dialog interaction.

Search, tokens and feedback are available without additional language packages:

| Operation | Python | Go | Zig |
| --- | --- | --- | --- |
| Copied choice | `Choice(id, label, detail, keywords, disabled)` | `Choice{ID, Label, Detail, Keywords, Disabled}` | `c.cui_choice` |
| Autocomplete/palette | `Widget.picker` | `Widget.Picker` | `Widget.picker` |
| Choice model/query | `picker_items`, `picker_set_query` | `PickerItems`, `PickerSetQuery` | `pickerItems`, `pickerSetQuery` |
| Accept/result | `picker_accept`, `picker_selected`, `picker_event` | `PickerAccept`, `PickerSelected`, `PickerEvent` | `pickerAccept`, `pickerSelected`, `pickerEvent` |
| Token field | `Widget.tokens` | `Widget.Tokens` | `Widget.tokens` |
| Ordered selection | `tokens_set_selected`, `tokens_selected` | `TokensSetSelected`, `TokensSelected` | `tokensSetSelected`, `tokensSelected` |
| Feedback region | `Widget.feedback` | `Widget.Feedback` | `Widget.feedback` |
| Display/event | `feedback_show`, `feedback_event` | `FeedbackShow`, `FeedbackEvent` | `feedbackShow`, `feedbackEvent` |
| Navigation callback | `on_key(callback)` | `OnKey(callback)` | `onKey(callback, data)` |
| Restart timer | `Timer.start()` | `Timer.Start()` | `Timer.start()` |

The Go, Python and Zig galleries include searchable greetings, a command palette, an Undo notification and removable project labels. Its smoke test exercises selection callbacks, notification actions, token removal and model updates. Python and Go retain callback references until app teardown and convert callback exceptions/panics into an error after the event loop exits. Zig exposes the complete public C interfaces through `ui.c`, including optional parts and typed event enums. Consult `desktop.md` for model ownership, keyboard behavior, limits and the distinction between these in-layout regions and floating popovers.

For file filters and multiple selection, Python provides `FileFilter`, `FileOptions`, and `Window.file_dialog_with_options(kind, title, options, callback)`. Its callback receives `(result, paths, filter_index)`, where paths is a copied list and the index is `None` when unavailable. `Dialog.paths` and `Dialog.filter_index` remain readable while the app lives.

Go provides `FileFilter`, `FileOptions`, and `Window.FileDialogWithOptions(kind, title, options, callback)`, with callback arguments `(result int, paths []string, filterIndex int)`. The index is `-1` when unavailable. `Dialog.Paths()` returns copied strings and `Dialog.FilterIndex()` retrieves the chosen filter. Invalid options are rejected; no callback is scheduled for failed construction.

Zig provides `Window.fileDialog`, `Window.fileDialogWithOptions`, `Dialog.pathCount`, `Dialog.path(index)`, and `Dialog.filterIndex`. Use `c.cui_file_options` and `c.cui_file_filter`; path slices borrow the app-owned result and must not outlive the app. The callback retains the normal C signature and can inspect every result through its dialog handle.

The Go, Python and Zig galleries include a **Choose files…** action with Documents/All files filters and multiple selection. Smoke tests cover pending/cancelled result access and cancellation callbacks; native C tests exercise actual accepted selections and filter switching. Single-path constructors remain compatible.


The Go, Python and Zig galleries also exercise stable-ID task collections, reentrant removal, insight datasets/selection and numeric record filters. [Pattern data models](guides/pattern-models.md) lists the C, Go, Python and Zig names and the borrowed-versus-copied getter contracts.

Reusable icon assets, SVG importing, raster loading and icon controls are available in every binding. See [Icons and vector assets](guides/icons.md).
