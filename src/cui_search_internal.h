#ifndef CUI_SEARCH_INTERNAL_H
#define CUI_SEARCH_INTERNAL_H
#include "cui_internal.h"
#include "cui_search.h"
typedef struct cui_choice_entry { cui_item_id id; char *label,*detail,*keywords,*key; int disabled; } cui_choice_entry;
typedef struct cui_choice_index { cui_item_id id; size_t index; } cui_choice_index;
typedef struct cui_choice_model { cui_choice_entry *items; cui_choice_index *lookup; size_t count; } cui_choice_model;
cui_choice_model *cui__choices_copy(const cui_choice *items,size_t count);
void cui__choices_free(cui_choice_model *model);
cui_choice_entry *cui__choices_find(const cui_choice_model *model,cui_item_id id);
#endif
