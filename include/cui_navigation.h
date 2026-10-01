#ifndef CUI_NAVIGATION_H
#define CUI_NAVIGATION_H
#include "cui.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t cui_item_id;
typedef struct cui_tree_item {
    cui_item_id id, parent; /* Unique nonzero ID; parent 0 means a root. */
    const char *text;
    int expanded;
} cui_tree_item;
typedef enum cui_tree_event {
    CUI_TREE_NONE, CUI_TREE_SELECTION, CUI_TREE_EXPAND,
    CUI_TREE_COLLAPSE, CUI_TREE_ACTIVATE
} cui_tree_event;
/* Native, scrollable tree with keyboard navigation and single selection.
 * Strings are copied. Parents must precede children; sibling order follows
 * input order. Maximum 65536 items / 128 levels. Invalid replacements leave
 * the previous model untouched. Replacing data clears selection. */
cui_widget *cui_tree(cui_widget *parent, const cui_tree_item *items, size_t count);
int cui_tree_set_items(cui_widget *tree, const cui_tree_item *items, size_t count);
int cui_tree_select(cui_widget *tree, cui_item_id id); /* 0 clears; opens ancestors. */
cui_item_id cui_tree_selected(const cui_widget *tree);
int cui_tree_expand(cui_widget *tree, cui_item_id id, int expanded);
int cui_tree_is_expanded(const cui_widget *tree, cui_item_id id);
/* User events only; programmatic updates never emit. The ID identifies the
 * selected, expanded, collapsed or activated item (0 for deselection). */
cui_tree_event cui_tree_last_event(const cui_widget *tree, cui_item_id *id);
typedef struct cui_breadcrumb_item { cui_item_id id; const char *text; } cui_breadcrumb_item;
/* An ordered path, root first and current location last. IDs are unique and
 * nonzero, strings are copied, at most 128 segments. Ancestors are native
 * keyboard-focusable buttons; the current location is a label. Long paths
 * wrap. Replacements are silent and reset the last activated ID; invalid
 * replacements preserve the existing path. Empty paths are supported. */
cui_widget *cui_breadcrumbs(cui_widget *parent, const cui_breadcrumb_item *items, size_t count);
int cui_breadcrumbs_set_items(cui_widget *breadcrumbs, const cui_breadcrumb_item *items, size_t count);
cui_item_id cui_breadcrumbs_current(const cui_widget *breadcrumbs);
cui_item_id cui_breadcrumbs_activated(const cui_widget *breadcrumbs);
/* Explicitly invoke an ancestor's action, like cui_activate. Does not change
 * the path; the application handles navigation in the root's on_action. */
int cui_breadcrumbs_activate(cui_widget *breadcrumbs, cui_item_id id);
#ifdef __cplusplus
}
#endif
#endif
