# CUI for Rust

Full raw ABI and convenience bindings for the native CUI GUI library. No Cargo dependencies. Requires the prebuilt CUI static library and installed operating-system libraries.

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cargo run --offline --manifest-path bindings/rust/Cargo.toml --example hello
cargo run --offline --manifest-path bindings/rust/Cargo.toml --example gallery
```

Use `cui = { path = "/absolute/path/to/cui/bindings/rust" }` in your application's Cargo.toml. Set `CUI_LIB_DIR` to the archive directory when it differs from this checkout's `build` directory. Rust 1.82+; Linux needs GTK 4 development files with X11 support and Xext (`pkg-config gtk4-x11 xext`) and pkg-config. No crate has been published to crates.io.

Create one App on the main thread. Handles are !Send/!Sync and use checked weak ownership; stale calls return Error::Closed. Icons use retain/release through Clone/Drop. Setters copy input; getters own output. Closures live until app destruction. Callback panics stop the loop and are returned by App::run with the normal unwind strategy. Keep raw `sys` usage within the C ABI's unsafe contracts.

See the [complete Rust guide](../../docs/guides/rust.md) for models, dialogs, icons, callbacks, cross-platform builds, errors and tests. Linux is tested; Windows/macOS native verification is deferred.
