#include "cui_navigation_internal.h"
#include <gtk/gtk.h>
#include "cui_desktop.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"navigation:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static cui_app *app;
static cui_widget *tree,*crumbs;
static int crumb_events;
static const cui_breadcrumb_item path[]={{90,"Workspace"},{2,"Sources"},{300,"東京.c"}};
static void crumb_changed(cui_widget *sender,void *data)
{ (void)data;CHECK(sender==crumbs);++crumb_events; }
static void verify_crumbs(void)
{
    CHECK(cui_breadcrumbs_current(crumbs)==300 && !cui_breadcrumbs_activated(crumbs));
    CHECK(!cui_breadcrumbs_activate(crumbs,300) && !cui_breadcrumbs_activate(crumbs,77));
    cui_widget *button=crumbs->first->first->next;
    CHECK(cui_focus(button)&&cui_has_focus(button));
    g_signal_emit_by_name(button->native,"clicked");CHECK(crumb_events==1 && cui_breadcrumbs_activated(crumbs)==90);
    cui_breadcrumb_item invalid[]={{90,"Root"},{90,"Duplicate"}};
    CHECK(!cui_breadcrumbs_set_items(crumbs,invalid,2));CHECK(cui_breadcrumbs_current(crumbs)==300);
    CHECK(!cui_breadcrumbs_set_items(crumbs,NULL,1));
    CHECK(cui_breadcrumbs_activate(crumbs,2)&&crumb_events==2);
    cui_set_enabled(crumbs,0);CHECK(!cui_breadcrumbs_activate(crumbs,2));cui_set_enabled(crumbs,1);
    for(int i=0;i<50;++i){CHECK(cui_breadcrumbs_set_items(crumbs,path,1));CHECK(cui_breadcrumbs_set_items(crumbs,path,3));}
    size_t children=0;for(cui_widget *row=crumbs->first;row;row=row->next)++children;
    CHECK(children==3 && crumb_events==2 && !cui_breadcrumbs_activated(crumbs));
    CHECK(!crumbs->last->last->hidden && crumbs->last->first->next->hidden);
    char copied[]="Mutable";cui_breadcrumb_item copy[]={{1,copied},{2,"Current"}};
    CHECK(cui_breadcrumbs_set_items(crumbs,copy,2));copied[0]='X';char label[32];
    cui_get_text(button,label,sizeof(label));CHECK(!strcmp(label,"Mutable"));
    CHECK(cui_breadcrumbs_set_items(crumbs,NULL,0));CHECK(!cui_breadcrumbs_current(crumbs));
    CHECK(crumbs->first->hidden && !cui_breadcrumbs_activate(crumbs,1));
}
static cui_timer *timer;
static int events;
static const cui_tree_item items[] = {
    {90, 0, "Workspace", 1}, {2, 90, "Sources", 0}, {300, 2, "東京.c", 0},
    {4, 90, "README.md", 0}, {5, 0, "Examples", 1}, {6, 5, "Python", 0}
};
static void event(cui_widget *sender, void *data)
{ (void)data; CHECK(sender == tree); ++events; }
static GListModel *model(void)
{ return G_LIST_MODEL(gtk_list_view_get_model(GTK_LIST_VIEW(tree->aux))); }
static void verify(void *data)
{
    (void)data; cui_timer_stop(timer);verify_crumbs();
    CHECK(g_list_model_get_n_items(model()) == 5);
    CHECK(cui_tree_selected(tree) == 0 && events == 0);
    CHECK(cui_tree_select(tree, 300));
    CHECK(cui_tree_selected(tree) == 300 && cui_tree_is_expanded(tree, 2));
    CHECK(g_list_model_get_n_items(model()) == 6 && events == 0);
    CHECK(cui_tree_expand(tree, 2, 0));
    CHECK(cui_tree_selected(tree) == 2 && events == 0);
    CHECK(cui_tree_select(tree, 300));
    cui_tree_item invalid[] = {{1, 0, "Root", 0}, {1, 0, "Duplicate", 0}};
    CHECK(!cui_tree_set_items(tree, invalid, 2));
    invalid[1].id = 2; invalid[1].parent = 42;
    CHECK(!cui_tree_set_items(tree, invalid, 2));
    invalid[0].parent = 2; invalid[1].parent = 0;
    CHECK(!cui_tree_set_items(tree, invalid, 2));
    CHECK(cui_tree_selected(tree) == 300 && !cui_tree_select(tree, 999));
    gtk_single_selection_set_selected(GTK_SINGLE_SELECTION(model()), 3);
    CHECK(cui_tree_selected(tree) == 4 && events == 1);
    cui_item_id id;
    CHECK(cui_tree_last_event(tree, &id) == CUI_TREE_SELECTION && id == 4);
    GtkTreeListRow *row = g_list_model_get_item(model(), 1);
    gtk_tree_list_row_set_expanded(row, FALSE);
    CHECK(!cui_tree_is_expanded(tree, 2));
    CHECK(cui_tree_last_event(tree, &id) == CUI_TREE_COLLAPSE && id == 2);
    gtk_tree_list_row_set_expanded(row, TRUE); g_object_unref(row);
    CHECK(cui_tree_is_expanded(tree, 2));
    g_signal_emit_by_name(tree->aux, "activate", 2);
    CHECK(cui_tree_last_event(tree, &id) == CUI_TREE_ACTIVATE && id == 300);
    events = 0;
    for (int i = 0; i < 10; ++i) {
        CHECK(cui_tree_set_items(tree, items, 6));
        CHECK(cui_tree_select(tree, 6));
    }
    CHECK(events == 0);
    CHECK(cui_tree_set_items(tree, NULL, 0));
    CHECK(cui_tree_selected(tree) == 0 && g_list_model_get_n_items(model()) == 0);
    cui_tree_item *large = calloc(10000, sizeof(*large)); CHECK(large);
    for (size_t i = 0; i < 10000; ++i) large[i] = (cui_tree_item){i + 1, i ? 1 : 0, "Virtualized row", !i};
    CHECK(cui_tree_set_items(tree, large, 10000)); free(large);
    CHECK(g_list_model_get_n_items(model()) == 10000);
    CHECK(cui_tree_select(tree, 10000) && cui_tree_selected(tree) == 10000);
    CHECK(events == 0);
    cui_app_quit(app);
}
int main(void)
{
    app = cui_app_create(); CHECK(app);
    cui_window *window = cui_window_create(app, "Native tree", 620, 500); CHECK(window);
    crumbs=cui_breadcrumbs(cui_window_root(window),path,3);CHECK(crumbs);cui_on_action(crumbs,crumb_changed,NULL);
    tree = cui_tree(cui_window_root(window), items, 6); CHECK(tree);
    cui_on_action(tree, event, NULL); cui_expand(tree, 1);
    timer = cui_every(app, 150, verify, NULL);
    cui_window_show(window); cui_app_run(app); cui_app_destroy(app);
    puts("navigation: nested data, IDs, validation, native events, replacement and 10000 rows passed");
    return 0;
}
