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

GTK native tests and screenshots establish current Linux behavior. Windows and macOS testing is a later development phase by project choice. Document a platform's implementation separately from its verification status; do not hide an untested behavior behind a generic supported badge.
