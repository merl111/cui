# Linking and packaging

Windows applications can embed their app icon as numeric `ICON` resource **1**
in the executable. Both Windows backends load that optional resource into their
window class (large and small icons), so the running window uses the same identity
as Explorer. `LR_SHARED` keeps native icon ownership with the executable module.
Applications without that resource keep the operating system's default behavior.

Linux and macOS can embed CUI as a static library. Windows uses a WinUI 3 DLL with a stable C calling convention; it requires the Windows App SDK runtime. Rust, Go and Zig link that DLL on Windows, while Python loads it with ctypes.

Static CUI on Linux/macOS still depends on the platform UI libraries. Linux always uses GTK; there is no `CUI_NO_GTK` option.

## Choose a distribution model

| Target | What goes into your executable | What remains outside it |
| --- | --- | --- |
| Windows | Your Rust/Go/C application | `cui.dll`, bootstrap DLL, Windows App Runtime 1.8 and Visual C++ runtime |
| macOS | CUI and your Rust/Go/C application | System AppKit, QuartzCore and their dependencies |
| Linux with system GTK | CUI and your Rust/Go/C application | GTK 4.6+ with X11 support, Xext and their runtime dependencies |
| Linux without a system GTK installation | Static CUI, plus GTK provided by an application package or managed runtime | A compatible host OS, display server and graphics stack; packaging is not implemented here |
| Linux without GTK anywhere | Not supported | Requires a new Linux backend |

Build tools belong on the developer machine. End users do not need Cargo, Go, a C compiler, headers or pkg-config. They do need the runtime libraries indicated above and any application assets you use.

The Linux Rust and Go recipes below were built and their dynamic dependencies inspected on October 1, 2026, on x86_64 Manjaro with GCC 16.2.1 and GTK 4.22.4. Windows/macOS recipes reflect the current source configuration; native verification on those platforms remains deferred.

## C and CMake

The `cui` target is static on Linux/macOS and a DLL import target on Windows. `CUI_BUILD_SHARED=OFF` disables the additional shared-library target; `BUILD_SHARED_LIBS=OFF` alone does not control that target. In your application's CMake project, use:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_app LANGUAGES C)

if(NOT WIN32)
    set(CUI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
endif()
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
    add_custom_command(TARGET my_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:cui>" "$<TARGET_FILE_DIR:my_app>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE_DIR:cui>/Microsoft.WindowsAppRuntime.Bootstrap.dll" "$<TARGET_FILE_DIR:my_app>")
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

Windows widgets use WinUI 3 and Fluent design. Win32 remains the window host and supplies shell file dialogs, clipboard and bitmap text/image services. The minimum OS is Windows 10 1809. The public ABI stays C; consumers do not need to compile C++ or use XAML.

### Build and deploy the DLL

Install Visual Studio 2022 with Desktop development with C++, a current Windows SDK, CMake 3.20+, and NuGet CLI. From a developer terminal:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCUI_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix stage
```

`windows/packages.config` pins C++/WinRT and Windows App SDK 1.8 components. Restore requires network access; `CUI_WINUI_PACKAGES` selects a package cache. The default output is `build/Release/cui.dll` with `cui.lib` as its import library. `CUI_BUILD_SHARED=OFF` is rejected on Windows. MSVC builds the backend; MinGW consumers link its C ABI DLL.

Ship `cui.dll` and `Microsoft.WindowsAppRuntime.Bootstrap.dll` next to the app. Install the matching architecture's [Windows App Runtime 1.8](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/downloads) and Visual C++ Redistributable. This is framework-dependent deployment, not a self-contained single executable. A packaged application must declare its Windows App SDK framework dependency; the bootstrap path is for unpackaged desktop apps. Test the actual package on a clean Windows machine.

### Rust

```powershell
$env:CUI_LIB_DIR = "$PWD/build/Release"
$env:PATH = "$env:CUI_LIB_DIR;$env:PATH"
cargo build --offline --release --target x86_64-pc-windows-msvc `
  --manifest-path bindings/rust/Cargo.toml --example hello
```

For distribution, copy both DLLs beside the resulting executable instead of relying on PATH. Rust uses `dylib=cui` on Windows. Static CRT flags do not remove the WinUI runtime dependency.

### Go with MinGW-w64

Build the DLL with MSVC first. Then use an architecture-matched MinGW-w64 `dlltool` to create a GNU import library:

```sh
dlltool -d windows/cui.def -D cui.dll -l build/Release/libcui.dll.a
cd bindings/go
go build -buildvcs=false -o ../../build/Release/go-hello.exe ./cmd/hello
```

The binding searches `build/Release` on Windows. For another directory use `-tags cui_external` and `CGO_LDFLAGS='-L/path/to/dll-directory -lcui'`. cgo needs GCC-compatible tooling, but does not compile the WinUI backend. The generated import library can also be used by Windows GNU Rust consumers. Keep the architecture and exported function names consistent.

### Zig and Python

Zig consumes the prebuilt import library:

```powershell
zig build -Dtarget=x86_64-windows-gnu -Dcui-lib-dir=build/Release -Dexample=hello
```

The install step copies the CUI and bootstrap DLLs to `zig-out/bin`. Cross-building requires those matching Windows artifacts in advance. Python uses `CUI_LIBRARY` pointing at the absolute `cui.dll` path; keep its bootstrap DLL alongside it.

### Application resources and validation

Embed or merge the supplied PerMonitorV2 manifest before signing external applications. Its Common Controls declaration does not change WinUI styling. WinUI controls use their own Fluent resources.

The Windows CI job restores the SDK, installs the runtime and runs the WinUI contract test. Native compilation, interaction and appearance still require a successful Windows run; Linux regression results do not establish those properties.

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

A static Linux/macOS CUI build has no dependency on `libcui.so` or `libcui.dylib`. Windows always requires `cui.dll`. Linux builds above still list GTK and other system libraries; that is expected. Windows imports should be checked for additional compiler-runtime DLLs. These tools list link dependencies, not every dynamically loaded module, data file or font. Finally launch the packaged application on a clean target machine that represents your supported baseline.

## Other bindings and common failures

Zig compiles CUI's C sources on Linux/macOS and links the prebuilt WinUI DLL on Windows. See the [Zig guide](zig.md). Python's current ctypes binding loads a shared CUI library and cannot load a static archive. Keep `CUI_BUILD_SHARED=ON` for that binding and use `CUI_LIBRARY` to locate the shared library when needed; see the [Python guide](python.md).

| Symptom | Check |
| --- | --- |
| Rust cannot find `cui` | Build the archive first; set `CUI_LIB_DIR` to its directory, not the file; account for `Release` under multi-configuration generators |
| Go cannot find the default archive | Supply `-tags cui_external` and an absolute archive path in `CGO_LDFLAGS` |
| `gtk4-x11` or `xext` not found | Install matching development packages; for cross-builds use a target SDK and target-aware pkg-config configuration |
| Wrong file format or machine type | Rebuild CUI and the application for the same architecture, ABI and compiler family |
| MSVC runtime mismatch | Align `/MT` or `/MD` with the consumer's CRT configuration and rebuild the archive |
| GTK loader error on another Linux machine | Install the runtime dependencies or use a validated bundled/runtime package; a static CUI archive does not include GTK |

## Keep size measurements honest

Measure the final application and any packaged dependencies separately. Archive size is not the size added to every executable: the linker can discard unused objects, and the language runtime and application assets have their own cost. Windows builds restore SDK dependencies; account separately for the installed Windows App Runtime.

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
