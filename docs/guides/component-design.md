# Reusable components and framework comparison

CUI is a C library. Rust, Python, Go and Zig adapt its models and events; they do not own separate renderers. Archaic supplies Matrix data, preferences and application actions. The three chat designs are starting presets, not three application-specific implementations.

## The middle ground

Use native controls for editing, focus, keyboard input and platform dialogs. Compose those controls with shared C rendering for the distinctive message and navigation surfaces. Keep four decisions separate:

1. **Data and behavior:** stable item IDs, copied models, events and application-owned state.
2. **Presentation:** message layout, room density, navigation orientation, visible controls and inspector content layout.
3. **Style:** palette, typography, spacing, borders and radii.
4. **Application composition:** which actions exist, which panels open, routing, persistence and backend operations.

A color change should not silently switch message layout. A translated label should not change command routing. A new application should not need to copy a renderer to remove a call button. Native editing behavior should survive appearance changes.

The current chat split has useful boundaries: room navigation, timeline, composer and workspace, plus standalone attachment, reaction, poll, reply, thread and avatar components sharing their timeline renderer. Headers and inspectors are optional convenience compositions. Applications can instead compose ordinary CUI boxes, tabs, buttons and their own content. These are not yet arbitrary replaceable child templates within the painted header/inspector.

## What other frameworks do

| Framework | Relevant approach | CUI decision |
| --- | --- | --- |
| SwiftUI | View composition and view styles; standard button behavior can be kept while supplying an appearance. | Keep interaction semantics shared; expose presentation separately from data. |
| AppKit | Native controls plus custom NSView-based controls, with accessibility responsibilities for custom content. | Retain native editors and platform services; painted content needs an explicit accessibility contract. |
| WinUI | Reusable styles/resources, visual states, control and item templates; custom automation peers. | Expose palette, state and commands independently. Do not treat a screenshot as proof of accessibility. |
| Qt Quick Controls | Background/content/delegate customization and reusable composite controls. | Provide small elements and optional compositions; presets should remain replaceable. |
| GTK | Widget composition, class templates and platform accessibility integration. | Keep one C ownership/event contract and reuse native behavior where possible. |

