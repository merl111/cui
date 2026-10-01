#ifndef CUI_SEARCH_H
#define CUI_SEARCH_H
#include "cui.h"
#include "cui_navigation.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct cui_choice {
    cui_item_id id; /* Unique, nonzero. */
    const char *label,*detail,*keywords;
    int disabled;
} cui_choice;
typedef enum cui_picker_kind { CUI_AUTOCOMPLETE, CUI_COMMAND_PALETTE } cui_picker_kind;
typedef enum cui_picker_event { CUI_PICKER_NONE, CUI_PICKER_QUERY, CUI_PICKER_SELECT, CUI_PICKER_SUBMIT, CUI_PICKER_CANCEL } cui_picker_event;
typedef enum cui_picker_part { CUI_PICKER_INPUT, CUI_PICKER_RESULTS, CUI_PICKER_STATUS, CUI_PICKER_ACCEPT, CUI_PICKER_CLOSE } cui_picker_part;
/* Searchable, virtualized native results with copied choice models (65536 max).
 * AUTOCOMPLETE starts with its input visible; COMMAND_PALETTE starts hidden.
 * Both expand in normal layout flow; a palette is not a modal/floating window.
 * Search matches label/detail/keywords, using platform Unicode case folding.
 * The root forwards events; preserve internal part callbacks.
 * Up/Down skip disabled choices, Enter accepts, Escape closes. Tab accepts the
 * highlighted autocomplete suggestion then continues ordinary focus traversal.
 * Plain unmatched autocomplete text emits SUBMIT with ID 0 on Enter. */
cui_widget *cui_picker(cui_widget *parent,cui_picker_kind kind,const char *placeholder);
int cui_picker_set_items(cui_widget *picker,const cui_choice *items,size_t count);
/* Independently show result headings, result status, and action buttons.
 * Defaults to all visible. Visibility choices survive filtering and reopening. */
int cui_picker_set_chrome(cui_widget *picker,int headings,int status,int actions);
int cui_picker_set_query(cui_widget *picker,const char *query); /* Silent; refreshes matches. */
size_t cui_picker_get_query(const cui_widget *picker,char *buffer,size_t capacity);
void cui_picker_open(cui_widget *picker,cui_widget *return_focus); /* Optional same-window target. */
void cui_picker_close(cui_widget *picker); /* Silent; restores return_focus. */
int cui_picker_is_open(const cui_widget *picker);
int cui_picker_select(cui_widget *picker,cui_item_id id); /* Silent; sets text and accepted ID. */
int cui_picker_accept(cui_widget *picker); /* Explicit user-equivalent action. */
cui_item_id cui_picker_selected(const cui_widget *picker);
size_t cui_picker_match_count(const cui_widget *picker);
cui_picker_event cui_picker_last_event(const cui_widget *picker);
cui_widget *cui_picker_get_part(cui_widget *picker,cui_picker_part part);
#ifdef __cplusplus
}
#endif
#endif
