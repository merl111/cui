# Build with Python

Create native windows with Python's standard-library `ctypes` binding. No pip runtime dependencies are required. Python 3.9+ is supported; the example below is a complete program with a real button callback, and is run by the Linux test suite.

## Build and run

From the repository root, build the native library once. Linux needs the installed GTK 4 development package; Windows uses system Win32 libraries and macOS uses AppKit. Windows/macOS native verification is deferred.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
PYTHONPATH=bindings/python python3 examples/python/hello.py
```

For a separate project, install the local package with `python3 -m pip install ./bindings/python`. Set `CUI_LIBRARY` to the absolute path of the built shared library if it is outside the repository's build directory. The Python package does not bundle the C library or GTK. On PowerShell, set `$env:PYTHONPATH = "bindings/python"` and run `python examples/python/hello.py`; use `$env:CUI_LIBRARY` if the DLL is in `build/Release`.

## Your first window

Save the following as a Python file. `App` owns the window and widgets; leaving the `with` block destroys them after the event loop returns. The label changes when the button is clicked.

<!-- include-example: examples/python/hello.py python -->

The optional `CUI_SMOKE_TEST` branch activates the same button and checks its result. Run it automatically with `CUI_SMOKE_TEST=1 PYTHONPATH=bindings/python python3 examples/python/hello.py`.

## Handle events and state

`on_action` receives the sending `Widget`. Setters such as `widget.text = ...`, `widget.checked = ...` and `widget.value = ...` are silent: they do not recursively call your callback. `activate()` explicitly invokes an action synchronously and can call your handler before it returns. Preserve a composition's internal callbacks by listening on its root.

Python retains callback functions until app destruction. An exception in a callback stops the event loop and is re-raised by `app.run()`. Call `app.quit()` from a callback; close the app only after `run()` returns. Handles check that the app is alive and calls stay on the main thread. A worker thread must arrange to apply results on that thread; there is no cross-thread dispatch API yet.

## Supply data

Use `RecordFilter`, `PatternItem` and `InsightSeries` for typed data. The [complete Python gallery](../../examples/python/gallery.py) demonstrates a task collection with removal callbacks, numeric record filters, insight datasets, native file/font/color dialogs, token fields and editable tables. It is runnable with:

```sh
PYTHONPATH=bindings/python python3 examples/python/gallery.py
```

This portion is taken directly from that tested gallery:

<!-- include-region: examples/python/gallery.py python pattern-models -->

Models copy strings/arrays before setters return. `item_at` returns a Python-owned snapshot; changing or removing the native item will not invalidate its strings. Check boolean results for rejected IDs, invalid data or capacity limits. Read [pattern model contracts](pattern-models.md) before storing references to item parts, because native slots may be reused after replacement.

## Find the right API

| Task | Python API |
| --- | --- |
| Appearance and large text | `app.theme(cui.SYSTEM)`, `app.text_scale(1.5)`, `widget.font(points=14)` |
| Layout | `root.box()`, `root.grid(...)`, `root.wrap(...)`, `root.split(...)` |
| Focus and accessibility | `widget.focus()`, `widget.accessibility(label, description)` |
| Display scaling | `window.scale` |
| Diagnostics and time | `app.error`, `cui.monotonic_time()` |
| Family names | `cui.pattern_name(cui.CHAT)` |
| All C functions | `cui.lib` has typed declarations for the complete current ABI |

The ordinary `App`, `Window`, `Widget`, `Command`, `Menu`, `Dialog` and `Timer` wrappers handle lifetime checks and callback retention. Use them in application code. Direct `cui.lib` calls require you to manage those rules yourself. Exact wrapper definitions are in the [Python binding source](../../bindings/python/cui/__init__.py), and [shared binding contracts](../bindings.md) cover packaging and platform details.

## Icons

Import SVG artwork and use reusable vector or raster assets in native controls. The [icon guide](icons.md) includes ownership rules and examples in C, Python, Go and Zig.

## Custom drawing

See [Drawing and compositing](drawing.md) for surfaces, group opacity, native text, icons, blur and accessible hit regions, with a runnable example in this language.
