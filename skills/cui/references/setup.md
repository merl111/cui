# Setup and runnable examples

Run commands from the CUI checkout unless a command changes directory. Replace
example paths with the selected checkout when integrating from another project.
Use the consumer's existing build configuration when present.

## Linux and macOS

Linux needs a C compiler, CMake 3.16+, pkg-config, GTK 4.6+ with X11 support and
Xext development files (`libgtk-4-dev libxext-dev` on Debian/Ubuntu). Runtime
machines need GTK and its transitive libraries installed. macOS needs Xcode
command line tools, CMake and AppKit (the code targets a macOS 10.15+ API surface).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCUI_BUILD_SHARED=ON
cmake --build build --parallel
./build/cui_settings
./build/cui_gallery
./build/cui_desktop_gallery
```

On macOS launch the bundle with `open build/cui_settings.app`. Linux/macOS produce
`libcui.a` and, with shared builds enabled, `libcui.so`/`libcui.dylib`.

## Windows

Use Windows 10 1809+, CMake 3.20+, Visual Studio 2022 C++ tools, a current Windows
SDK and `nuget.exe`. The build restores pinned WinUI dependencies. Run in a Visual
Studio developer terminal:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCUI_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\cui_settings.exe
```

Windows uses `cui.dll` and its import library, not a static CUI backend. Ship the
DLL and `Microsoft.WindowsAppRuntime.Bootstrap.dll` beside the executable; install
the matching Windows App Runtime 1.8 and Visual C++ runtime. Build the backend
with MSVC, not MinGW. Go/Zig consumers can use the built DLL through the import
library route in `docs/guides/packaging.md`. Use the PerMonitorV2 manifest example
in `examples/windows.manifest` when packaging consumers.

## Language entry points

### C

Use `docs/guides/getting-started.md` and `examples/settings.c`. Link the CMake
target so platform dependencies propagate:

```cmake
add_subdirectory(path/to/cui)
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE cui)
```

### Python

Requires Python 3.9+ and the native shared library. The wrapper uses standard
library ctypes; `pip install ./bindings/python` installs the local Python package,
not the native library. Start with `docs/guides/python.md`.

```sh
PYTHONPATH=bindings/python python3 examples/python/hello.py
PYTHONPATH=bindings/python python3 examples/python/gallery.py --smoke-test
```

Set `CUI_LIBRARY` to the absolute shared-library path for a nondefault build.
PowerShell uses `$env:PYTHONPATH` and `$env:CUI_LIBRARY`, then invokes `python`.
Use the `cui.App()` context manager to release the app after `app.run()` returns.

### Rust

Requires Rust 1.82+, native development libraries and a built CUI library. Read
`docs/guides/rust.md` and `bindings/rust/examples/hello.rs`.

```sh
cargo run --offline --manifest-path bindings/rust/Cargo.toml --example hello
cargo run --offline --manifest-path bindings/rust/Cargo.toml --example daylight
```

Add `cui = { path = "/absolute/path/to/cui/bindings/rust" }` to a consumer's Cargo
dependencies. Set `CUI_LIB_DIR` for a nondefault library directory; defaults are
`build` on Linux/macOS and `build/Release` on Windows, relative to the CUI checkout.
Linux/macOS link the static archive; Windows links the DLL. Handles are neither
Send nor Sync, and cloned widgets still refer to the same app-owned object.

### Go

Read `docs/guides/go.md`. Build the native library first, enable cgo and keep UI
work on the locked main OS thread (`runtime.LockOSThread()`).

```sh
cd bindings/go
go run -buildvcs=false ./cmd/hello
go run -buildvcs=false ./cmd/gallery --smoke-test
```

Consumers can use `replace cui.local/cui => /absolute/path/to/cui/bindings/go`.
For a custom Linux/macOS archive location, use the `cui_external` build tag and
`CGO_LDFLAGS=/absolute/path/libcui.a`. See the packaging guide for Windows cgo
import libraries. Close the app only after `Run` returns.

### Zig

Read `docs/guides/zig.md`; the supplied build is tested with Zig 0.16.0. It
compiles CUI sources directly on Linux/macOS and exposes the raw ABI as `ui.c`.

```sh
zig build run
CUI_SMOKE_TEST=1 zig build run
zig build -Dexample=drawing run
```

Windows consumes prebuilt WinUI libraries via
`zig build -Dtarget=x86_64-windows-gnu -Dcui-lib-dir=build/Release`.
Keep callback context alive and call app `deinit` after `run`. Struct copies of
owned icon/surface handles do not automatically retain references.

## Tests and documentation

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCUI_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build -N
ctest --test-dir build --output-on-failure
```

Layout tests do not require a display. Linux GUI tests require `xvfb-run`; optional
language tests require `CUI_BUILD_BINDING_TESTS=ON` and the relevant toolchains.
Run GUI tests serially unless their display isolation has been established.
For public ABI changes, run:

```sh
python3 tools/check_bindings.py
python3 tools/generate_rust_bindings.py --check
python3 tools/check_rust_abi.py
```

The static docs generator needs Python 3.12+ and the docs checker needs the built
showcase. See `docs/site/README.md` before changing catalogs or captures.

```sh
python3 tools/build_docs.py
python3 tools/check_docs.py
python3 -m http.server 8080 --directory build/docs
```

The generated site provides `llms.txt`, `llms-full.txt`, `api.json` and
`catalog.json`. Author source guides, public header comments and catalog entries;
do not edit generated `build/docs` output.