Sources: [SwiftUI view styles](https://developer.apple.com/documentation/swiftui/view-styles), [AppKit custom controls](https://developer.apple.com/documentation/appkit/custom-controls), [WinUI styles](https://learn.microsoft.com/en-us/windows/apps/develop/platform/xaml/xaml-styles), [WinUI item templates](https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/item-containers-templates), [Windows accessibility](https://learn.microsoft.com/en-us/windows/apps/design/accessibility/accessibility-overview), [Qt customization](https://doc.qt.io/qt-6/qtquickcontrols-customize.html), [GTK widgets](https://docs.gtk.org/gtk4/class.Widget.html).

This is an architectural comparison, not a claim that these frameworks have identical rendering, performance or API models. It does not imply that CUI should implement XAML, QML or a declarative language.

## Configurable chat components

`cui_chat_presentation_preset` returns an editable value. `cui_chat_set_presentation` copies it; `cui_chat_presentation_get` reads the current value. `cui_chat_set_theme` changes colors and font size while preserving presentation and commands. To switch a whole design, explicitly apply both its palette and presentation to each component and arrange the application containers.

```c
cui_chat_theme palette;
cui_chat_theme_get(CUI_CHAT_NEBULA, &palette);
cui_chat_set_theme(timeline, &palette);
cui_chat_presentation presentation;
cui_chat_presentation_preset(CUI_CHAT_DAYLIGHT, &presentation);
presentation.messages = CUI_CHAT_SOFT; /* bubbles with a dark palette */
presentation.rooms = CUI_CHAT_COMPACT;
presentation.room_height = 34;
presentation.bubble_radius = 12;
cui_chat_set_presentation(timeline, &presentation);
cui_chat_set_presentation(room_list, &presentation);

const cui_chat_command commands[] = {
    {.id = 401, .label = "Search conversation", .symbol = CUI_SYMBOL_SEARCH,
     .action = CUI_CHAT_MORE},
    {.id = 402, .label = "Details", .symbol = CUI_SYMBOL_PANEL,
     .action = CUI_CHAT_MORE, .flags = CUI_CHAT_MINE}
};
cui_chat_set_commands(header, commands, 2);
```

Commands configure header buttons, timeline/message hover actions and inspector tabs. They have copied labels/glyphs, unique stable IDs, symbols, action kinds and selected/disabled states. Pass zero commands to remove a toolbar. Header/message events carry the item ID in `id` and command ID in `detail_id`; inspector tab events carry the command ID in `id`. Rendering positions and translated labels are not command identities.

Inspector tab identity and content presentation are separate. The application selects profile, people-list or media-grid presentation and supplies content when the selected tab changes. The sample's About/People/Media names are replaceable. Composer parts remain native widgets; applications can translate their labels and select which attachment/emoji/poll controls appear through `composer_tools`.

The Daylight example's profile button opens appearance preferences for independent message and room-list presentation. Those preferences are local to the running example; persistence belongs to the application. Layout changes preserve current message data and editor state.

| Binding | Configuration API |
| --- | --- |
| Rust | `chat_presentation`, `ChatPresentation`, `ChatCommand`, `ChatComponent::presentation/set_presentation/set_commands` |
| Python | `chat.presentation`, `Presentation`, `Command`, `Chat.presentation/set_presentation/set_commands` |
| Go | `ChatPresentationPreset`, `ChatPresentation`, `ChatCommand`, `ChatComponent.Presentation/SetPresentation/SetCommands` |
| Zig | `Chat.presentationPreset/presentation/setPresentation/setCommands` with imported C value types |

## Remaining toolkit gaps

This inventory is based on CUI's public headers and implementation, not just the component count. Existing native selects, menus, tooltips, file/color/font dialogs, trees, tables, keyboard handling, images, splitters and command palettes are already present.

| Area | CUI today | Gap and priority |
| --- | --- | --- |
| Overlay lifecycle | `cui_stack` and `cui_stack_layer` now compose same-window layers; the C chat example demonstrates outside-click dismissal, input blocking and focus restoration. Anchored windows and platform dialogs remain available. | **Remaining work:** a toolkit-level modal lifecycle/focus-trap contract, backdrop effects and platform validation. [Qt Popup](https://doc.qt.io/qt-6/qml-qtquick-controls-popup.html) and [GTK Overlay](https://docs.gtk.org/gtk4/class.Overlay.html) illustrate separate lifecycle and composition concerns. |
| Scrollable content | Scrollable windows and specialized list/tree/table/timeline widgets. | **Design priority:** an arbitrary-child scroll viewport and reusable virtual item/delegate contract. A native tree's existence does not supply general content virtualization. [SwiftUI ScrollView](https://developer.apple.com/documentation/swiftui/scrollview). |
| Painted-content accessibility | Native control behavior, widget labels/descriptions and canvas action regions. | **Required toolkit work:** semantic message/list/poll roles, selection/value state, live announcements and platform bridge validation. Keyboard regions alone are insufficient. [Windows accessibility](https://learn.microsoft.com/en-us/windows/apps/design/accessibility/accessibility-overview). |
| Style inheritance and system settings | Widget styles, semantic roles, fonts, theme selection and chat presentation values. | Shared inherited tokens, per-state styling, complete high-contrast/reduced-motion behavior, font selection for painted chat, and system-preference propagation need more work. |
| Layout and templates | Boxes, grid, wrap, layered stacks, split panes and specialized composites. | Explicit min/preferred/max sizing, alignment/constraints and replaceable child content slots need a consistent public contract. |
| Data transfer | Text clipboard and native file dialogs. | Typed clipboard formats, application drag sources/drop targets and a cross-platform MIME/data-offer API. [GTK drag and drop](https://docs.gtk.org/gtk4/drag-and-drop.html). |
| Rich content | Styled chat spans, native text editing and image/icon decoding. | General selectable rich documents, media playback, async image loading/cache policy, and comprehensive bidi/RTL validation are broader work. |
| Reactive updates and motion | Explicit setters, callbacks and timers. | No general property binding, transition or animation system. These are optional future layers, not a prerequisite for the reference designs. |
| Cross-platform evidence | Shared C APIs with GTK, AppKit and Win32 implementations. | Linux tests/captures do not establish macOS/Windows parity. Native accessibility, input, layout and rendering validation on those hosts remains outstanding. |

The current extension removes the fixed command lists and separates important presentation choices. It is not a full styling/template engine: some sub-element dimensions, default English strings and specialized profile-card structure still need broader customization. Keep adding options only where a concrete use case establishes their meaning; use general primitives when a composition needs a different structure.

## Design-parity acceptance

The reference HTML and supplied screenshots define the preset targets. Preserve initial and interactive states: closed/open thread, room switching, inspector tabs, message grouping, sending/replying, reactions, polls, pane focus/close/maximize and dialogs. Compare identical viewport sizes and text scale, and inspect both geometry and behavior. Native font rasterization can differ from browser rasterization, so visual comparison and interaction coverage must be reported separately.

The native examples are not yet a measured 99% match. This architecture work makes further corrections reusable; it does not substitute for completing the remaining visual details and state-by-state comparisons.
