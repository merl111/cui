#ifndef CUI_TEST_GTK_FIND_H
#define CUI_TEST_GTK_FIND_H
#include <gtk/gtk.h>
typedef int (*cui_test_widget_match)(GtkWidget *, const void *);
/* Test-only depth-first search through native, private GTK child trees. */
static GtkWidget *cui_test_find_widget(GtkWidget *root, cui_test_widget_match match, const void *data)
{
    if (match(root,data)) return root;
    for (GtkWidget *child=gtk_widget_get_first_child(root);child;child=gtk_widget_get_next_sibling(child)) {
        GtkWidget *found=cui_test_find_widget(child,match,data);
        if (found) return found;
    }
    return NULL;
}
#endif
