#include "cui_tables_internal.h"
#include "gtk_find.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"tables:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static cui_app *app;
static cui_widget *table;
static cui_timer *timer;
static int events,phase;
static void changed(cui_widget *sender,void *data)
{ (void)data;CHECK(sender==table);++events; }
static int editor_matches(GtkWidget *widget,const void *text)
{ return GTK_IS_EDITABLE_LABEL(widget) && !strcmp(gtk_editable_get_text(GTK_EDITABLE(widget)),(const char *)text); }
static GtkWidget *editor(GtkWidget *widget,const char *text)
{ return cui_test_find_widget(widget,editor_matches,text); }
static unsigned widgets(GtkWidget *w)
{
    unsigned count=GTK_IS_EDITABLE_LABEL(w)?1:0;
    for(GtkWidget *c=gtk_widget_get_first_child(w);c;c=gtk_widget_get_next_sibling(c))count+=widgets(c);
    return count;
}
static void verify(void *data)
{
    (void)data;
    if(phase++){
        unsigned realized=widgets(GTK_WIDGET(table->aux));
        CHECK(realized>0&&realized<1000);
        CHECK(cui_table_select_row(table,9999,1));
        size_t selected[2];CHECK(cui_table_selected_rows(table,selected,2)==1&&selected[0]==9999);
        cui_timer_stop(timer);cui_app_quit(app);return;
    }
    char text[64];size_t selected[4];
    GtkColumnViewColumn *width_column=g_list_model_get_item(gtk_column_view_get_columns(GTK_COLUMN_VIEW(table->aux)),0);
    gtk_column_view_column_set_fixed_width(width_column,217);g_object_unref(width_column);
    cui_table_set_multiple(table,1);
    CHECK(cui_table_select_row(table,0,1)&&cui_table_select_row(table,2,1));
    CHECK(cui_table_selected_rows(table,selected,4)==2&&selected[0]==0&&selected[1]==2&&events==0);
    CHECK(!cui_table_select_row(table,4,1));
    cui_table_set_multiple(table,0);CHECK(cui_table_selected_rows(table,selected,4)==1&&selected[0]==0);
    cui_table_set_multiple(table,1);
    GtkSelectionModel *model=gtk_column_view_get_model(GTK_COLUMN_VIEW(table->aux));
    gtk_selection_model_select_item(model,3,FALSE);
    CHECK(cui_table_selected_rows(table,selected,4)==2&&events>0);
    int row,column;CHECK(cui_table_last_event(table,&row,&column)==CUI_TABLE_SELECTION);
    events=0;
    CHECK(cui_table_set_cell(table,1,2,"Updated · 世界"));
    CHECK(cui_table_get_cell(table,1,2,text,sizeof(text))==strlen("Updated · 世界")&&!strcmp(text,"Updated · 世界"));
    CHECK(!cui_table_set_cell(table,4,0,"bad"));CHECK(!cui_table_set_editable(table,3,1));
    CHECK(cui_table_sort(table,1,0,1)&&events==0);
    width_column=g_list_model_get_item(gtk_column_view_get_columns(GTK_COLUMN_VIEW(table->aux)),0);
    CHECK(gtk_column_view_column_get_fixed_width(width_column)==217);g_object_unref(width_column);
    cui_table_get_cell(table,0,0,text,sizeof(text));CHECK(!strcmp(text,"Alpha"));
    CHECK(cui_table_source_row(table,0)==1 && cui_table_source_row(table,1)==2);
    cui_table_get_cell(table,0,2,text,sizeof(text));CHECK(!strcmp(text,"Updated · 世界"));
    CHECK(cui_table_selected_rows(table,NULL,0)==0);
    cui_table_get_cell(table,3,1,text,sizeof(text));CHECK(!strcmp(text,"n/a"));
    CHECK(cui_table_sort(table,1,1,1));
    cui_table_get_cell(table,0,0,text,sizeof(text));CHECK(!strcmp(text,"Beta"));
    /* Model replacement rebuilds native rows; wait a frame before editing. */
    const char *cells[]={"Beta","10","one","Alpha","2","two","Alpha","2","three","Other","n/a","four"};
    CHECK(cui_table_set_rows(table,cells,4));
    CHECK(cui_table_set_editable(table,0,1));
}
static void edit_after_frame(void *data)
{
    (void)data;
    if(!phase)return;
    GtkWidget *cell=editor(GTK_WIDGET(table->aux),"Beta");if(!cell)return;
    g_object_ref(cell);events=0;
    gtk_editable_label_start_editing(GTK_EDITABLE_LABEL(cell));
    gtk_editable_set_text(GTK_EDITABLE(cell),"編集");
    gtk_editable_label_stop_editing(GTK_EDITABLE_LABEL(cell),TRUE);
    char text[64];cui_table_get_cell(table,0,0,text,sizeof(text));CHECK(!strcmp(text,"編集"));
    int row,column;CHECK(events==1&&cui_table_last_event(table,&row,&column)==CUI_TABLE_EDIT&&row==0&&column==0);
    gtk_editable_label_start_editing(GTK_EDITABLE_LABEL(cell));
    gtk_editable_set_text(GTK_EDITABLE(cell),"Cancel this edit");
    gtk_editable_label_stop_editing(GTK_EDITABLE_LABEL(cell),FALSE);CHECK(events==1);
    g_object_unref(cell);
#if GTK_CHECK_VERSION(4,10,0)
    GtkColumnViewColumn *native=g_list_model_get_item(gtk_column_view_get_columns(GTK_COLUMN_VIEW(table->aux)),0);
    gtk_column_view_sort_by_column(GTK_COLUMN_VIEW(table->aux),native,GTK_SORT_ASCENDING);g_object_unref(native);
    CHECK(cui_table_last_event(table,&row,&column)==CUI_TABLE_SORT&&column==0);
    cui_table_get_cell(table,0,0,text,sizeof(text));CHECK(!strcmp(text,"Alpha"));
#endif
    const char **cells=calloc(30000,sizeof(*cells));CHECK(cells);
    for(size_t i=0;i<10000;++i){cells[3*i]="Row";cells[3*i+1]="1";cells[3*i+2]="Detail";}
    events=0;CHECK(cui_table_set_rows(table,cells,10000));free(cells);
    CHECK(events==0&&g_list_model_get_n_items(G_LIST_MODEL(gtk_column_view_get_model(GTK_COLUMN_VIEW(table->aux))))==10000);
    phase=2;
}
static void tick(void *data)
{
    (void)data;
    if(!phase)verify(NULL);
    else if(phase==1)edit_after_frame(NULL);
    else verify(NULL);
}
int main(void)
{
    app=cui_app_create();CHECK(app);
    cui_window *window=cui_window_create(app,"Editable table",800,520);CHECK(window);
    const char *headers[]={"Name","Count","Detail"};
    const char *cells[]={"Beta","10","one","Alpha","2","two","Alpha","2","three","Other","n/a","four"};
    table=cui_table(cui_window_root(window),headers,3);CHECK(table&&cui_table_set_rows(table,cells,4));
    cui_expand(table,1);cui_on_action(table,changed,NULL);
    timer=cui_every(app,150,tick,NULL);cui_window_show(window);cui_app_run(app);cui_app_destroy(app);
    puts("tables: multiple selection, stable numeric sorting, UTF-8 editing/cancel, headers and 10000 virtualized rows passed");return 0;
}
