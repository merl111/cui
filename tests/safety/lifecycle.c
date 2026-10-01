#include "allocations.h"
#include "cui_internal.h"
#include <gtk/gtk.h>
#include "cui_desktop.h"
#include "cui_navigation.h"
#include "cui_patterns.h"
#include "cui_search.h"
#include "cui_tokens.h"
#include "cui_tables.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const cui_icon_command path[]={
    {CUI_ICON_MOVE,{0,0},0,0},{CUI_ICON_LINE,{24,24},0,0},
    {CUI_ICON_STROKE,{2,1,1},0,1}};
static const cui_tree_item nodes[]={{1,0,"Root",1},{2,1,"Child",0}};
static const cui_choice choices[]={{1,"Orbit","Design","native",0},{2,"Harbor","Desktop","rust",0}};
static void action(cui_widget *w,void *data){(void)w; ++*(int *)data;}
static void quit(void *data){cui_app_quit(data);}
static void no_action(void *data){(void)data;}
static void clean(void)
{
    if(cui_test_live_allocations()) {
        fprintf(stderr,"CUI retained %zu allocations (%zu bytes) after teardown\n",cui_test_live_allocations(),cui_test_live_bytes());
        abort();
    }
}
/* Tolerates a failed construction at any allocation; teardown must still work. */
static void construction(void)
{
    cui_app *app=cui_app_create();if(!app)return;
    cui_window *window=cui_window_create(app,"Allocation failure",480,360);
    cui_widget *root=cui_window_root(window);
    cui_icon_asset *icon=cui_icon_vector(24,24,path,3);
    cui_widget *button=cui_icon_button(root,icon,"Play");
    cui_set_icon(button,NULL);cui_icon_release(icon);
    const char *columns[]={"Name","Status"},*cells[]={"Orbit","Active","Harbor","Ready"};
    cui_widget *table=cui_table(root,columns,2);
    if(table){cui_table_set_rows(table,cells,2);cui_table_sort(table,0,0,0);}
    cui_tree(root,nodes,2);
    cui_widget *picker=cui_picker(root,CUI_AUTOCOMPLETE,"Project");
    cui_picker_set_items(picker,choices,2);
    cui_widget *tokens=cui_tokens(root,"Labels",4);cui_tokens_set_items(tokens,choices,2);
    const char *extensions[]={"txt","md"};
    const cui_file_filter filter={"Documents",extensions,2};
    const cui_file_options options={"",&filter,1,0,1};
    cui_dialog *dialog=cui_file_dialog_ex(window,CUI_DIALOG_OPEN,"Open",&options,NULL,NULL);
    cui_dialog_cancel(dialog);
    cui_command *command=cui_command_create(app,"Save",0,0,no_action,NULL);
    cui_menu *menu=cui_menu_create(app);cui_menu_add(menu,command);
    cui_timer *timer=cui_every(app,10,no_action,NULL);cui_timer_stop(timer);
    cui_app_destroy(app);
}
static void failure_sweep(void)
{
    cui_test_fail_allocation(0);construction();
    size_t count=cui_test_allocation_attempts();clean();
    for(size_t i=1;i<=count;++i){
        cui_test_fail_allocation(i);construction();
        assert(cui_test_allocation_failed());clean();
    }
    cui_test_fail_allocation(0);
    printf("Construction/cleanup: %zu injected allocation failures\n",count);
}
static void finalized(void *data,GObject *object){(void)object; *(int *)data=1;}
static void check_tree_release(cui_widget *tree)
{
    GListModel *model=G_LIST_MODEL(gtk_list_view_get_model(GTK_LIST_VIEW(tree->aux)));
    GtkTreeListRow *row=g_list_model_get_item(model,0);assert(row);
    GObject *node=gtk_tree_list_row_get_item(row);assert(node);int released=0;
    g_object_weak_ref(node,finalized,&released);g_object_unref(node);g_object_unref(row);
    assert(cui_tree_set_items(tree,nodes,2));assert(released);
}
static void replace_models(cui_widget *table,cui_widget *tree,cui_widget *picker)
{
    const char *old[]={"Original","Stable"},*next[]={"New","Value","Second","Value"};
    assert(cui_table_set_rows(table,old,1));
    cui_test_fail_allocation(0);assert(cui_table_set_rows(table,next,2));
    size_t count=cui_test_allocation_attempts();
    for(size_t i=1;i<=count;++i){
        assert(cui_table_set_rows(table,old,1));
        cui_test_fail_allocation(i);int okay=cui_table_set_rows(table,next,2);
        cui_test_fail_allocation(0);
        if(!okay){char value[32];cui_table_get_cell(table,0,0,value,sizeof(value));assert(!strcmp(value,"Original"));}
    }
    assert(cui_tree_set_items(tree,nodes,2));assert(cui_picker_set_items(picker,choices,2));
    check_tree_release(tree);
    size_t baseline=cui_test_live_allocations(),baseline_bytes=cui_test_live_bytes();
    for(int i=0;i<300;++i){
        assert(cui_tree_set_items(tree,nodes,2));assert(cui_picker_set_items(picker,choices,2));
        assert(cui_test_live_allocations()==baseline && cui_test_live_bytes()==baseline_bytes);
    }
    printf("Model replacement: %zu injected table failures; 300 stable tree/picker replacements\n",count);
}
static void lifecycle(int iterations)
{
    for(int i=0;i<iterations;++i){
        cui_app *app=cui_app_create();assert(app);
        cui_window *window=cui_window_create(app,"Lifecycle stress",600,480);assert(window);
        cui_window_set_scrollable(window,1);cui_widget *root=cui_window_root(window);
        const char *columns[]={"Name","Status"};cui_widget *table=cui_table(root,columns,2);assert(table);
        cui_widget *tree=cui_tree(root,nodes,2),*picker=cui_picker(root,CUI_AUTOCOMPLETE,"Find");assert(tree&&picker);
        if(!i)replace_models(table,tree,picker);
        for(int kind=0;kind<CUI_PATTERN_COUNT;++kind)assert(cui_pattern_create(root,(cui_pattern)kind,"Stress"));
        cui_widget *button=cui_button(root,"Run");assert(button);int clicks=0;
        for(int n=0;n<100;++n){
            cui_icon_asset *icon=cui_icon_vector(24,24,path,3);assert(icon);
            assert(cui_set_icon(button,icon));cui_icon_release(icon);
            cui_on_action(button,action,&clicks);assert(cui_activate(button));
        }
        assert(clicks==100);cui_on_action(button,NULL,NULL);assert(cui_set_icon(button,NULL));
        cui_timer *timer=cui_every(app,10,quit,app);assert(timer);cui_timer_stop(timer);assert(cui_timer_start(timer));
        cui_window_show(window);cui_app_run(app);cui_app_destroy(app);clean();
    }
    printf("Lifecycle: %d catalog construction/teardown cycles; zero live CUI allocations\n",iterations);
}
int main(int argc,char **argv)
{
    int iterations=argc>1?atoi(argv[1]):12;assert(iterations>0&&iterations<=1000);
    failure_sweep();lifecycle(iterations);return 0;
}
