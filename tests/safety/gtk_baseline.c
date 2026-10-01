/* Standalone upstream-allocation reproducer: deliberately does not link CUI. */
#include <gtk/gtk.h>
#include <string.h>
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static GtkWidget *first_cell;
static void setup(GtkSignalListItemFactory *factory,GtkListItem *item,gpointer data)
{
    (void)factory;(void)data;
    GtkWidget *cell=gtk_editable_label_new("Editable row");
    gtk_list_item_set_child(item,cell);
    if(!first_cell)first_cell=cell;
}
static gboolean edit(gpointer data)
{
    (void)data;
    if(first_cell){
        gtk_editable_label_start_editing(GTK_EDITABLE_LABEL(first_cell));
        gtk_editable_label_stop_editing(GTK_EDITABLE_LABEL(first_cell),FALSE);
    }
    return G_SOURCE_REMOVE;
}
static gboolean done(gpointer loop){g_main_loop_quit(loop);return G_SOURCE_REMOVE;}
int main(int argc,char **argv)
{
    gtk_init();
    GtkWidget *window=gtk_window_new(),*box=gtk_box_new(GTK_ORIENTATION_VERTICAL,8);
    gtk_window_set_child(GTK_WINDOW(window),box);
    gtk_box_append(GTK_BOX(box),gtk_label_new("Font baseline: 世界 — Grüße"));
    gtk_box_append(GTK_BOX(box),gtk_calendar_new());
    GtkFileChooserNative *files=NULL;
    if(argc>1 && !strcmp(argv[1],"files")){
        files=gtk_file_chooser_native_new("Baseline",GTK_WINDOW(window),GTK_FILE_CHOOSER_ACTION_OPEN,"Open","Cancel");
        gtk_native_dialog_show(GTK_NATIVE_DIALOG(files));
    }
    if(argc>1 && !strcmp(argv[1],"pickers")){
        for(int i=0;i<2;++i){
            GtkWidget *font=gtk_font_chooser_dialog_new("Font",GTK_WINDOW(window));
            gtk_window_present(GTK_WINDOW(font));
            while(g_main_context_iteration(NULL,FALSE)){}
            gtk_window_destroy(GTK_WINDOW(font));
        }
        for(int i=0;i<3;++i){
            GtkWidget *color=gtk_color_chooser_dialog_new("Color",GTK_WINDOW(window));
            gtk_window_destroy(GTK_WINDOW(color));
        }
    }
    if(argc>1 && !strcmp(argv[1],"table")){
        GListStore *rows=g_list_store_new(G_TYPE_OBJECT);
        GObject *row=g_object_new(G_TYPE_OBJECT,NULL);g_list_store_append(rows,row);g_object_unref(row);
        GtkWidget *view=gtk_column_view_new(GTK_SELECTION_MODEL(gtk_single_selection_new(G_LIST_MODEL(rows))));
        for(int i=0;i<3;++i){
            GtkListItemFactory *factory=gtk_signal_list_item_factory_new();
            g_signal_connect(factory,"setup",G_CALLBACK(setup),NULL);
            GtkColumnViewColumn *column=gtk_column_view_column_new("Name",factory);
            gtk_column_view_append_column(GTK_COLUMN_VIEW(view),column);g_object_unref(column);
        }
        gtk_box_append(GTK_BOX(box),view);g_timeout_add(100,edit,NULL);
    }
    GMainLoop *loop=g_main_loop_new(NULL,FALSE);
    gtk_window_present(GTK_WINDOW(window));g_timeout_add(350,done,loop);g_main_loop_run(loop);
    if(files){gtk_native_dialog_destroy(GTK_NATIVE_DIALOG(files));g_object_unref(files);}
    gtk_window_destroy(GTK_WINDOW(window));
    while(g_main_context_iteration(NULL,FALSE)){}
    g_main_loop_unref(loop);
    return 0;
}

G_GNUC_END_IGNORE_DEPRECATIONS
