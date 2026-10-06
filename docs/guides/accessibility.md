# Keyboard, accessibility and localization

Native controls provide a useful baseline for keyboard input, selection, focus, IME and assistive technologies. Applications still need meaningful labels, coherent focus order, usable large-text layouts and testing of their composed experiences.

## Name actions and fields

Use visible labels that describe a field or action. `cui_accessibility` supplies a label and longer description when the visible content is insufficient. `cui_set_tooltip` provides supplemental help, but a tooltip is not a replacement for a persistent label or validation message.

Append controls in the order a keyboard user should reach them. Group radio buttons under one immediate parent and put separate groups in separate boxes. Preserve visible focus indication. Do not force focus to a status message when ordinary inline feedback would be less disruptive.

## Preserve native text behavior

Text is UTF-8 at the C boundary. Native backends handle shaping, font fallback and input methods. A navigation-key callback must leave unhandled keys to the native control; composing text is not the same as pressing Enter to submit a form.

Avoid fixed-size buffers when reading user text. The getters return byte counts, and truncating a short buffer can split a Unicode character. Labels, choices and context data should use stable numeric IDs for logic so translation does not change application behavior.

## Large text and contrast

Test at enlarged application text scale as well as display DPI scale. Let wrapping rows and scrolling keep every action reachable. Pair success/error colors with words and keep recovery actions descriptive. Respect the system theme where appropriate; high-contrast handling exists in the backends, but its presence does not certify every composition.

## Current limitations

Tabs and disclosures are composed from native controls and do not yet claim fully audited native tab semantics. Charts and RGBA images lack a rich accessibility model. Full RTL layout on Windows/macOS, complete reduced-motion handling and end-to-end screen-reader/IME audits remain in the backlog.

GTK native tests and screenshots establish current Linux behavior. The native CI matrix includes Windows and macOS, but those jobs and screen readers must run on their respective hosts before claiming verification. Document implementation separately from runtime verification.

## Application focus-outline policy

`cui_app_set_focus_indicators(app, 0)` suppresses CUI's visible native and chat
focus outlines while preserving focus, keyboard traversal and activation.
The default is enabled. This is an application-wide visual policy, not a way to
remove widgets from keyboard navigation. Bindings expose `App.focus_indicators`
(Rust/Python), `App.FocusIndicators` (Go), and `App.focusIndicators` (Zig).

## Chat text and dialog focus

Canvas regions have an explicit `cui_canvas_role`: zero/default
`CUI_CANVAS_BUTTON` for actions, or `CUI_CANVAS_TEXT` for readable content.
Chat message bodies expose author, timestamp and complete concatenated spans;
links, attachments and reactions retain independent actionable regions. GTK
uses selectable native labels, AppKit static-text accessibility elements, and
WinUI read-only TextBox peers. The legacy Win32 adapter uses read-only edits.
Windows retains unchanged peers across refreshes to preserve native focus.
The new region field changes the ABI: rebuild CUI and every consumer together.

Capture `cui_focused_descendant(background)` **before** disabling the background
for an application modal. Re-enable it before calling `cui_focus(bookmark)` on
close; choose a logical fallback if the saved control is hidden or disabled.
The borrowed handle remains valid until app destruction. This complements
native dialog behavior; it does not itself implement modality or a focus trap.

The `chat_ux` test runs shared scene, scaling, translation, contrast and focus
contracts on all native CI targets. It also checks GTK's native text role and
AppKit's static-text value. Linux has been exercised locally; VoiceOver,
Narrator/UIA and Orca end-to-end validation remain separate requirements.
