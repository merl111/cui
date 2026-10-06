# Build with Go

Use native CUI controls through cgo with Go callbacks and copied models. The binding uses `runtime/cgo.Handle` to retain callback context safely; it adds no third-party Go modules. This guide starts with a complete program tested on Linux.

## Build and run

Build the C library, then run the example from the Go module directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cd bindings/go
go run -buildvcs=false ./cmd/hello
```

A C compiler and cgo are required. Linux also needs installed GTK 4 development libraries. `-buildvcs=false` works around this checkout's missing Git metadata; normal Git repositories can omit it.

For a separate application module, add a local replacement pointing at this repository's binding. The module has not been published to a registry:

```sh
go mod edit -require=cui.local/cui@v0.0.0
go mod edit -replace=cui.local/cui=/absolute/path/to/cui/bindings/go
```

The default binding links the repository's `build/libcui.a`. To use another archive, build with `-tags cui_external` and set `CGO_LDFLAGS=/absolute/path/to/libcui.a`. Platform libraries are still linked by the binding. Windows cgo links the MSVC-built WinUI DLL through a GNU import library; see [static linking and packaging](packaging.md) for release commands on Linux, Windows and macOS. Windows/macOS native verification is deferred.

## Your first window

Call `runtime.LockOSThread()` before creating the app. All UI operations must stay on that main OS thread. The following is the complete `cmd/hello` program:

<!-- include-example: bindings/go/cmd/hello/main.go go -->

The optional smoke branch is exercised with `CUI_SMOKE_TEST=1 go run -buildvcs=false ./cmd/hello` from `bindings/go`.

## Callbacks, errors and ownership

`OnAction` receives a `cui.Widget`. Use `SetText` to update a label and `Text()` to read it. Ordinary setters are silent. `Activate()` and `Command.Invoke()` can call your handler synchronously. Composition callbacks belong on the root; replacing an internal part's handler replaces its built-in behavior.

`app.Run()` returns a recovered callback panic as an error. `app.Error()` reads the separate native diagnostic string. Constructors generally panic on allocation failure; model setters return `bool` so invalid input can be handled without exceptions. Always handle the error from `cui.New()` and `Run()`.

`defer app.Close()` releases native handles and retained Go callbacks after the loop returns. Inside callbacks call `app.Quit()`, never `Close()`. Background goroutines must arrange for UI work to return to the owning thread; CUI does not yet expose a dispatcher.

## Supply data

The [complete Go gallery](../../bindings/go/cmd/gallery/main.go) includes records, tasks, insight datasets, menus/dialog integrations, native selection and callbacks. Run it with `go run -buildvcs=false ./cmd/gallery` from `bindings/go`.

This is its tested pattern-model setup:

<!-- include-region: bindings/go/cmd/gallery/main.go go pattern-models -->

`PatternItems`, `InsightSeries`, `Records` and `Filters` copy their input before returning. `ItemAt` returns Go-owned strings. Stable item IDs survive reordering; reacquire `ItemPart` after model mutations because slots can be reused. Badge `Tone` must be supplied explicitly; the zero value is not a badge tone. See [pattern model contracts](pattern-models.md).

## Find the right API

| Task | Go API |
| --- | --- |
| Appearance and large text | `app.Theme(cui.System)`, `app.TextScale(1.5)`, `widget.Font(...)` |
| Layout | `root.Box(...)`, `root.Grid(...)`, `root.Wrap(...)`, `root.Split(...)` |
| Native text | `widget.SetText(...)`, `widget.Text()`, `widget.SelectedText()` |
| Commands | `app.Command(...)`, `command.Invoke()` |
| Diagnostics and time | `app.Error()`, `cui.Time()` |
| Family names | `cui.PatternName(cui.Chat)` |

All current C functions have Go wrappers or callback bridges. The [main binding source](../../bindings/go/cui.go), [pattern wrappers](../../bindings/go/patterns.go), [search/feedback wrappers](../../bindings/go/search_feedback.go) and [file wrappers](../../bindings/go/files.go) are published with this site. CUI handles remain app-owned; copying a Go wrapper does not clone or extend the native handle's lifetime.

## Icons

Import SVG artwork and use reusable vector or raster assets in native controls. The [icon guide](icons.md) includes ownership rules and examples in C, Python, Go and Zig.

## Custom drawing

See [Drawing and compositing](drawing.md) for surfaces, group opacity, native text, icons, blur and accessible hit regions, with a runnable example in this language.
