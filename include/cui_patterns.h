#ifndef CUI_PATTERNS_H
#define CUI_PATTERNS_H
#include "cui.h"
#include "cui_navigation.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Native compositions inspired by the 21 Beautiful UI component families.
 * No AI, networking, microphone, or screen-capture service is bundled.
 * Set on_action on the root to receive integration requests; inspect event().
 * Part handles remain app-owned. Replacing a part's callback replaces its
 * internal behavior, so listen on the root instead. Strings are copied. */
typedef enum cui_pattern {
    CUI_LOADING, CUI_THINKING, CUI_STREAMING, CUI_APPROVAL, CUI_TOOL_CHIPS,
    CUI_TASK_ROWS, CUI_CHAT, CUI_PROMPT_BAR, CUI_RECOMMENDATION, CUI_CONTEXT,
    CUI_DIFF_TABLE, CUI_RECORDS_TABLE, CUI_FILTER_TABLE, CUI_SIDEBAR,
    CUI_SEARCH_PANEL, CUI_FLOWCHART, CUI_INSIGHTS, CUI_CODE_BLOCK,
    CUI_FINE_TUNE, CUI_SELECTION_ACTIONS, CUI_AGENT_SCREEN, CUI_PATTERN_COUNT
} cui_pattern;
typedef enum cui_part {
    CUI_PART_TITLE, CUI_PART_BODY, CUI_PART_INPUT, CUI_PART_PRIMARY,
    CUI_PART_SECONDARY, CUI_PART_CHOICE, CUI_PART_STATUS, CUI_PART_PROGRESS,
    CUI_PART_DETAILS, CUI_PART_CHART, CUI_PART_PREVIEW, CUI_PART_AUXILIARY,
    CUI_PART_COUNT
} cui_part;
typedef enum cui_event {
    CUI_EVENT_NONE, CUI_EVENT_SUBMIT, CUI_EVENT_CANCEL, CUI_EVENT_SELECT,
    CUI_EVENT_CHANGE, CUI_EVENT_OPEN, CUI_EVENT_DICTATE, CUI_EVENT_TRANSFORM
} cui_event;
const char *cui_pattern_name(cui_pattern kind);
cui_widget *cui_pattern_create(cui_widget *parent, cui_pattern kind, const char *title);
cui_widget *cui_pattern_part(cui_widget *pattern, cui_part part); /* NULL if absent. */
cui_event cui_pattern_event(const cui_widget *pattern);
/* Replaces the source rows used by search/filter/sort compositions. Three
 * columns: name, status, detail. For list compositions only name is displayed. */
int cui_pattern_set_records(cui_widget *pattern, const char *const *cells, size_t rows);
/* Sidebar source tree. Details is an optional parallel array of UTF-8 strings.
 * Search includes ancestors of matching nodes and opens those paths. Clearing
 * search restores source expansion state. Model changes clear selection. The
 * root emits SELECT / OPEN / CHANGE; query tree IDs through the BODY part.
 * Both arrays are copied. Use this API rather than replacing the body's model. */
int cui_sidebar_set_items(cui_widget *sidebar,const cui_tree_item *items,
    const char *const *details,size_t count);
int cui_stream_append(cui_widget *stream, const char *chunk); /* STREAMING only. */
void cui_pattern_set_busy(cui_widget *pattern, int busy);
/* Copied record predicates for CONTEXT, DIFF_TABLE, RECORDS_TABLE,
 * FILTER_TABLE and SEARCH_PANEL. Up to 32 predicates on columns 0..2.
 * all!=0 combines with AND, otherwise OR; zero predicates matches all.
 * Search text and the built-in Active toggle further constrain the result.
 * Text comparisons use platform Unicode case folding. Numeric comparisons
 * require complete finite C numeric values; invalid cell numbers never match.
 * Invalid replacements preserve prior filters/data. Setters are silent.
 * Query replacement also works on SIDEBAR; updates its search field/results. */
