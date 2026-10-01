# CUI performance diagnostics

These Linux diagnostics measure synchronous API work in milliseconds, excluding native presentation/compositor latency. They require a Release CUI build and GTK 4 development libraries. The harness must be compiled **without NDEBUG** because its assertions execute the APIs being measured.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
cc -O2 -Wall -Wextra -Werror -Iinclude benchmarks/performance.c \
  build/libcui.a $(pkg-config --cflags --libs gtk4-x11 xext) -lm -o /tmp/cui-perf
GSK_RENDERER=cairo GTK_A11Y=none GDK_DISABLE=gl,vulkan \
  xvfb-run -a /tmp/cui-perf > /tmp/cui-performance.jsonl
```

Every case emits its median, nearest-rank p95 and all 11 measured samples (sorted), in milliseconds. Drawing/model cases have two warmups; widget construction has one. Scales 1/2 mean 1280×900/2560×1800 physical surface pixels. The virtual window is not shown, so native layout, GPU uploads, scanout, accessibility service costs and sustained FPS are **not** measured. Widget construction excludes App/window creation and destruction.

The original drawing cases retain the same workloads as the initial investigation. `card_blur` repeats identical input and can reuse blur results; `card_blur_changed_input` changes the background each iteration. `full_surface_blur` at 2× exceeds the 32 MiB cache budget. `small_damage_update` changes one 80×40 rectangle in a scene containing 64 other rounded rectangles and commits an 84×44 logical rectangle. Compiling with `-DCUI_PERF_BASELINE` performs a full redraw for the same change, permitting comparison with the pre-optimization static library. Do not label that build as the baseline if it links the current library.

The `results` directory contains dated published measurements and environment metadata. Baseline/optimized static-library hashes identify the binaries used; the old archive is retained locally under `build-perf/before`, not reconstructed by the current source tree. Re-running the current source verifies current performance, not a historical baseline. No cross-library ranking is implied.

`raster_equivalence.c` writes deterministic RGBA output to stdout. Compile/link it separately against both archives, redirect output to two files, and compare them with `cmp` to check exact rendering equivalence across 128 frames. It needs no display server.
