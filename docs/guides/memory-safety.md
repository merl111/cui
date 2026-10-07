# Memory safety and leak checks

CUI uses several independent checks. Passing them means the exercised paths had no reported errors; it does not prove that every C execution is safe. These are development tools, not shipped library dependencies. The current executed backend is Linux/GTK. Windows and macOS verification is deferred.

## Run the checks

Install Clang, compiler-rt/libFuzzer, scan-build (often packaged as clang-tools), Valgrind, matching glibc/toolkit debug symbols, CMake, GTK 4 development files, Python 3, D-Bus and Xvfb. Debian/Ubuntu packages include `clang clang-tools valgrind libc6-dbg libgtk-4-dev xvfb dbus`. No Python or Cargo packages are needed by the safety runner.

From the repository root:

```sh
python3 tools/check_memory.py sanitizer
python3 tools/check_memory.py analyzer
python3 tools/check_memory.py fuzz --seconds 60
python3 tools/check_memory.py valgrind
# Run all stages, preserving failures while continuing independent checks:
python3 tools/check_memory.py all
```

The [runner](../../tools/check_memory.py) writes `build-memory/results.json`, command logs under `build-memory/logs`, HTML analyzer reports, and separate fuzz corpora/crash artifacts for each harness. Exit status is nonzero for findings, missing tools, command failures or timeouts. It never turns an infrastructure failure into a passing test. `--build-dir`, `--jobs`, `--timeout` and `--seconds` configure build location and budgets. The timeout applies to each command, not the complete workflow.

The [Linux safety workflow](../../.github/workflows/memory.yml) runs four independent jobs and uploads diagnostics even when a job fails. It pins Ubuntu 24.04 and installs matching GTK, GLib, Pango, Cairo, Fontconfig and librsvg symbols so allocation-specific exceptions can resolve internal functions. Missing symbols remain a setup failure; they are not grounds for disabling leak detection.

Do not run concurrent Xvfb jobs from separate commands on one host. CTest serializes its own GUI tests. Sandboxed environments must allow the virtual display server and LeakSanitizer's process/thread inspection; a LeakSanitizer ptrace failure is a tooling failure, not evidence of a CUI leak or a pass.

## What each tool covers

- **AddressSanitizer (ASan)** detects exercised out-of-bounds accesses, use-after-free, double free and related memory errors. **LeakSanitizer (LSan)** checks unreachable allocations at process exit. The runner enables leak detection explicitly.
- **UndefinedBehaviorSanitizer (UBSan)** detects many exercised C errors, including invalid shifts, signed overflow, alignment and null-pointer violations. Both compiler instrumentation and runtime options stop at the first error.
- **Valgrind Memcheck** uses an independent execution engine and can detect reads of uninitialized memory in code that was not sanitizer-instrumented. It runs an ordinary Debug build, never an ASan binary. Definite/indirect leaks and memory errors fail the run; possible leaks and reachable allocations are summarized but do not fail this gate. Use `--show-leak-kinds=all` for their complete stacks. Its selected workloads cover lifecycle/OOM, icons, navigation, table editing and native pickers.
- **Clang Static Analyzer** explores C paths without running the GUI. `scan-build --status-bugs` fails on a reported bug and saves HTML paths. It complements runtime checks; it is not a proof of correctness.
- **libFuzzer** mutates inputs while ASan/UBSan/LSan observe the execution. The icon harness exercises binary decoding and vector validation, the model harness exercises tree ancestry/IDs and copied choices, and the layout harness exercises bounded box/grid/wrap/split arrangements without a display.

ThreadSanitizer is a separate future build if worker-thread functionality is introduced; it cannot share the ASan configuration. MemorySanitizer can supplement uninitialized-read testing, but requires instrumented dependencies for useful results, which is difficult with stock GTK. Neither has been executed in this audit.

## Lifetime and allocation-failure regressions

[lifecycle.c](../../tests/safety/lifecycle.c) runs construction/teardown, model replacement, all 21 pattern families, repeated icon replacement, callbacks and timer stop/restart. A test-only allocation ledger and linker wrappers inject a failed C allocation at each position along representative construction paths. The current run covers 131 injected construction failures, seven table-model replacement failures and 300 same-shaped tree/picker replacements. Failed table replacement must preserve the old data. The default lifecycle test repeats full catalog construction twelve times.

The ledger must return to zero CUI allocations after teardown. It tracks direct CUI/test malloc-family calls, not every GTK/GLib allocation. LSan and native GObject weak-reference checks cover additional ownership paths. This distinction matters: a stable CUI ledger alone would not have found the GTK tree-reference leak.

The audit fixed two missing releases of owned references returned by `gtk_tree_list_row_get_item` in the GTK navigation backend. A regression observes the actual tree node being finalized when its model is replaced. The table backend now preserves native column objects across row and sorting updates and changes a factory only when its editability changes. This preserves column widths/focus and reduces native object churn; teardown also works around the GTK focused-column leak described below.

Static analysis also prompted explicit model/mapping bounds checks before picker row lookup and removal of an unused initial assignment in insight navigation. C test assertions are now kept enabled in Release builds, so assertions containing native interactions cannot silently disappear under `NDEBUG`.

## Toolkit allocations and honest failure reporting

