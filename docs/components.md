# Component coverage

The catalog follows the 21 families listed at [Beautiful UI](https://www.beautifului.dev/), inspected September 30, 2026. These are native CUI compositions, not the site's React components or a pixel-identical port. Every family has a constructor, an example in the C gallery's **Components** tab, and a native integration test. The table describes the current implementation and its limits; presence in the catalog does not mean full interaction parity with the reference.

```c
#include "cui_patterns.h"
cui_widget *approval = cui_pattern_create(root, CUI_APPROVAL, "Review changes");
cui_set_text(cui_pattern_part(approval, CUI_PART_BODY), "Choose the next step.");
cui_on_action(approval, on_approval, application);
```

| Family | Available now | Further work / application integration |
| --- | --- | --- |
| Loading State | Spinner, elapsed timer, progress, cancel action | Application reports progress and cancels its work |
| Thinking | Expandable trace and selectable text | Application supplies reasoning/status content |
| Streaming Text | Incremental UTF-8 append, copy, stop | Application supplies chunks; plain text, no Markdown renderer |
| Approval Card | Choices, custom answer, continue/skip events | Application performs the approved action |
| Tool Chips | Stable-ID tool collection, independent disclosure, copied result text/metadata, open/removal events | Plain selectable text; rich inline content and icons remain |
| Task Rows | Stable-ID task collection, independent expansion, per-task progress/completion and removal | Application supplies task state; drag reordering remains |
| Chat | Copied message collection, author/badge/metadata, selectable bodies, copy/removal, composer/send | Gallery appends local messages; rich text, avatars and virtualized history remain |
| Prompt Bar | Composer, model/source selectors, send/dictate events | Speech capture, command completion and attachment ingestion are application integrations |
| Recommendation Card | Alternatives, confidence progress, custom answer, accept/skip | Application supplies scores and recommendations |
| Context Cards | Individual source cards with stable IDs, searchable body/metadata, selection, open/removal events | Application loads source contents; rich previews and icons remain |
| Diff Table | Before/after columns, selected change, include checkbox, apply/reject | Application applies changes; multiple selection is available, per-row checkboxes remain |
| Records Table | Virtualized native rows, resizing, multiple selection, column sorting; optional cell editing via `cui_tables.h` | Copied in-memory data; lazy remote data, column reordering and richer cell types remain |
| Filter Table | Live Unicode search, copied text/numeric predicates with AND/OR, active-status filter, source-row mapping | Application supplies predicates; a visual nested-expression editor remains |
| Sidebar Nav | Native nested tree, stable IDs, searchable names/details, ancestor-preserving results, selection/open events, restored expansion state | Icon items remain; search uses platform Unicode normalization |
| Search | Live matching, selection, empty state | Unicode-normalized matching; ranking and async providers remain |
| Flowchart | Trigger selector, editable condition rows, add condition, action field | Structured vertical editor; freeform node dragging/connectors remain |
| Insight Cards | Copied named datasets/point labels, ID-preserving replacement, previous/next, exact point scrubbing, empty states | Single-series line chart; axes, zoom and multi-series plots are outside this component |
| Code Block | Selectable monospace text, copy | Unified diffs can be supplied as text; syntax highlighting and split diff view remain |
| Fine-tune Card | Width/height sliders, text field, live preview dimensions/title | Additional properties such as radius, opacity and color remain |
| Selection Actions | Native text selection, selected UTF-8 extraction, action requests | Application provides the transformation; no AI service bundled |
| Agent Screen | Copied RGBA frame, loading state, open event | Gallery opens a sample frame; capture, remote streaming and recording are application integrations |

`cui_pattern_part` returns an app-owned widget or NULL when that part is absent. Text, options, values and data can be changed using ordinary CUI setters. Listen on the pattern root to preserve its internal callbacks; setting a callback on a part replaces that part's internal behavior. `cui_pattern_event` identifies the last emitted action. Programmatic setters do not emit events.

`cui_pattern_set_records` copies three-column rows (name, status, detail; property, before, after for diffs). Search/filter/sort refreshes the displayed data and clears its selection. Selection indices refer to the displayed rows. This is a small in-memory model, not a scalable database grid.

See [Pattern data models](guides/pattern-models.md) for complete collection, filter and insight contracts and binding equivalents.

## Native building blocks

Rows/columns, labels, buttons, toggles, switches, grouped radio buttons, checkboxes, entries, passwords, search fields, multiline text, code surfaces, dropdowns, lists, tables, sliders, progress bars, spinners, separators, badges, line charts, RGBA images, tabs, and disclosures. Windows can scroll their document. Native trees, breadcrumb paths, numeric steppers, date/time inputs, color/font pickers, labeled validation fields, grids, wrapping containers, split panes, dialogs, menu commands, toolbars, shortcuts, focus and accessibility labels are available through the additional headers described in [Desktop controls](desktop.md). Font controls, placeholders, tooltips, minimum dimensions, visibility, enabled state, clipboard and main-thread timers are shared APIs.

Tabs/disclosures are composed controls using native buttons and containers. They still need a dedicated screen-reader audit; native tab semantics are not claimed. Charts and image previews are custom native drawing surfaces and currently have no rich accessibility model.

## Recommended next additions

1. Further table capabilities: lazy data providers, column reordering and richer cell types.
2. Floating popovers/overlays and vector icons. In-layout banners/toasts, empty/error states, searchable command palettes, autocomplete and choice-backed tokens are now implemented; see `desktop.md`.
3. Rich content, drag-and-drop, and dynamic widget lifetime.
4. Finish the remaining family-specific behaviors listed above.

Native table editing, multiselection and stable text/numeric sorting are now implemented in `cui_tables.h`, including source-row indices that survive sorting. Header interaction requires GTK 4.10+ on Linux; programmatic sorting works with GTK 4.6. These remaining additions require corresponding keyboard, focus, accessibility and language-binding support. Add them as optional compositions or separate static-library objects where practical so small applications do not pay for every component.
