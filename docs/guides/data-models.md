# Data models and integration recipes

CUI copies small in-memory models so application data can change independently from the native view. Virtualized controls create visible rows on demand, but the backing data is still copied into memory. Virtualization does not mean remote or lazy data loading.

## Editable tables

Start with column headers and row-major cell strings. Enable editing for individual columns and multiple selection explicitly. User edits report the displayed row and column. After sorting, map that displayed row through `cui_table_source_row` before updating your application model. Source indices refer to the last `cui_table_set_rows` call.

Text sorting compares UTF-8 bytes; it is not locale collation. Numeric sorting compares finite parsed numbers and puts non-numbers last. Sorting and data replacement clear selection. Persist any desired selection in your model and restore it deliberately after refresh.

Use two-call getters for cell strings and selected-row arrays. Read counts first, allocate storage, then fetch. Rich cell types, lazy providers and column reordering are pending work.

## Trees and breadcrumbs

Tree IDs are stable, unique and nonzero. Parent 0 denotes a root; every other parent must appear before its child in the input array. The model accepts up to 65,536 nodes and 128 levels. Invalid replacements preserve the old model; successful replacement clears selection.

Selection can open ancestors. Observe expansion events if your app persists navigation state. Breadcrumbs represent a root-to-current path of up to 128 segments. Activation reports an ancestor ID; the application performs navigation and replaces the path. The final location is a label, not a redundant button.

## Searchable choices

Autocomplete and command palettes share copied choices: ID, label, detail, keywords and disabled state. Matching uses platform Unicode case folding. Up/Down skip disabled choices; Enter accepts; Escape closes. Autocomplete can submit unmatched text with ID 0 so the app can validate a new value.

These controls expand inside ordinary layout. For a command palette, register a shared command that opens it and remembers the return-focus target. Dispatch its accepted stable ID to application actions rather than inferring actions from translated labels.

## Ordered token selections

Tokens reuse the choice model. Selected IDs are unique, ordered and limited to 1…128 values. Model replacement preserves selected IDs that still exist and remain enabled. Transactional selection replacement rejects duplicates, unknown IDs and values beyond the limit.

For user-created tags, listen for unmatched SUBMIT, validate the query, update your application choice model with a new nonzero ID, then replace choices and select that ID. The widget does not invent identifiers or silently accept invalid free text.

## Validate a form

Numeric, date and time controls enforce their representable domains. A validation field adds a label, persistent help and an inline error for application rules. Listen on its root and set an error when the value is invalid. An empty error clears the state. Validation presentation alone does not prevent your save command from running; disable or reject submission in the application model.

## Integrate the 21 pattern families

Pattern compositions expose part handles and typed integration events. The application supplies streaming chunks, records, scores, frames or transformations. Services such as inference, speech capture, networking and recording are intentionally outside the GUI library.

Some pattern interactions remain prototypes: for example, streaming text is plain UTF-8, flowchart is a structured editor, and code surfaces have no syntax highlighting. Every pattern page lists these limits. Treat its example as a working starting point, not a claim of completed parity with the reference design.

See [Pattern data models](pattern-models.md) for stable-ID tool/task/chat/source collections, copied insight datasets and configurable record predicates. These APIs replace fixed sample state with application-owned data and typed user events.
