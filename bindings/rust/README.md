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

The [chat component guide](../../docs/guides/chat.md) covers `cui::chat`: themeable
room navigation, timelines, reactions/polls/threads, a native composer, persistent
conversation panes, a command palette and reusable panels. Run
`cargo run --offline --manifest-path bindings/rust/Cargo.toml --example chat_components -- tiles`
from the repository root to try the native composition example.

All fourteen chat component kinds are exposed by `ChatComponent::create` with
`ChatKind`, including standalone avatars, messages, files, reactions, polls,
reply previews and thread summaries. They use libcui's C renderer; Rust only
marshals models and events. `Message.author_color` and
`ThreadSummary.participants` preserve Daylight sender colors and thread avatars.
Rebuild libcui and this crate together when the C model ABI changes.


The full [Daylight composition](examples/daylight.rs) is ready for application
integration. Run `CUI_LIB_DIR="$PWD/build-chat" cargo run --offline
--manifest-path bindings/rust/Cargo.toml --example daylight` from the repository
root after building CUI. Its application state is Rust; all component rendering
and controls use the shared C implementation. The C and Rust examples share
fixture data and an eight-state screenshot comparison. This comparison checks
the language port, not HTML-reference fidelity.
