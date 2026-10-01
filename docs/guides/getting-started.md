# Your first native window

CUI is a small C99 library that uses the operating system's UI libraries. Your application describes controls, content and semantic roles; the backend creates GTK controls on Linux, AppKit views on macOS and Win32 controls on Windows. There is no browser runtime, bundled font or downloaded UI dependency.

## Build the library

For Linux development, install a C compiler, CMake 3.16 or newer, pkg-config and GTK 4.6 or newer development files with X11 support, and Xext development files (`pkg-config gtk4-x11 xext`) through your system package manager. The target computer also needs the GTK runtime. System libraries are dependencies, but CUI does not bundle them.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/cui_settings
./build/cui_showcase button
```

The build produces a static archive, a shared library and native example applications. Python loads the shared library; Go's default development configuration links the static archive. Zig's build compiles the C sources directly.

To embed CUI in the executable you ship, see [static linking and packaging](packaging.md) for CMake, Rust and Go recipes on Windows, macOS and Linux, including the Linux GTK requirements.

## Assemble a window

Create one application on the main thread. A window comes with a vertical root box. Append children in their visual and keyboard order. Register callbacks before entering the event loop.

```c
#include "cui.h"

static void clicked(cui_widget *sender, void *data) {
    (void)sender;
    cui_set_text((cui_widget *)data, "Ready to go.");
}

int main(void) {
    cui_app *app = cui_app_create();
    if (!app) return 1;
    cui_window *window = cui_window_create(app, "Hello", 480, 260);
    if (!window) { cui_app_destroy(app); return 1; }
    cui_widget *root = cui_window_root(window);
    cui_widget *status = cui_label(root, "Built with CUI.");
    cui_widget *button = cui_button(root, "Get started");
    if (!status || !button) { cui_app_destroy(app); return 1; }
    cui_set_role(button, CUI_ROLE_PRIMARY);
    cui_on_action(button, clicked, status);
    cui_window_show(window);
    cui_app_run(app);
    cui_app_destroy(app);
    return 0;
}
```

This follows the same lifecycle as the compiled settings and showcase examples. In a CMake consumer, add the CUI source directory and link your application against the `cui` target; its system link requirements propagate to your target.

```cmake
add_subdirectory(path/to/cui)
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE cui)
```

## Choose the right entry point

Use the core header for basic controls. Include `cui_layouts.h` for grids, wrapping rows and split panes; `cui_navigation.h` for trees and breadcrumbs; `cui_inputs.h` for numeric/date/time inputs and validation; `cui_desktop.h` for commands, menus and dialogs. The API reference is generated from every public header and keeps the exact declarations.

The component catalog includes a compiled C recipe and a real Linux screenshot for each entry. Launch any entry by passing its stable ID to `cui_showcase`. `--list` prints all IDs. The Python, Go and Zig galleries cover the same shared C ABI and provide working callback examples.

## Current development scope

CUI remains under active development. Implemented controls and incomplete pattern compositions are labeled separately. Windows and macOS implementations continue to be developed; native verification on those platforms is a later phase, not a gate on framework or documentation development. Avoid treating screenshots from Linux as evidence of how another backend renders.