typedef enum cui_filter_op {
    CUI_FILTER_CONTAINS, CUI_FILTER_EQUALS, CUI_FILTER_NOT_EQUALS,
    CUI_FILTER_STARTS_WITH, CUI_FILTER_ENDS_WITH,
    CUI_FILTER_LESS, CUI_FILTER_LESS_EQUAL, CUI_FILTER_GREATER,
    CUI_FILTER_GREATER_EQUAL, CUI_FILTER_EMPTY, CUI_FILTER_NOT_EMPTY
} cui_filter_op;
typedef struct cui_record_filter { size_t column; cui_filter_op operation; const char *value; } cui_record_filter;
int cui_pattern_set_filters(cui_widget *pattern,const cui_record_filter *filters,size_t count,int all);
int cui_pattern_set_query(cui_widget *pattern,const char *query);
size_t cui_pattern_record_source(const cui_widget *pattern,size_t displayed_row); /* SIZE_MAX if invalid. */
/* INSIGHTS: application-supplied datasets; no built-in sample values.
 * Copies up to 256 uniquely identified series, at most 65536 total points.
 * IDs are nonzero; titles have 1..255 bytes, details up to 65536 bytes,
 * point labels up to 255 bytes, and all values are finite. Empty datasets and
 * an empty model are allowed. Optional per-point labels are copied.
 * Replacing retains the current series ID and clamps its point, or selects the
 * first series. Navigation/scrubbing emits CHANGE; model/select calls are silent.
 * select rejects an invalid point (only point 0 is valid on an empty series).
 * Selection getter returns 0 with no series and leaves outputs unchanged. */
typedef struct cui_insight_series {
    cui_item_id id; const char *title,*detail;
    const double *values; const char *const *labels; size_t count;
} cui_insight_series;
int cui_insights_set_series(cui_widget *insights,const cui_insight_series *series,size_t count);
int cui_insights_select(cui_widget *insights,cui_item_id id,size_t point);
cui_item_id cui_insights_selection(const cui_widget *insights,size_t *point,double *value);
/* Dynamic TOOL_CHIPS, TASK_ROWS, CHAT and CONTEXT collections. The first call
 * opts into collection rendering in place of the legacy single-item body.
 * At most 256 items; IDs unique/nonzero; strings copied. title is required
 * (1..255 bytes), body <=65536, detail <=4096, badge <=63; NULL optional strings
 * mean empty. Finite progress is 0..1. Tone uses SUBTLE/SUCCESS/WARNING/DANGER.
 * Each tool/task has independent disclosure state. Chat shows author/badge,
 * metadata and selectable message text; context cards support source selection.
 * Replacing/upserting/removing is silent; getters return borrowed strings until
 * the next model mutation. Slots are reused up to the largest model displayed.
 * Internal actions emit CHANGE (expansion/task), SELECT (source/copy), OPEN
 * (tool/source), or CANCEL (removal requested); CANCEL does not remove an item.
 * Read item_event_id in the root callback. Preserve internal part callbacks.
 * BODY becomes a box; resolve item parts by ID after each model mutation.
 * Context query searches title/body/detail; predicates use title/badge/detail.
 * Context set_records returns 0 after opting into collection rendering.
 * Text is plain UTF-8; rich content is a separate rendering capability. */
typedef enum cui_pattern_item_flags {
    CUI_ITEM_EXPANDED=1, CUI_ITEM_DISABLED=2, CUI_ITEM_COMPLETE=4,
    CUI_ITEM_SELECTED=8
} cui_pattern_item_flags;
typedef struct cui_pattern_item {
    cui_item_id id; const char *title,*body,*detail,*badge;
    double progress; cui_role tone; unsigned flags;
} cui_pattern_item;
int cui_pattern_set_items(cui_widget *pattern,const cui_pattern_item *items,size_t count);
int cui_pattern_upsert_item(cui_widget *pattern,const cui_pattern_item *item);
int cui_pattern_remove_item(cui_widget *pattern,cui_item_id id);
size_t cui_pattern_item_count(const cui_widget *pattern);
int cui_pattern_item_at(const cui_widget *pattern,size_t index,cui_pattern_item *item);
cui_item_id cui_pattern_item_event_id(const cui_widget *pattern);
cui_widget *cui_pattern_item_part(cui_widget *pattern,cui_item_id id,cui_part part);
#ifdef __cplusplus
}
#endif
#endif
