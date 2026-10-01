# Performance, measured

The October 1, 2026 optimization pass reduced unnecessary drawing, blur work, pixel conversions, and native control replacement. These are measured **before/after CUI results on one Linux machine**, with complete samples and a runnable harness. Lower milliseconds are better; a ratio below 1 means a regression.

## Hardware and operating system

| Detail | Configuration |
| --- | --- |
| CPU | Intel Core i9-12900K, 24 logical CPUs |
| Memory | 33,335,578,624 bytes reported by Linux (31.05 GiB) |
| OS | Manjaro Linux, rolling, x86_64 |
| Kernel | 7.1.9-1-MANJARO |
| C compiler | GCC 16.2.1 (20260810) |
| CUI build | Release, -O3 -DNDEBUG |
| Harness | -O2, assertions enabled |
| Native backend | GTK 4.22.4; Cairo software renderer |
| Display | Xvfb; GSK_RENDERER=cairo; GDK_DISABLE=gl,vulkan; GTK_A11Y=none |
| Surface | 1280 × 900 logical pixels; 1280 × 900 at 1×, 2560 × 1800 at 2× |
| Scheduling | No CPU pinning or fixed frequency; background desktop workloads may vary |

The 2× surface is **not a full 3840 × 2160 render**. The separate Waypoint 2× check used a 3840 × 2160 virtual display, a 2560 × 1800 backing surface, and enlarged text.

## What is measured

Two sequential before/after pairs, with 11 samples per case per run. The tables use the median of all 22 measured samples per version. Drawing and model cases have two warmups; button construction has one. Raw JSONL includes sorted samples and each run’s median and nearest-rank p95. With 11 samples, that per-run p95 is the maximum; this is a small diagnostic sample, not a statistical confidence interval.

These tests measure synchronous API work with an unshown native window. They exclude native layout/presentation, GPU upload, compositor/scanout, accessibility service costs, sustained FPS, and end-to-end input latency. Button timings exclude App/window creation and destruction. The same machine, compiler options, surface dimensions and case definitions were used for both versions.

## Results at 2×

| Workload | Before (ms) | After (ms) | Before / after |
| --- | --- | --- | --- |
| Full clear | 1.949 | 0.890 | 2.19× |
| 64 rounded rectangles | 40.241 | 34.020 | 1.18× |
| 64 text labels | 4.537 | 3.445 | 1.32× |
| 64 vector icons | 3.002 | 1.704 | 1.76× |
| Panel blur / repeated input | 35.891 | 19.057 | 1.88× |
| Full-surface blur / repeated input | 174.219 | 168.981 | 1.03× |
| Group opacity / small content | 7.726 | 3.256 | 2.37× |
| Straight-RGBA readback | 16.988 | 2.809 | 6.05× |
| GTK canvas submission¹ | 18.688 | <0.01 | — |
| Cached RGBA background blit | 23.364 | 24.799 | 0.94× |
| Small damaged-area update² | 40.188 | 1.131 | 35.55× |
| Panel blur / changing input | 35.423 | 31.337 | 1.13× |

¹ GTK submission now hands an immutable premultiplied pixel buffer to a native texture. The sub-0.01 ms number measures this CPU-side handoff, **not the time to display or upload a frame to the GPU**. RGBA readback is measured separately.

² The same 80 × 40 logical-pixel rectangle changes in a 66-command scene. Before: replay the full scene. After: replay and commit its 84 × 44 damage rectangle. Full-scene rendering still exists and is measured separately. Applications must supply correct damage, including old/new bounds and affected effects.

## Results at 1×

| Workload | Before (ms) | After (ms) | Before / after |
| --- | --- | --- | --- |
| Full clear | 0.277 | 0.113 | 2.45× |
| 64 rounded rectangles | 10.015 | 8.460 | 1.18× |
| 64 text labels | 1.284 | 1.150 | 1.12× |
| 64 vector icons | 0.591 | 0.468 | 1.26× |
| Panel blur / repeated input | 8.554 | 4.586 | 1.87× |
| Full-surface blur / repeated input | 39.243 | 20.885 | 1.88× |
| Group opacity / small content | 0.655 | 0.585 | 1.12× |
| Straight-RGBA readback | 4.293 | 0.663 | 6.48× |
| GTK canvas submission¹ | 4.753 | <0.01 | — |
| Cached RGBA background blit | 5.639 | 5.890 | 0.96× |
| Small damaged-area update² | 10.006 | 0.201 | 49.78× |
| Panel blur / changing input | 8.505 | 7.576 | 1.12× |

