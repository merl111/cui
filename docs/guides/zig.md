# Build with Zig

CUI provides allocation-free convenience wrappers for the complete C API, plus the raw ABI as `ui.c`. Use typed `App`, `Window`, `Widget`, `Command`, `Menu`, `Dialog` and `Timer` handles. The repository build is tested with Zig 0.16.0 and fetches no Zig packages.

## Build and run

The Zig build compiles the C backend directly; a separate CMake build is not required. Linux needs the installed GTK 4 development package. From the repository root:

```sh
zig build -Dexample=hello run
# Exercise the same button callback automatically:
CUI_SMOKE_TEST=1 zig build -Dexample=hello run
# Explore the full gallery:
zig build run
```

`-Dexample=hello` selects `examples/zig/hello.zig`; the default selects the gallery. `-Dexample=bindings` runs the convenience-wrapper integration test. The build links system GTK or AppKit. Windows requires a prebuilt WinUI DLL/import library selected with `-Dcui-lib-dir=build/Release`; see [packaging](packaging.md#windows). Windows/macOS native verification is deferred.

To embed the binding in your own build, use [build.zig](../../build.zig) as the platform-linking reference. Register [cui.zig](../../bindings/zig/cui.zig) as a module named `cui`, give it this repository's `include` path, and link the C library or compile the backend sources as the supplied build does. There is no published Zig package dependency URL yet.

## Your first window

This complete program creates a native window and changes a label when its button is pressed:

<!-- include-example: examples/zig/hello.zig zig -->

`try` propagates initialization/allocation failures. `Demo` stays on the stack for the entire blocking `app.run()` call, so callback userdata remains alive. C callbacks use `callconv(.c)` and cannot propagate Zig errors; handle fallible operations inside them. The optional environment branch runs a native click check for CI.

## Native types with convenient operations

The wrapper uses `[:0]const u8` for strings, `[]const` slices for arrays, `bool` for state and optional handles for missing parts. It does not allocate a Zig-side widget tree. Native constructors and model setters copy their input strings/arrays where the C contract promises that.

Use `widget.getText(buffer)` for text. It returns the required UTF-8 byte count excluding the trailing NUL; a short buffer may split a UTF-8 character. Pass an empty slice to query the required length, then provide that many bytes plus one. CUI does not allocate the destination buffer for you.

All handles are owned by `App`. Keep calls on the main thread and call `app.quit()` inside callbacks. Call `deinit()` only after `run()` returns. Zig does not check use-after-destruction at runtime. Ordinary setters are silent; explicit `activate()`/`invoke()` operations may call back before returning.

## Models and events

The [complete Zig gallery](../../examples/zig/gallery.zig) shows tasks, filters, datasets, native dialogs and token fields. This is its tested model setup:

<!-- include-region: examples/zig/gallery.zig zig pattern-models -->

Use C structures such as `ui.c.cui_pattern_item` to describe models and enum constants. `patternItems`, `records`, `filters` and `insightSeries` accept slices. `records` takes flat name/status/detail triples and rejects incomplete rows. `table.rows` also checks its stored column count.

`tableEvent()` returns `{ kind, row, column }` and `treeEvent()` returns `{ kind, id }`. Read these in root action callbacks. Unlike Go/Python snapshots, `itemAt()` returns borrowed C string pointers valid only until the next model mutation. Copy them if they must outlive that boundary. Reacquire `itemPart` after replacement. See [pattern model contracts](pattern-models.md).

## Layout, commands and state

| Task | Zig API |
| --- | --- |
| Appearance and large text | `app.theme(...)`, `app.textScale(1.5)`, `widget.font(...)` |
| Layout | `root.grid(...)`, `grid.gridCell(...)`, `root.wrapping(...)`, `root.split(...)` |
| Disclosure | `root.disclosure(...)`, `disclosure.disclosureContent()`, `setExpanded(...)` |
| State and focus | `setChecked(...)`, `isChecked()`, `setEnabled(...)`, `focus()`, `hasFocus()` |
| Choice controls | `root.select(...)`, `root.list(...)`, `widget.setItems(...)` |
| Commands and menus | `app.command(...)`, `app.menu()`, `menu.add(...)`, `window.menu(...)`, `root.toolbar(...)` |
| Diagnostics and time | `app.errorText()`, `ui.monotonicTime()` |
| Family names | `ui.patternName(ui.c.CUI_CHAT)` |

`wrapping` creates a wrapping layout; `Widget.wrap` wraps an existing C handle. `switchControl` avoids Zig's `switch` keyword. `Command` has C layout with one native pointer, allowing an allocation-free command slice for toolbars. `app.errorText()` returns borrowed native diagnostic text; copy it if a later error might replace it. Raw `ui.c` remains available for C interop; convenience wrappers cover every current function.

## Icons

Import SVG artwork and use reusable vector or raster assets in native controls. The [icon guide](icons.md) includes ownership rules and examples in C, Python, Go and Zig.

## Custom drawing

See [Drawing and compositing](drawing.md) for surfaces, group opacity, native text, icons, blur and accessible hit regions, with a runnable example in this language.
