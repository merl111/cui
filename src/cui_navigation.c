#include "cui_navigation_internal.h"
#include <stdlib.h>
#include <string.h>

static int compare_ids(const void *a, const void *b)
{
    cui_item_id x = ((const cui_tree_index *)a)->id, y = ((const cui_tree_index *)b)->id;
    return (x > y) - (x < y);
}
cui_tree_node *cui__tree_model_find(const cui_tree_model *m, cui_item_id id)
{
    cui_tree_index key = {id, 0};
    const cui_tree_index *found = m && m->count ? bsearch(&key, m->lookup, m->count, sizeof(key), compare_ids) : NULL;
    return found ? m->nodes + found->index : NULL;
}
cui_tree_node *cui__tree_find(const cui_widget *w, cui_item_id id)
{ return w && w->kind == CUI_TREE ? cui__tree_model_find(w->payload, id) : NULL; }
void cui__tree_model_free(void *data)
{
    cui_tree_model *m = data;
    if (!m) return;
    for (size_t i = 0; i < m->count; ++i) free(m->nodes[i].text);
    free(m->nodes); free(m->lookup); free(m);
}
static int link_nodes(cui_tree_model *m, const cui_tree_item *items)
{
    int *tails = malloc((m->count + 1) * sizeof(*tails));
    if (!tails) return 0;
    for (size_t i = 0; i <= m->count; ++i) tails[i] = -1;
    for (size_t i = 0; i < m->count; ++i) {
        cui_tree_node *n = m->nodes + i;
        cui_tree_node *parent = items[i].parent ? cui__tree_model_find(m, items[i].parent) : NULL;
        if (items[i].parent && (!parent || parent >= n || parent->depth >= 128)) { free(tails); return 0; }
        n->parent = parent ? (int)(parent - m->nodes) : -1;
        n->depth = parent ? parent->depth + 1 : 1;
        int *first = parent ? &parent->first : &m->first;
        int *last = tails + n->parent + 1;
        if (*last < 0) *first = (int)i; else m->nodes[*last].next = (int)i;
        *last = (int)i;
    }
    free(tails); return 1;
}
cui_tree_model *cui__tree_model_copy(const cui_tree_item *items, size_t count)
{
    if (count > 65536 || (count && !items)) return NULL;
    cui_tree_model *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->first = -1;
    if (!count) return m;
    m->nodes = calloc(count, sizeof(*m->nodes)); m->lookup = calloc(count, sizeof(*m->lookup));
    if (!m->nodes || !m->lookup) { cui__tree_model_free(m); return NULL; }
    m->count = count;
    for (size_t i = 0; i < count; ++i) {
        cui_tree_node *n = m->nodes + i;
        const char *text = items[i].text ? items[i].text : "";
        n->id = items[i].id; n->first = n->next = -1; n->expanded = !!items[i].expanded;
        n->text = malloc(strlen(text) + 1);
        if (!n->id || !n->text) { cui__tree_model_free(m); return NULL; }
        strcpy(n->text, text); m->lookup[i] = (cui_tree_index){n->id, i};
    }
    qsort(m->lookup, count, sizeof(*m->lookup), compare_ids);
    for (size_t i = 1; i < count; ++i)
        if (m->lookup[i-1].id == m->lookup[i].id) { cui__tree_model_free(m); return NULL; }
    if (!link_nodes(m, items)) { cui__tree_model_free(m); return NULL; }
    for (size_t i = 0; i < count; ++i) if (m->nodes[i].first < 0) m->nodes[i].expanded = 0;
    return m;
}
int cui_tree_set_items(cui_widget *w, const cui_tree_item *items, size_t count)
{
    if (!w || w->kind != CUI_TREE) return 0;
    cui_tree_model *m = cui__tree_model_copy(items, count);
    if (!m) { w->window->app->error = "Invalid tree model or out of memory"; return 0; }
    void *old = w->payload;
    ++w->updating; w->payload = m; w->destroy_payload = cui__tree_model_free;
    cui__backend_tree_items(w);
    --w->updating; cui__tree_model_free(old);
    return 1;
}
cui_widget *cui_tree(cui_widget *parent, const cui_tree_item *items, size_t count)
{
    /* Validate before attaching a native widget to the parent. */
    cui_tree_model *m = cui__tree_model_copy(items, count);
    if (!m) return NULL;
    cui_widget *w = cui__append(parent, CUI_TREE, "", CUI_VERTICAL, 0);
    if (!w) { cui__tree_model_free(m); return NULL; }
    w->payload = m; w->destroy_payload = cui__tree_model_free;
    ++w->updating; cui__backend_tree_items(w); --w->updating;
    return w;
}
static void reveal(cui_widget *w, cui_tree_node *n)
{
    if (n->parent < 0) return;
    cui_tree_model *m = w->payload;
    cui_tree_node *parent = m->nodes + n->parent;
    reveal(w, parent); parent->expanded = 1; cui__backend_tree_expand(w, parent);
}
int cui_tree_select(cui_widget *w, cui_item_id id)
{
    if (!w || w->kind != CUI_TREE) return 0;
    cui_tree_node *n = cui__tree_find(w, id);
    if (id && !n) return 0;
    ++w->updating;
    if (n) reveal(w, n);
    ((cui_tree_model *)w->payload)->selected = id;
    cui__backend_tree_select(w, n);
    --w->updating; return 1;
}
cui_item_id cui_tree_selected(const cui_widget *w)
{ return w && w->kind == CUI_TREE ? ((cui_tree_model *)w->payload)->selected : 0; }
static void collapse_selection(cui_widget *w, cui_tree_node *branch)
{
    cui_tree_model *m = w->payload;
    cui_tree_node *selected = cui__tree_model_find(m, m->selected);
    while (selected && selected->parent >= 0) {
        selected = m->nodes + selected->parent;
        if (selected == branch) { cui_tree_select(w, branch->id); return; }
    }
}
int cui_tree_expand(cui_widget *w, cui_item_id id, int expanded)
{
    cui_tree_node *n = cui__tree_find(w, id);
    if (!n) return 0;
    ++w->updating;
    if (!expanded) collapse_selection(w, n);
    n->expanded = !!expanded && n->first >= 0;
    cui__backend_tree_expand(w, n); --w->updating; return 1;
}
int cui_tree_is_expanded(const cui_widget *w, cui_item_id id)
{ cui_tree_node *n = cui__tree_find(w, id); return n ? n->expanded : 0; }
cui_tree_event cui_tree_last_event(const cui_widget *w, cui_item_id *id)
{
    const cui_tree_model *m = w && w->kind == CUI_TREE ? w->payload : NULL;
    if (id) *id = m ? m->event_id : 0;
    return m ? m->event : CUI_TREE_NONE;
}
void cui__tree_event(cui_widget *w, cui_tree_event event, cui_item_id id)
{
    cui_tree_model *m = w->payload;
    if (!m || w->updating) return;
    cui_tree_node *n = cui__tree_model_find(m, id);
    if (id && !n) return;
    if (event == CUI_TREE_SELECTION) m->selected = id;
    if (n && event == CUI_TREE_COLLAPSE) collapse_selection(w, n);
    if (n && (event == CUI_TREE_EXPAND || event == CUI_TREE_COLLAPSE)) n->expanded = event == CUI_TREE_EXPAND;
    m->event = event; m->event_id = id; cui__emit(w);
}
