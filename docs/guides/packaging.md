# Static linking and packaging

You can embed CUI directly in a C, Rust or Go executable on Windows, macOS and Linux. The user does not need a separate CUI library file. Rust and Go already link CUI statically by default; the commands below make the archive location and release configuration explicit.

**Static CUI does not mean a completely static executable.** Windows and macOS still use their operating-system libraries. The current Linux backend always uses GTK. There is no GTK-free Linux backend or `CUI_NO_GTK` build option.

## Choose a distribution model

| Target | What goes into your executable | What remains outside it |
| --- | --- | --- |
| Windows | CUI and your Rust/Go/C application | Windows system DLLs; compiler runtime dependencies depend on your toolchain/settings |
| macOS | CUI and your Rust/Go/C application | System AppKit, QuartzCore and their dependencies |
| Linux with system GTK | CUI and your Rust/Go/C application | GTK 4.6+ with X11 support, Xext and their runtime dependencies |
| Linux without a system GTK installation | Static CUI, plus GTK provided by an application package or managed runtime | A compatible host OS, display server and graphics stack; packaging is not implemented here |
| Linux without GTK anywhere | Not supported | Requires a new Linux backend |

Build tools belong on the developer machine. End users do not need Cargo, Go, a C compiler, headers or pkg-config. They do need the runtime libraries indicated above and any application assets you use.

The Linux Rust and Go recipes below were built and their dynamic dependencies inspected on October 1, 2026, on x86_64 Manjaro with GCC 16.2.1 and GTK 4.22.4. Windows/macOS recipes reflect the current source configuration; native verification on those platforms remains deferred.

## C and CMake on all three platforms

The `cui` target is always a static archive. `CUI_BUILD_SHARED=OFF` disables the additional shared-library target; `BUILD_SHARED_LIBS=OFF` alone does not control that target. In your application's CMake project, use:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES C)

set(CUI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(CUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CUI_BUILD_BINDING_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/cui cui-build)
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE cui)

if(WIN32)
    enable_language(RC)
    target_sources(my_app PRIVATE path/to/cui/examples/windows.rc)
    target_include_directories(my_app PRIVATE path/to/cui/examples)
endif()
```

Replace `path/to/cui` with the source location. The target propagates public headers and platform link libraries. On macOS it also enables Objective-C for the private backend. On Linux configuration requires GTK development files even when the CUI archive is static. CUI does not currently install a CMake package configuration for `find_package(CUI)`.

For a standalone archive on Linux or macOS, run from the CUI repository root:

```sh
cmake -S . -B build-static-docs -DCMAKE_BUILD_TYPE=Release \
  -DCUI_BUILD_SHARED=OFF -DCUI_BUILD_EXAMPLES=OFF \
  -DCUI_BUILD_TESTS=OFF -DCUI_BUILD_BINDING_TESTS=OFF
cmake --build build-static-docs --parallel --target cui
```

The result is `build-static-docs/libcui.a`. No GTK, AppKit or Windows system libraries are copied into this archive. Always match the archive's architecture, ABI and compiler runtime to the consuming application. The build-directory name is arbitrary; keep it consistent in subsequent commands.

## Linux with system GTK

Install a C compiler, CMake, pkg-config, GTK 4.6+ development files with X11 support, and Xext development files through your distribution. Check discovery before running the archive build above:

```sh
pkg-config --modversion gtk4-x11 xext
```

Build on the oldest supported distribution baseline, or in a matching build environment. A binary built against a newer glibc or GTK can require symbols unavailable on older machines; the source's GTK minimum does not establish the compatibility of every produced binary.

### Rust

From the repository root, after building the archive:

```sh
CUI_LIB_DIR="$PWD/build-static-docs" \
CARGO_TARGET_DIR="$PWD/build-static-docs/rust" \
cargo build --offline --release \
  --manifest-path bindings/rust/Cargo.toml --example hello

