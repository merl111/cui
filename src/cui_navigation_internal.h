#ifndef CUI_NAVIGATION_INTERNAL_H
#define CUI_NAVIGATION_INTERNAL_H
#include "cui_internal.h"
#include "cui_navigation.h"
typedef struct cui_tree_node {
    cui_item_id id;
    char *text;
    int parent, first, next, depth, expanded;
    void *native;
} cui_tree_node;
typedef struct cui_tree_index { cui_item_id id; size_t index; } cui_tree_index;
typedef struct cui_tree_model {
    cui_tree_node *nodes;
    cui_tree_index *lookup;
    size_t count;
    int first;
    cui_item_id selected, event_id;
    cui_tree_event event;
} cui_tree_model;
cui_tree_model *cui__tree_model_copy(const cui_tree_item *items,size_t count);
void cui__tree_model_free(void *model);
cui_tree_node *cui__tree_model_find(const cui_tree_model *model,cui_item_id id);
cui_tree_node *cui__tree_find(const cui_widget *w, cui_item_id id);
void cui__tree_event(cui_widget *w, cui_tree_event event, cui_item_id id);
void cui__backend_tree_items(cui_widget *w);
void cui__backend_tree_select(cui_widget *w, cui_tree_node *node);
void cui__backend_tree_expand(cui_widget *w, cui_tree_node *node);
#endif
