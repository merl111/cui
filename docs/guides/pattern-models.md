# Pattern data models

Use `cui_patterns.h` for native tool, task, message and source collections, record predicates, and insight datasets. These models copy application data and reuse native controls. All calls belong on the app's main thread. Model setters are silent; user actions emit events on the pattern root. All four language galleries contain working integrations.

## Tool, task, chat and context collections

`cui_pattern_set_items` enables collection rendering on `CUI_TOOL_CHIPS`, `CUI_TASK_ROWS`, `CUI_CHAT` or `CUI_CONTEXT`. It replaces the entire collection. `cui_pattern_upsert_item` replaces a matching ID in place or appends a new one. `cui_pattern_remove_item` removes an existing ID and returns 0 if it was absent. Empty collections are valid and show an empty status.

```c
cui_widget *tasks = cui_pattern_create(root, CUI_TASK_ROWS, "Release checklist");
const cui_pattern_item items[] = {
    {42, "Review typography", "Check labels at 150% text scale.",
     "Design team", "In progress", .5, CUI_ROLE_SUBTLE, CUI_ITEM_EXPANDED},
    {77, "Publish", "Release after review.", "", "Queued",
     0, CUI_ROLE_SUBTLE, 0}
};
if (!cui_pattern_set_items(tasks, items, 2)) { /* report failure */ }
cui_on_action(tasks, task_action, application);
```

IDs must be unique, nonzero 64-bit values. A model holds up to 256 items. Titles require 1–255 UTF-8 bytes; bodies allow 65536, details 4096, and badges 63. Optional strings may be NULL. Progress must be finite and within 0–1. Badge tone is `CUI_ROLE_SUBTLE`, `SUCCESS`, `WARNING` or `DANGER`. Invalid input leaves the existing model unchanged.

`CUI_ITEM_EXPANDED` controls each tool/task disclosure independently. `CUI_ITEM_DISABLED` disables all controls belonging to that item. `CUI_ITEM_COMPLETE` controls the task checkbox; toggling it also sets progress to 1 or 0. Programmatic models supply the completion flag and progress separately. `CUI_ITEM_SELECTED` controls a context source checkbox. Model replacement uses the supplied flags; retain them from `cui_pattern_item_at` if preserving user state.

| User action | Root event | State |
| --- | --- | --- |
| Expand/collapse tool or task | `CUI_EVENT_CHANGE` | Expanded flag updated |
| Toggle task completion | `CUI_EVENT_CHANGE` | Complete flag and progress updated |
| Toggle context source | `CUI_EVENT_SELECT` | Selected flag updated |
| Copy chat message | `CUI_EVENT_SELECT` | Body copied to clipboard |
| Open tool result or source/message title | `CUI_EVENT_OPEN` | Application decides what to open |
| Remove button | `CUI_EVENT_CANCEL` | Request only; application removes the item |

Read `cui_pattern_item_event_id` inside the root callback for these item actions. It is not a selection getter and has no meaning for composer or query events. Mutation resets it to zero. Save it before replacing/removing the model:

```c
static void task_action(cui_widget *tasks, void *data)
{
    (void)data;
    cui_item_id id = cui_pattern_item_event_id(tasks);
    if (cui_pattern_event(tasks) == CUI_EVENT_CANCEL && id)
        cui_pattern_remove_item(tasks, id);
}
```

Callbacks may replace or remove the model synchronously. Preserve internal callbacks on item parts. The application owns sending a chat message: read the composer from `CUI_PART_INPUT` on `SUBMIT`, append it with a unique ID, and clear the input after successful insertion. Sending does not automatically contact a service.

## Parts and lifetime

The pattern's `CUI_PART_BODY` becomes a collection box. Use `cui_pattern_item_part(pattern, id, part)` to find current controls. `PREVIEW` is the item container, `TITLE` the title control, `BODY` the selectable text, `AUXILIARY` metadata, `STATUS` the badge, and `PRIMARY`/`SECONDARY` the item action/removal controls. Tool/task `DETAILS` is the disclosure; task `PROGRESS` is its progress bar. Absent IDs/parts return NULL.

Native slots are reused as collections shrink, reorder and grow. Resolve parts again by ID after every model mutation: a saved widget pointer remains app-owned but may now represent another item. Memory usage tracks the largest collection used, capped at 256 slots. These collections are not virtualized.

`cui_pattern_item_at` returns borrowed C strings valid until the next model mutation. Use them during that call sequence or copy them. Go and Python getters copy strings into language-owned values; Zig exposes the borrowed C structure. Do not reuse a borrowed structure after an intervening model change.

## Queries and record filters

`cui_pattern_set_records` provides three-column source rows to record, filter, diff and search compositions. `cui_pattern_set_query` updates the search field and immediately refreshes results. Calling the ordinary text setter alone does not trigger filtering.

