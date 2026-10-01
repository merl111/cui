# Chat design implementation

The implementation belongs in C. The public contract is `include/cui_chat.h`; model ownership and events are in `src/cui_chat.c`, layout/rendering in `src/cui_chat_paint.c`, and native composition/workspace behavior in `src/cui_chat_compose.c`. Rust, Python, Go and Zig expose this contract.

The target is close visual and interaction parity with Archaic's Nebula, Daylight and Tiles HTML designs. The C example uses the shared design fixture. User screenshots provide additional interaction states: Nebula starts with the thread closed; opening a thread and switching rooms can show an empty panel; Daylight starts with an About inspector, incoming grouped bubbles and outgoing red bubbles.

Implemented shared components: room navigation, timeline, native composer, persistent workspace, room header, space navigation and inspector, plus standalone messages, attachment cards, reaction strips, polls, reply previews, thread summaries and avatars. All fourteen kinds have individual component catalog entries and native recipes. General CUI additions support widget styling, vector chat symbols, allocated widget sizes, native text measurement, textarea height/placeholder, and key handling.

Validation includes C model/event contracts, native input tests, Rust adapters and ABI checks, Python/Go/Zig nested-model and event checks, and native screenshots with source hashes. The documentation guide distinguishes tested component capabilities from unfinished design/application interactions. Windows/macOS execution and a measured 99% visual comparison remain outstanding.

The other Archaic functionality task owns Matrix behavior. This work does not change Archaic source.