./build-static-docs/rust/release/examples/hello
```

Ship the executable, plus your own assets. There is no need to ship `libcui.so`, `libcui.a` or the Rust toolchain. The binding's build script explicitly requests `static=cui` and discovers system GTK libraries through `pkg-config --libs gtk4-x11 xext`.

For your own project, add `cui = { path = "/absolute/path/to/cui/bindings/rust" }` to `[dependencies]`, set `CUI_LIB_DIR` as above and run `cargo build --release` in that project. The crate does not compile the C archive for you. See the [Rust quickstart](rust.md) for complete application code.

### Go

From the repository root, after building the archive:

```sh
cui_root="$PWD"
(
  cd bindings/go
  CGO_ENABLED=1 CGO_LDFLAGS="$cui_root/build-static-docs/libcui.a" \
    go build -buildvcs=false -tags cui_external -trimpath \
    -o "$cui_root/build-static-docs/go-hello" ./cmd/hello
)
./build-static-docs/go-hello
```

`cui_external` disables only the binding's default `build/libcui.a` path; it does not disable platform libraries. `CGO_LDFLAGS` supplies the replacement archive. Keep cgo enabled: `CGO_ENABLED=0` cannot build this binding. A suitable C compiler must be available during the build. These are cgo build requirements, not tools to install on the end user's machine. See the [official cgo documentation](https://pkg.go.dev/cmd/cgo).

For a separate Go module, first follow the local `require`/`replace` setup in the [Go quickstart](go.md), then use the same environment and `-tags cui_external` with your own main package. `-buildvcs=false` only works around this checkout's missing Git metadata and can be omitted in a normal repository.

### What users must have installed

Both executables still need GTK, Xext and their runtime dependencies. A distribution package should declare those dependencies so the package manager installs them. Copying the executable to a machine without GTK will not work. Development headers and `.pc` files are only needed when compiling.

## Linux without system GTK, or without GTK at all

There are two different requirements here:

- **No GTK installation step for the user:** possible by distributing GTK with your application, or using a managed runtime that supplies it. An AppImage or private application directory can carry runtime libraries and data; Flatpak can supply GTK through its runtime. CUI remains statically linked into your application. This repository does not currently build those packages. See [AppImage's packaging model](https://docs.appimage.org/introduction/concepts.html) and [Flatpak's runtime model](https://docs.flatpak.org/en/latest/basic-concepts.html).
- **No GTK dependency, even inside the package:** not possible with the current Linux implementation. The controls, text, windows and drawing backend call GTK. A different renderer/windowing backend would have to be implemented first. Disabling shared CUI, using Rust's musl target or turning off cgo does not replace that backend.

### Statically linking GTK itself

This is a separate, advanced toolchain project, not a supported CUI build mode. It still uses GTK, even if there is no `libgtk-4.so` next to the executable. There is no tested one-command recipe in this repository.

You would need a compatible static GTK SDK, static archives for the dependencies selected by that GTK build, appropriate linker ordering and runtime data/module handling. GTK's own build has multiple dependencies and configurable backends; see its [build documentation](https://docs.gtk.org/gtk4/building.html). Retain X11 support for CUI's current `gtk4-x11` and Xext requirements, including when targeting Wayland sessions.

`pkg-config --static --libs gtk4-x11 xext` can expose additional private link dependencies; it does not build missing archives or force the linker to choose them. The current Rust build script invokes ordinary `--libs`, and the Go binding uses cgo's ordinary pkg-config integration. Neither provides a complete static-GTK switch. Changing those link settings, preparing the SDK and testing its runtime behavior would be additional engineering work. Review redistribution requirements for any dependencies you choose to include.

For now, use the tested static-CUI/system-GTK path. If avoiding a GTK installation step becomes a release requirement, build and validate a packaging workflow before advertising a standalone Linux download.

## Windows

CUI uses Win32 and system GDI+, not GTK. The backend currently targets Windows 10 1703+ APIs; Windows 11 enables newer chrome features. Build the C archive with the same architecture and toolchain family as your application. A MinGW `libcui.a` and an MSVC `cui.lib` are not interchangeable inputs for these recipes.

### Rust with MSVC and a static C runtime

Use an x64 Visual Studio Developer PowerShell with the Windows SDK, CMake, Ninja and the `x86_64-pc-windows-msvc` Rust toolchain available. Run from the repository root:

```powershell
cmake -S . -B build-msvc -G Ninja -DCMAKE_C_COMPILER=cl `
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded `
  -DCUI_BUILD_SHARED=OFF -DCUI_BUILD_EXAMPLES=OFF `
  -DCUI_BUILD_TESTS=OFF -DCUI_BUILD_BINDING_TESTS=OFF
cmake --build build-msvc --target cui

$env:CUI_LIB_DIR = "$PWD/build-msvc"
$env:CARGO_TARGET_DIR = "$PWD/build-msvc/rust"
$env:RUSTFLAGS = "-C target-feature=+crt-static"
cargo build --offline --release --target x86_64-pc-windows-msvc `
  --manifest-path bindings/rust/Cargo.toml --example hello

