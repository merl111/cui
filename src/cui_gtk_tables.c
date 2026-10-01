#include "cui_gtk.h"
#include "cui_tables_internal.h"
#include <string.h>
typedef struct table_column { cui_widget *widget; size_t column; } table_column;
static GObject *row_object(size_t row)
{
    GObject *item=g_object_new(G_TYPE_OBJECT,NULL);
    g_object_set_data(item,"cui-row",GSIZE_TO_POINTER(row+1));return item;
}
static void editing(GObject *object,GParamSpec *spec,gpointer data)
{
    (void)spec;(void)data;
    if(gtk_editable_label_get_editing(GTK_EDITABLE_LABEL(object)))return;
    table_column *context=g_object_get_data(object,"cui-column");
    size_t position=GPOINTER_TO_SIZE(g_object_get_data(object,"cui-row"));
    if(!position)return;
    char *text=g_strdup(gtk_editable_get_text(GTK_EDITABLE(object)));
    cui__table_edit(context->widget,position-1,context->column,text);g_free(text);
}
static void setup(GtkSignalListItemFactory *factory,GtkListItem *item,gpointer data)
{
    (void)factory;table_column *context=data;
    cui_table_state *s=context->widget->payload;
    GtkWidget *cell;
    if(s->editable[context->column]){
        cell=gtk_editable_label_new("");
        table_column *copy=g_new(table_column,1);*copy=*context;
        g_object_set_data_full(G_OBJECT(cell),"cui-column",copy,g_free);
        g_signal_connect(cell,"notify::editing",G_CALLBACK(editing),NULL);
    }else{cell=gtk_label_new("");gtk_label_set_xalign(GTK_LABEL(cell),0);}
    g_object_set_data(G_OBJECT(cell),"cui-column-index",GSIZE_TO_POINTER(context->column+1));
    gtk_list_item_set_child(item,cell);
}
static void bind(GtkSignalListItemFactory *factory,GtkListItem *item,gpointer data)
{
    (void)factory;table_column *context=data;cui_widget *w=context->widget;
    GObject *object=gtk_list_item_get_item(item);
    size_t row=GPOINTER_TO_SIZE(g_object_get_data(object,"cui-row"));
    GtkWidget *cell=gtk_list_item_get_child(item);
    const char *text=row&&row<=((cui_table_state *)w->payload)->rows?w->items[(row-1)*w->columns+context->column]:"";
    g_object_set_data(G_OBJECT(cell),"cui-row",GSIZE_TO_POINTER(row));
    if(GTK_IS_EDITABLE_LABEL(cell))gtk_editable_set_text(GTK_EDITABLE(cell),text);
    else gtk_label_set_text(GTK_LABEL(cell),text);
}
static void unbind(GtkSignalListItemFactory *factory,GtkListItem *item,gpointer data)
{
    (void)factory;(void)data;
    GtkWidget *cell=gtk_list_item_get_child(item);
    g_object_set_data(G_OBJECT(cell),"cui-row",NULL);
    if(GTK_IS_EDITABLE_LABEL(cell))gtk_editable_label_stop_editing(GTK_EDITABLE_LABEL(cell),FALSE);
}
static void selected(GtkSelectionModel *selection,guint position,guint count,gpointer data)
{
    (void)position;(void)count;cui_widget *w=data;
    if(w->updating)return;
    cui_table_state *s=w->payload;if(s->rows)memset(s->selected,0,s->rows);
    GtkBitset *set=gtk_selection_model_get_selection(selection);GtkBitsetIter iter;guint value;
    for(gboolean valid=gtk_bitset_iter_init_first(&iter,set,&value);valid;valid=gtk_bitset_iter_next(&iter,&value))
        if(value<s->rows)s->selected[value]=1;
    gtk_bitset_unref(set);cui__table_selection_changed(w);
}
#if GTK_CHECK_VERSION(4,10,0)
static gint unused_compare(gconstpointer a,gconstpointer b,gpointer data)
{(void)a;(void)b;(void)data;return GTK_ORDERING_EQUAL;}
static void sort_changed(GtkSorter *sorter,GtkSorterChange change,gpointer data)
{
    (void)change;cui_widget *w=data;if(w->updating)return;
    GtkColumnViewColumn *column=gtk_column_view_sorter_get_primary_sort_column(GTK_COLUMN_VIEW_SORTER(sorter));
    if(!column)return;
    size_t index=GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(column),"cui-column"))-1;
    cui__table_sort(w,index,gtk_column_view_sorter_get_primary_sort_order(GTK_COLUMN_VIEW_SORTER(sorter))==GTK_SORT_DESCENDING);
}
#endif
static void columns(cui_widget *w)
{
    GtkColumnView *view=GTK_COLUMN_VIEW(w->aux);GListModel *model=gtk_column_view_get_columns(view);
    cui_table_state *s=w->payload;
    /* Columns have a fixed schema. Reuse them when rows/sorting change; this
     * preserves focus and widths and avoids tearing down active GTK headers. */
    for(size_t i=0;i<w->columns;++i){
        GtkColumnViewColumn *column=i<g_list_model_get_n_items(model)?g_list_model_get_item(model,(guint)i):NULL;
        int editable=s->editable[i]+1;
        if(column && GPOINTER_TO_INT(g_object_get_data(G_OBJECT(column),"cui-editable"))==editable){
            g_object_unref(column);continue;
        }
        table_column *context=g_new(table_column,1);*context=(table_column){w,i};
        GtkListItemFactory *factory=gtk_signal_list_item_factory_new();
        g_object_set_data_full(G_OBJECT(factory),"cui-column",context,g_free);
        g_signal_connect(factory,"setup",G_CALLBACK(setup),context);
        g_signal_connect(factory,"bind",G_CALLBACK(bind),context);
        g_signal_connect(factory,"unbind",G_CALLBACK(unbind),context);
        if(column){
            gtk_column_view_column_set_factory(column,factory);g_object_unref(factory);
            g_object_set_data(G_OBJECT(column),"cui-editable",GINT_TO_POINTER(editable));
            g_object_unref(column);continue;
        }
        column=gtk_column_view_column_new(w->headers[i],factory);
        g_object_set_data(G_OBJECT(column),"cui-editable",GINT_TO_POINTER(editable));
        gtk_column_view_column_set_expand(column,TRUE);gtk_column_view_column_set_resizable(column,TRUE);
        g_object_set_data(G_OBJECT(column),"cui-column",GSIZE_TO_POINTER(i+1));
#if GTK_CHECK_VERSION(4,10,0)
        GtkCustomSorter *sorter=gtk_custom_sorter_new(unused_compare,NULL,NULL);
        gtk_column_view_column_set_sorter(column,GTK_SORTER(sorter));g_object_unref(sorter);
#endif
        gtk_column_view_append_column(view,column);g_object_unref(column);
    }
}
void cui__gtk_table_items(cui_widget *w)
{
    GtkColumnView *view=GTK_COLUMN_VIEW(w->aux);cui_table_state *s=w->payload;
    gtk_column_view_set_model(view,NULL);columns(w);
    GListStore *store=g_list_store_new(G_TYPE_OBJECT);
    for(size_t row=0;row<s->rows;++row){GObject *item=row_object(row);g_list_store_append(store,item);g_object_unref(item);}
    GtkSelectionModel *selection;
    if(s->multiple)selection=GTK_SELECTION_MODEL(gtk_multi_selection_new(G_LIST_MODEL(store)));
    else{
        GtkSingleSelection *single=gtk_single_selection_new(G_LIST_MODEL(store));
        gtk_single_selection_set_autoselect(single,FALSE);gtk_single_selection_set_can_unselect(single,TRUE);
        gtk_single_selection_set_selected(single,GTK_INVALID_LIST_POSITION);selection=GTK_SELECTION_MODEL(single);
    }
    gtk_column_view_set_model(view,selection);
    g_object_set_data(G_OBJECT(view),"cui-store",store);
    g_signal_connect(selection,"selection-changed",G_CALLBACK(selected),w);g_object_unref(selection);
#if GTK_CHECK_VERSION(4,10,0)
    GtkSorter *sorter=gtk_column_view_get_sorter(view);
    if(!g_object_get_data(G_OBJECT(view),"cui-sort-connected")){
        g_signal_connect(sorter,"changed",G_CALLBACK(sort_changed),w);g_object_set_data(G_OBJECT(view),"cui-sort-connected",GINT_TO_POINTER(1));
    }
    if(s->sort_column>=0){
        GtkColumnViewColumn *column=g_list_model_get_item(gtk_column_view_get_columns(view),(guint)s->sort_column);
        gtk_column_view_sort_by_column(view,column,s->descending?GTK_SORT_DESCENDING:GTK_SORT_ASCENDING);g_object_unref(column);
    }
#endif
}
void cui__backend_table_selection(cui_widget *w)
{
    cui_table_state *s=w->payload;GtkSelectionModel *model=gtk_column_view_get_model(GTK_COLUMN_VIEW(w->aux));
    gtk_selection_model_unselect_all(model);
    for(size_t i=0;i<s->rows;++i)if(s->selected[i])gtk_selection_model_select_item(model,(guint)i,!s->multiple);
}
static void update_cell(GtkWidget *widget,size_t row,size_t column,const char *text)
{
    if(GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(widget),"cui-row"))==row+1 &&
       GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(widget),"cui-column-index"))==column+1){
        if(GTK_IS_EDITABLE_LABEL(widget)){
            if(gtk_editable_label_get_editing(GTK_EDITABLE_LABEL(widget)))gtk_editable_label_stop_editing(GTK_EDITABLE_LABEL(widget),FALSE);
            gtk_editable_set_text(GTK_EDITABLE(widget),text);
        }else gtk_label_set_text(GTK_LABEL(widget),text);
        return;
    }
    for(GtkWidget *child=gtk_widget_get_first_child(widget);child;child=gtk_widget_get_next_sibling(child))update_cell(child,row,column,text);
}
void cui__backend_table_cell(cui_widget *w,size_t row,size_t column)
{ update_cell(GTK_WIDGET(w->aux),row,column,w->items[row*w->columns+column]); }

void cui__backend_table_reveal(cui_widget *w,size_t row)
{
#if GTK_CHECK_VERSION(4,12,0)
    gtk_column_view_scroll_to(GTK_COLUMN_VIEW(w->aux),(guint)row,NULL,GTK_LIST_SCROLL_NONE,NULL);
#else
    /* GTK 4.6 exposes this action on ColumnView's native ListView child. */
    for(GtkWidget *child=gtk_widget_get_first_child(w->aux);child;child=gtk_widget_get_next_sibling(child))
        if(GTK_IS_LIST_VIEW(child)){gtk_widget_activate_action(child,"list.scroll-to-item","u",(guint)row);break;}
#endif
}

void cui__backend_table_headers(cui_widget *w,int visible)
{
    for(GtkWidget *child=gtk_widget_get_first_child(w->aux);child;child=gtk_widget_get_next_sibling(child))
        if(!strcmp(gtk_widget_get_css_name(child),"header"))gtk_widget_set_visible(child,visible);
}
