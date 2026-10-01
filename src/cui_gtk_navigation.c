#include "cui_gtk.h"
#include "cui_navigation_internal.h"

static cui_item_id row_id(GtkTreeListRow *row)
{
    GObject *item = row ? gtk_tree_list_row_get_item(row) : NULL;
    cui_item_id *id = item ? g_object_get_data(item, "cui-id") : NULL;
    cui_item_id result = id ? *id : 0;
    /* get_item transfers a reference, unlike gtk_list_item_get_item. */
    g_clear_object(&item);
    return result;
}
static GListModel *children(gpointer item, gpointer data)
{
    (void)data;
    GListModel *model = g_object_get_data(item, "cui-children");
    return model ? g_object_ref(model) : NULL;
}
static void expanded(GObject *row, GParamSpec *spec, gpointer data)
{
    (void)spec;
    cui__tree_event(data, gtk_tree_list_row_get_expanded(GTK_TREE_LIST_ROW(row)) ? CUI_TREE_EXPAND : CUI_TREE_COLLAPSE,
                    row_id(GTK_TREE_LIST_ROW(row)));
}
static void setup(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
    (void)factory; (void)data;
    GtkWidget *expander = gtk_tree_expander_new(), *label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), label);
    gtk_list_item_set_child(item, expander);
    gtk_list_item_set_focusable(item, FALSE);
}
static void bind(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
    (void)factory; (void)data;
    GtkTreeListRow *row = gtk_list_item_get_item(item);
    GObject *node = gtk_tree_list_row_get_item(row);
    GtkTreeExpander *expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(item));
    gtk_tree_expander_set_list_row(expander, row);
    gtk_label_set_text(GTK_LABEL(gtk_tree_expander_get_child(expander)), g_object_get_data(node, "cui-text"));
    g_clear_object(&node);
}
static void unbind(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
    (void)factory; (void)data;
    gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(gtk_list_item_get_child(item)), NULL);
}
static void selection_changed(GObject *selection, GParamSpec *spec, gpointer data)
{
    (void)spec;
    cui__tree_event(data, CUI_TREE_SELECTION, row_id(gtk_single_selection_get_selected_item(GTK_SINGLE_SELECTION(selection))));
}
static void activated(GtkListView *view, guint position, gpointer data)
{
    GtkTreeListRow *row = g_list_model_get_item(G_LIST_MODEL(gtk_list_view_get_model(view)), position);
    cui_item_id id = row_id(row);
    g_clear_object(&row); /* A callback may replace the model. */
    cui__tree_event(data, CUI_TREE_ACTIVATE, id);
}
GtkWidget *cui__gtk_tree(cui_widget *w)
{
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(setup), w);
    g_signal_connect(factory, "bind", G_CALLBACK(bind), w);
    g_signal_connect(factory, "unbind", G_CALLBACK(unbind), w);
    GtkWidget *view = gtk_list_view_new(NULL, factory), *scroll = gtk_scrolled_window_new();
    g_signal_connect(view, "activate", G_CALLBACK(activated), w);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view);
    gtk_widget_set_size_request(scroll, 240, 180);
    gtk_widget_add_css_class(scroll, "cui-input-surface");
    w->aux = view; return scroll;
}
static GListStore *store_nodes(cui_tree_model *m, int first)
{
    GListStore *store = g_list_store_new(G_TYPE_OBJECT);
    for (int i = first; i >= 0; i = m->nodes[i].next) {
        cui_tree_node *n = m->nodes + i;
        GObject *item = g_object_new(G_TYPE_OBJECT, NULL);
        cui_item_id *id = g_new(cui_item_id, 1); *id = n->id;
        g_object_set_data_full(item, "cui-id", id, g_free);
        g_object_set_data_full(item, "cui-text", g_strdup(n->text), g_free);
        if (n->first >= 0) g_object_set_data_full(item, "cui-children", store_nodes(m, n->first), g_object_unref);
        g_list_store_append(store, item); g_object_unref(item);
    }
    return store;
}
/* Watch all visible rows, including unrealized rows. Virtualized row widgets
 * come and go independently of expansion state and event delivery. */
static void watch_rows(GListModel *model, guint position, guint removed, guint added, gpointer data)
{
    (void)removed; (void)position; (void)added;
    cui_widget *w = data;
    if (g_object_get_data(G_OBJECT(model), "cui-scanning")) return;
    g_object_set_data(G_OBJECT(model), "cui-scanning", GINT_TO_POINTER(1));
    for (guint i = 0; i < g_list_model_get_n_items(model); ++i) {
        GtkTreeListRow *row = g_list_model_get_item(model, i);
        if (!g_object_get_data(G_OBJECT(row), "cui-watched")) {
            g_object_set_data(G_OBJECT(row), "cui-watched", GINT_TO_POINTER(1));
            g_signal_connect(row, "notify::expanded", G_CALLBACK(expanded), w);
            cui_tree_node *n = cui__tree_find(w, row_id(row));
            if (n && n->expanded) { ++w->updating; gtk_tree_list_row_set_expanded(row, TRUE); --w->updating; }
        }
        g_object_unref(row);
    }
    g_object_set_data(G_OBJECT(model), "cui-scanning", NULL);
}
void cui__backend_tree_items(cui_widget *w)
{
    cui_tree_model *m = w->payload;
    GtkTreeListModel *tree = gtk_tree_list_model_new(G_LIST_MODEL(store_nodes(m, m->first)), FALSE, FALSE, children, NULL, NULL);
    g_signal_connect(tree, "items-changed", G_CALLBACK(watch_rows), w);
    watch_rows(G_LIST_MODEL(tree), 0, 0, g_list_model_get_n_items(G_LIST_MODEL(tree)), w);
    GtkSingleSelection *selection = gtk_single_selection_new(G_LIST_MODEL(tree));
    gtk_single_selection_set_autoselect(selection, FALSE);
    gtk_single_selection_set_can_unselect(selection, TRUE);
    gtk_single_selection_set_selected(selection, GTK_INVALID_LIST_POSITION);
    gtk_list_view_set_model(GTK_LIST_VIEW(w->aux), GTK_SELECTION_MODEL(selection));
    g_signal_connect(selection, "notify::selected", G_CALLBACK(selection_changed), w);
    g_object_unref(selection);
}
static GtkTreeListRow *find_row(cui_widget *w, cui_item_id id, guint *position)
{
    GListModel *model = G_LIST_MODEL(gtk_list_view_get_model(GTK_LIST_VIEW(w->aux)));
    for (guint i = 0; i < g_list_model_get_n_items(model); ++i) {
        GtkTreeListRow *row = g_list_model_get_item(model, i);
        if (row_id(row) == id) { if (position) *position = i; return row; }
        g_object_unref(row);
    }
    return NULL;
}
void cui__backend_tree_select(cui_widget *w, cui_tree_node *node)
{
    guint position = GTK_INVALID_LIST_POSITION;
    GtkTreeListRow *row = node ? find_row(w, node->id, &position) : NULL;
    gtk_single_selection_set_selected(GTK_SINGLE_SELECTION(gtk_list_view_get_model(GTK_LIST_VIEW(w->aux))), position);
    g_clear_object(&row);
}
void cui__backend_tree_expand(cui_widget *w, cui_tree_node *node)
{
    GtkTreeListRow *row = find_row(w, node->id, NULL);
    if (row) gtk_tree_list_row_set_expanded(row, node->expanded);
    g_clear_object(&row);
}