mt.exe -manifest examples/windows.manifest `
  "-outputresource:build-msvc/rust/x86_64-pc-windows-msvc/release/examples/hello.exe;#1"
```

The executable is `build-msvc/rust/x86_64-pc-windows-msvc/release/examples/hello.exe`. CMake's `MultiThreaded` selects `/MT` for CUI, and Rust's `+crt-static` selects the corresponding runtime linkage. Keep these choices consistent across foreign code. See [CMake's runtime setting](https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html) and [Rust's C runtime linkage](https://doc.rust-lang.org/reference/linkage.html#static-and-dynamic-c-runtimes). The flags above replace any existing `RUSTFLAGS` for this shell; combine them with your project settings if needed.

If you intentionally use a dynamic MSVC runtime, use `MultiThreadedDLL` and Rust's matching dynamic runtime configuration instead, then account for its redistribution requirements. Static CUI alone does not select a static CRT. With a multi-configuration generator instead of Ninja, use `--config Release` and point `CUI_LIB_DIR` at the directory containing `cui.lib`, usually `build-msvc/Release`.

### Go with MinGW-w64

Use a native Windows MSYS2 UCRT64 shell with matching x86_64 GCC, windres, CMake, Ninja and Windows Go on PATH. Start at the repository root; use a checkout path without spaces for this shell example:

```sh
cmake -S . -B build-mingw -G Ninja -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_BUILD_TYPE=Release -DCUI_BUILD_SHARED=OFF \
  -DCUI_BUILD_EXAMPLES=OFF -DCUI_BUILD_TESTS=OFF \
  -DCUI_BUILD_BINDING_TESTS=OFF
cmake --build build-mingw --target cui

windres -I examples examples/windows.rc -O coff \
  -o bindings/go/cmd/hello/cui_windows_amd64.syso
cui_root=$(cygpath -m "$PWD")
(
  cd bindings/go
  CGO_ENABLED=1 CC=gcc GOOS=windows GOARCH=amd64 \
    CGO_LDFLAGS="$cui_root/build-mingw/libcui.a -static-libgcc" \
    go build -buildvcs=false -tags cui_external -trimpath \
    -o "$cui_root/build-mingw/go-hello.exe" ./cmd/hello
)
```

Go includes the generated architecture-specific `.syso` resource from the main package. For your own application, put that resource in your main package instead. `-static-libgcc` requests GCC's static support library; it is not a promise that every dependency is static. Inspect the executable's DLL imports before shipping. The Go binding links the Win32 system libraries itself, and cgo needs a GCC-compatible compiler rather than an MSVC-built CUI archive.

### Windows application resources

The supplied `examples/windows.manifest` enables Common Controls v6 and PerMonitorV2 DPI behavior. Embedding CUI does not automatically embed that manifest in an external Rust or Go executable. The examples above embed it explicitly; merge its settings into your own application manifest if you already have one. Modify resources before signing the executable. For GUI-only applications you may also configure the Windows GUI subsystem; that is independent of static linkage.

## macOS

Install the Xcode command-line tools and CMake, then build `libcui.a` using the standalone archive command above. No GTK is used. AppKit and QuartzCore are system frameworks and remain dynamically linked.

The native-host Rust and Go commands in the Linux sections work unchanged on macOS after this archive build; GTK development files and pkg-config are not required there. They produce `build-static-docs/rust/release/examples/hello` and `build-static-docs/go-hello` respectively. Run every step with the same architecture: use native arm64 tools on Apple Silicon or native x86_64 tools on Intel. A universal application requires separate, matching builds for both architectures before combining the final executables.

For an explicit Apple Silicon archive, add `-DCMAKE_OSX_ARCHITECTURES=arm64` to CMake, use Rust's `--target aarch64-apple-darwin` or Go's `GOARCH=arm64`, and update the Cargo output path to include its target triple. For Intel, use `x86_64`, `x86_64-apple-darwin` and `amd64` respectively. Set a consistent deployment target across CMake, Rust and cgo for the oldest macOS release you intend to support; verify on that release.

Distribute the executable inside an `.app` bundle when appropriate, with `Info.plist`, icons and application assets. Signing and notarization are separate release steps. There is no `libcui.dylib` to ship with these static builds, and no need to install a Rust or Go runtime. Do not add a global `-static` flag: the OS frameworks are expected to stay external.

## Verify the binary you will ship

Inspect the final executable, not just the archive. Run these against your own build outputs:

```sh
# Linux: direct dynamic dependencies
readelf -d build-static-docs/rust/release/examples/hello
readelf -d build-static-docs/go-hello

# macOS: linked libraries and frameworks
otool -L build-static-docs/rust/release/examples/hello
otool -L build-static-docs/go-hello
```

On Windows, use `dumpbin /DEPENDENTS path\to\app.exe` in the Developer shell, or `objdump -p path/to/app.exe` with MinGW and inspect its DLL imports.

A static CUI build has no dependency on `libcui.so`, `libcui.dylib` or `cui.dll`. Linux builds above still list GTK and other system libraries; that is expected. Windows imports should be checked for additional compiler-runtime DLLs. These tools list link dependencies, not every dynamically loaded module, data file or font. Finally launch the packaged application on a clean target machine that represents your supported baseline.

## Other bindings and common failures

Zig's existing build compiles CUI's C sources into its application; it still uses the same platform libraries. See the [Zig guide](zig.md). Python's current ctypes binding loads a shared CUI library and cannot load a static archive. Keep `CUI_BUILD_SHARED=ON` for that binding and use `CUI_LIBRARY` to locate the shared library when needed; see the [Python guide](python.md).

| Symptom | Check |
| --- | --- |
| Rust cannot find `cui` | Build the archive first; set `CUI_LIB_DIR` to its directory, not the file; account for `Release` under multi-configuration generators |
| Go cannot find the default archive | Supply `-tags cui_external` and an absolute archive path in `CGO_LDFLAGS` |
| `gtk4-x11` or `xext` not found | Install matching development packages; for cross-builds use a target SDK and target-aware pkg-config configuration |
| Wrong file format or machine type | Rebuild CUI and the application for the same architecture, ABI and compiler family |
| MSVC runtime mismatch | Align `/MT` or `/MD` with the consumer's CRT configuration and rebuild the archive |
| GTK loader error on another Linux machine | Install the runtime dependencies or use a validated bundled/runtime package; a static CUI archive does not include GTK |

## Keep size measurements honest

Measure the final application and any packaged dependencies separately. Archive size is not the size added to every executable: the linker can discard unused objects, and the language runtime and application assets have their own cost. Source builds do not vendor UI dependencies.

The October 1, 2026 Linux Release snapshot measured `libcui.so` at 278,160 bytes (about 272 KiB) after stripping, with GCC 16.2.1 on x86_64 Manjaro. This historical shared-library measurement excludes GTK/X11, application code and assets; it is not a size guarantee for statically linked applications.

## Build the documentation

The documentation website uses Python 3.12+ and its standard library as build tooling and plain HTML/CSS/JavaScript at runtime. No npm packages, network font requests or CDN scripts are required.

```sh
python3 tools/build_docs.py
python3 tools/check_docs.py
python3 -m http.server 8080 --directory build/docs
```

The generated directory is suitable for any static host. Source documentation, examples and the catalog stay in this repository. To refresh native Linux images, build `cui_showcase` and run the capture tool inside an X11 session or Xvfb. ImageMagick's `import` is capture tooling, not a CUI runtime dependency.

```sh
GSK_RENDERER=cairo GTK_A11Y=none GSETTINGS_BACKEND=memory \
  xvfb-run -a -s '-screen 0 1920x1200x24' python3 tools/capture_showcase.py
```