The CI reproduction uses Ubuntu 24.04, GTK 4.14.5, GLib 2.80, Pango 1.52, Fontconfig 2.15 and Valgrind 3.22 with matching debug symbols. Earlier investigation on GTK 4.22.4 also found the focused-column leak. A [standalone GTK reproducer](../../tests/safety/gtk_baseline.c) links **no CUI code**. Use it to distinguish CUI ownership errors from dependency reports:

```sh
cmake -S . -B build-safety -DCMAKE_C_COMPILER=clang \
  -DCMAKE_BUILD_TYPE=Debug -DCUI_BUILD_TESTS=ON -DCUI_ENABLE_SANITIZERS=ON
cmake --build build-safety --parallel
dbus-run-session -- xvfb-run -a env GDK_DISABLE=gl,vulkan GSK_RENDERER=cairo GTK_A11Y=none \
  ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=print_suppressions=1 \
  ./build-safety/cui_gtk_baseline
# Additional baseline cases: files, folders, pickers, table or quarks.
```

Function-specific exceptions live in [lsan.supp](../../tests/safety/lsan.supp) and [valgrind.supp](../../tests/safety/valgrind.supp). They cover dependency font caches, mnemonic lists, GMarkup parsing, module discovery and retired GLib quark tables reproduced by the baseline. The file chooser also exposes two upstream ownership bugs: GTK 4.14's path bar omits `GFileInfo` releases, and its folder-selection `get_files` branch appends an owned current-folder reference without releasing it. The `folders` baseline reproduces a 32-byte GFile plus its path allocation even though the caller releases its own reference.

**Limits of these exceptions:** LSan matches allocation stacks, not the later reference that was lost. The `g_local_vfs_get_file_for_path` exception consequently masks other local GFile leaks, including possible future CUI ownership mistakes. `query_info_async_thread` can mask other leaked query results, and the GMarkup exceptions can mask other parser leaks. These are explicit coverage gaps, not fixes to GTK. Re-run with `--unsuppressed` after dependency upgrades and remove exceptions when the baseline is clean. Never suppress all `malloc`, GLib, GTK, GObject allocation, CUI functions, or table-column creation.

Two additional Valgrind conditional-value exceptions are restricted to librsvg 2.58's Rust GLib property builders reached through the SVG pixbuf loader. The standalone picker baseline reproduces them. Invalid reads/writes and UBSan errors remain fatal. Ubuntu's Valgrind 3.22 does not intercept glibc's AVX2 `wcpncpy` implementation correctly; Memcheck runs set `GLIBC_TUNABLES=glibc.cpu.hwcaps=-AVX2` to exercise the scalar/SSE path instead of suppressing address errors. Sanitizers continue using the normal CPU dispatch. Each GUI run gets a D-Bus session so GTK cannot repeatedly auto-launch a bus under Memcheck.

`GDK_DISABLE=gl,vulkan` and `GSK_RENDERER=cairo` keep this safety workflow on the software renderer. Without disabling GDK GPU initialization, an earlier icon test reported Mesa/EGL allocations even with Cairo rendering selected. This workflow does not certify GPU-driver cleanup.

**Focused table teardown:** GTK's [column-removal implementation](https://github.com/GNOME/gtk/blob/4.14.5/gtk/gtkcolumnview.c#L1592) obtains an owned neighboring column reference when moving focus and does not release it. On GTK 4.12+, CUI moves focus to the final column before disposal, then detaches the model. An empty table temporarily receives an inert row because the public focus API requires a valid row. Disposal then removes the focused column last, with no neighbor to retain. Native weak-reference assertions cover populated and empty tables, and column allocations remain unsuppressed.

The Ubuntu reproduction passes all five selected Memcheck workloads and the ASan/UBSan suite (43 tests passed, one window-manager-dependent test skipped). The new populated/empty table weak-reference checks also pass under ASan at normal and 2× scale. Clang static analysis reports no bugs. The older Arch host's missing loader symbols remain an environment limitation; those failed starts are not counted as verification.

## Evidence and limits

The completed 30-second fuzz runs processed 25,310,636 icon inputs, 2,755,762 model inputs and 12,105,547 layout inputs with no reported sanitizer findings. These are bounded smoke campaigns: model inputs contain at most 32 entries and layouts at most 16 immediate children. Existing deterministic GUI tests cover larger models. Preserve any crashing input and add a focused regression after fixing its cause.

Clang's analyzer initially reported a picker pointer-arithmetic concern and an insight dead store; after the changes, a complete scan reported no bugs. GCC 16 `-fanalyzer -Wanalyzer-too-complex` was also run. Ten non-complexity diagnostics involved correlated count/pointer invariants, revalidated widget state, and deliberate RGBA rounding. Inspection did not establish an additional defect; they remain advisory review findings, and complexity bailouts limit coverage. GCC's run is not labeled warning-free and is not the clean Clang gate.

Read the [validation snapshot](../validation.md) for exact final test counts. No tool run here establishes Windows/macOS resource cleanup, all FFI-language lifetime behavior, concurrency safety, or every allocation-failure path.

Primary tool references: [ASan](https://clang.llvm.org/docs/AddressSanitizer.html), [UBSan](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html), [LSan](https://clang.llvm.org/docs/LeakSanitizer.html), [libFuzzer](https://llvm.org/docs/LibFuzzer.html), [Clang Static Analyzer](https://clang.llvm.org/docs/analyzer/user-docs.html), and [Valgrind Memcheck](https://valgrind.org/docs/manual/mc-manual.html).