`cui_pattern_set_filters` copies up to 32 predicates. Column 0 is name, 1 status, 2 detail; diff columns are property, before and after. Pass nonzero `all` for AND, zero for OR. No predicates matches all rows. Search and the Filter Table's “Active only” switch further constrain the results.

```c
const cui_record_filter filters[] = {
    {1, CUI_FILTER_EQUALS, "Active"},
    {2, CUI_FILTER_GREATER_EQUAL, "10"}
};
cui_pattern_set_filters(records, filters, 2, 1);
cui_pattern_set_query(records, "design");
```

Text operators are `CONTAINS`, `EQUALS`, `NOT_EQUALS`, `STARTS_WITH` and `ENDS_WITH`. They use the platform Unicode search normalization already used by autocomplete. Numeric `LESS`, `LESS_EQUAL`, `GREATER` and `GREATER_EQUAL` require a complete finite number; surrounding whitespace is accepted, malformed cell values do not match, and malformed predicate values reject the update. Numeric parsing follows the process C locale. `EMPTY`/`NOT_EMPTY` test the original cell's byte length; whitespace is not empty. Each predicate value and query is limited to 65536 bytes.

`cui_pattern_record_source(pattern, displayed_row)` maps a visible sorted/filtered row back to the most recently supplied source array, or returns `SIZE_MAX`. Refreshing results clears selection. Do not replace the internal table model or callbacks directly, because that bypasses the source-row map.

For context collections, queries search title/body/detail; predicates address title/badge/detail. Context sources use item IDs instead of row indices. `set_records` rejects updates after collection rendering is enabled; create another pattern if the legacy list presentation is needed. Sidebar uses its tree model and query API; record predicates do not apply to trees.

## Insight datasets

Insights start empty. Supply named datasets; there are no fixed sample values in the library:

```c
const double requests[] = {12, 18, 15, 24};
const char *labels[] = {"Mon", "Tue", "Wed", "Thu"};
const cui_insight_series series[] = {
    {100, "Requests", "Requests per day", requests, labels, 4},
    {200, "Pending dataset", "Waiting for measurements", NULL, NULL, 0}
};
cui_insights_set_series(insights, series, 2);
cui_insights_select(insights, 100, 2);
```

The model copies up to 256 series and 65536 points in total. IDs must be unique/nonzero, titles nonempty and at most 255 bytes, details at most 65536, and values finite. Optional labels must have one non-NULL entry per point, each at most 255 bytes. Empty series and empty models are valid. Point labels and numbers appear in the current-point status; details supply chart accessibility help and tooltip text.

Replacement retains the selected series ID where possible and clamps its selected point. Otherwise it selects the first series and first point. Previous/next buttons stop at the ends. The scrub slider selects the nearest data point and is disabled for zero/one-point series. User navigation emits `CHANGE`; setters and `cui_insights_select` are silent.

`cui_insights_selection` returns the series ID and optionally writes the point index/value. No model returns 0 and leaves outputs untouched. An empty series returns its ID with point 0 and value 0. `select` rejects an unknown ID or out-of-range point; empty series accept only point 0.

The chart remains a single vector line series, without axes, legends, zoom or multi-series overlays. This API completes supplied dataset navigation; it is not a general plotting package.

## Binding equivalents

| C | Go | Python | Zig |
| --- | --- | --- | --- |
| `cui_pattern_set_items` | `PatternItems` | `pattern_items` | `patternItems` |
| `cui_pattern_upsert_item` | `UpsertItem` | `upsert_item` | `upsertItem` |
| `cui_pattern_remove_item` | `RemoveItem` | `remove_item` | `removeItem` |
| `cui_pattern_item_at` | `ItemAt` | `item_at` | `itemAt` |
| `cui_pattern_item_part` | `ItemPart` | `item_part` | `itemPart` |
| `cui_pattern_item_count` | `ItemCount` | `item_count` property | `itemCount` |
| `cui_pattern_item_event_id` | `ItemEventID` | `item_event_id` property | `itemEventId` |
| `cui_pattern_set_filters` | `Filters` | `filters` | `filters` |
| `cui_pattern_set_query` | `Query` | `query` | `patternQuery` |
| `cui_pattern_record_source` | `RecordSource` | `record_source` | `recordSource` |
| `cui_insights_set_series` | `InsightSeries` | `insight_series` | `insightSeries` |
| `cui_insights_select` | `SelectInsight` | `select_insight` | `selectInsight` |
| `cui_insights_selection` | `InsightSelection` | `insight_selection` | `insightSelection` |

Go uses `PatternItem`, `RecordFilter` and `InsightSeries`; explicitly choose an allowed `Tone` (the zero value is not a badge tone). Python uses the same type names and defaults item tone to `SUBTLE`. Zig uses the corresponding imported C structures. See the [binding guide](../bindings.md) for build/run commands and the executable galleries for round trips, filters, insight navigation and reentrant item removal.
