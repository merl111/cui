#ifndef CUI_PATTERNS_INTERNAL_H
#define CUI_PATTERNS_INTERNAL_H
#include "cui_internal.h"
#include "cui_patterns.h"
#include "cui_navigation_internal.h"
typedef struct pattern_state {
    cui_pattern kind;
    cui_event event;
    cui_widget *root, *parts[CUI_PART_COUNT], *spinner;
    cui_timer *timer;
    char **records;
    size_t rows, *visible_rows;
    cui_tree_model *sidebar;
    char **sidebar_details;
    int sidebar_filtering;
    int sort_column;
    int reverse, filter, page, busy;
    double started;
    struct cui_pattern_filters *filters;
    struct cui_insight_model *insights;
    struct cui_pattern_collection *collection;
    size_t point;
} pattern_state;
pattern_state *cui__pattern_state(const cui_widget *widget);
void cui__pattern_emit(pattern_state *state,cui_event event);
int cui__pattern_refresh_records(pattern_state *state);
int cui__pattern_matches(pattern_state *state,const char *const *row);
void cui__pattern_filters_free(struct cui_pattern_filters *filters);
void cui__insights_free(struct cui_insight_model *model);
void cui__insight_action(cui_widget *sender,void *data);
void cui__insight_empty(pattern_state *state);
void cui__collection_free(struct cui_pattern_collection *collection);
int cui__collection_refresh(pattern_state *state);
int cui__collection_active(pattern_state *s);
#endif
