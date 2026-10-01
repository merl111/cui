#ifndef CUI_GTK_H
#define CUI_GTK_H
#include "cui_internal.h"
#include <gtk/gtk.h>
void cui__gtk_table_items(cui_widget *widget);
GtkWidget *cui__gtk_tree(cui_widget *widget);
GtkWidget *cui__gtk_control(cui_widget *widget, const char *text);
void cui__gtk_action(GtkWidget *native, gpointer data);
GtkWidget *cui__gtk_container(cui_widget *widget);
void cui__gtk_append(cui_widget *parent,cui_widget *child);
const char *cui__gtk_styles(void);
#endif