## Native models and controls

| Workload | Before (ms) | After (ms) | Before / after |
| --- | --- | --- | --- |
| Replace table rows / 64 | 3.139 | 3.146 | 1.00× |
| Replace table rows / 512 | 10.425 | 10.403 | 1.00× |
| Replace table rows / 4096 | 12.180 | 12.163 | 1.00× |
| 256 unchanged regions | <0.01 | <0.01 | — |
| Change 1 of 256 regions | 3.476 | 0.556 | 6.25× |
| Construct buttons / 32 | 0.473 | 0.485 | 0.98× |
| Construct buttons / 128 | 1.761 | 1.815 | 0.97× |
| Construct buttons / 512 | 6.889 | 7.061 | 0.98× |

## Where gains depend on the workload

Repeated-input panel blur benefits from cached results. The changing-input case changes its background every iteration and therefore misses the cache. The 2× full-surface blur entry exceeds the 32 MiB cache budget, so it remains expensive. Background bitmap blits did not improve consistently and remain an optimization opportunity; all measurements, including slower cases, are included above.

Caches keep at most 32 MiB per surface, with up to another 32 MiB pending during rendering, plus normal frame/layer/scratch allocations. Damage rendering still uses a full-size transactional buffer, so this is not a promise of memory use proportional to damage. Windows/macOS received source changes but were not benchmarked or runtime-verified.

## Original Waypoint workload

The unchanged original single-stage demo was instrumented across 24 scripted states. These are median synchronous frame-work times, including scene construction, C rendering, canvas submission and region updates. They are **not FPS**, and they describe the historical workload snapshot linked below, not the later separate-window simulator.

| Scale | Before (ms) | After (ms) |
| --- | --- | --- |
| 1× | 92.571 | 41.466 |
| 2× | 362.884 | 169.520 |

## Other frameworks

No cross-library ranking is published. [LVGL publishes software-rendering benchmark data](https://lvgl.io/blog/release-v9-6#benchmarks), but it uses different boards, scenes and timing boundaries. [Dear ImGui delegates rendering to an application-provided backend](https://github.com/ocornut/imgui), so a widget-generation timing is not equivalent to CUI’s CPU rasterization. We did not find directly comparable published results for this hardware and workload. Comparing those figures numerically would be misleading.

## Reproduce and inspect

Read the [benchmark instructions](../../benchmarks/README.md) and [C harness](../../benchmarks/performance.c). It requires system GTK development libraries and a Release CUI build; compile the harness with assertions enabled.

```sh
cc -O2 -Wall -Wextra -Werror -Iinclude benchmarks/performance.c \
  build/libcui.a $(pkg-config --cflags --libs gtk4-x11 xext) -lm -o /tmp/cui-perf
GSK_RENDERER=cairo GTK_A11Y=none GDK_DISABLE=gl,vulkan \
  xvfb-run -a /tmp/cui-perf > /tmp/cui-performance.jsonl
```

The [environment manifest](../../benchmarks/results/environment.json) includes static-library and harness hashes. The original static archive is retained locally under `build-perf/before`; rebuilding the current source does not recreate that historical baseline. The current renderer can be measured independently with the command above.

- [Before, run 1](../../benchmarks/results/before-1.jsonl) and [run 2](../../benchmarks/results/before-2.jsonl).
- [After, run 1](../../benchmarks/results/after-1.jsonl) and [run 2](../../benchmarks/results/after-2.jsonl).
- [Original Waypoint timing source](../../benchmarks/results/waypoint-profile.rs), [before 1×](../../benchmarks/results/waypoint-before-1x.log), [after 1×](../../benchmarks/results/waypoint-after-1x.log), [before 2×](../../benchmarks/results/waypoint-before-2x.log), [after 2×](../../benchmarks/results/waypoint-after-2x.log).
- [Pixel equivalence harness](../../benchmarks/raster_equivalence.c): 128 before/after renders matched byte-for-byte.

For API contracts and examples in C, Python, Go, Zig and Rust, see [drawing and compositing](drawing.md#partial-redraws-and-caching).
