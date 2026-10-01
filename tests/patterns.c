#include "cui_internal.h"
#include "cui_patterns.h"
#include "cui_tables_internal.h"
#include "cui_navigation_internal.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "patterns:%d: %s\n", __LINE__, #x); exit(1); } } while (0)
static cui_app *app;
static cui_timer *timer;
static cui_widget *patterns[CUI_PATTERN_COUNT];
static int calls;
static void action(cui_widget *sender, void *data) { (void)data; CHECK(cui_pattern_event(sender) != CUI_EVENT_NONE); ++calls; }
static void exercise(void *data)
{
    (void)data; cui_timer_stop(timer);
    for (int i = 0; i < CUI_PATTERN_COUNT; ++i) {
        cui_widget *primary = cui_pattern_part(patterns[i], CUI_PART_PRIMARY);
        if (primary) CHECK(cui_activate(primary));
        cui_pattern_set_busy(patterns[i], 1); cui_pattern_set_busy(patterns[i], 0);
    }
    CHECK(calls >= 13);
    cui_widget *flow = cui_pattern_part(patterns[CUI_FLOWCHART], CUI_PART_BODY);
    CHECK(flow->first && flow->first->next);
    cui_widget *inspector = patterns[CUI_FINE_TUNE];
    cui_widget *width = cui_pattern_part(inspector, CUI_PART_CHOICE);
    gtk_range_set_value(GTK_RANGE(width->native), 0.75);
    CHECK(cui_pattern_part(inspector, CUI_PART_PREVIEW)->min_width == 315);
    CHECK(cui_get_value(cui_pattern_part(patterns[CUI_TASK_ROWS], CUI_PART_PROGRESS)) == 1);
    CHECK(cui_stream_append(patterns[CUI_STREAMING], "Hello ") && cui_stream_append(patterns[CUI_STREAMING], "世界"));
    char text[128]; cui_get_text(cui_pattern_part(patterns[CUI_STREAMING], CUI_PART_BODY), text, sizeof(text)); CHECK(!strcmp(text,"Hello 世界"));
    cui_widget *selection = cui_pattern_part(patterns[CUI_SELECTION_ACTIONS], CUI_PART_BODY);
    cui_set_text(selection, "Hello 世界");
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(selection->aux)); GtkTextIter a,z;
    gtk_text_buffer_get_iter_at_offset(buffer,&a,6); gtk_text_buffer_get_end_iter(buffer,&z); gtk_text_buffer_select_range(buffer,&a,&z);
    cui_get_selected_text(selection,text,sizeof(text)); CHECK(!strcmp(text,"世界"));
    const char *records[] = {"Bravo","Draft","B", "Alpha","Active","A"};
    CHECK(cui_pattern_set_records(patterns[CUI_SEARCH_PANEL],records,2));
    cui_widget *search = cui_pattern_part(patterns[CUI_SEARCH_PANEL],CUI_PART_INPUT);
    gtk_editable_set_text(GTK_EDITABLE(search->native),"alpha");
    CHECK(cui_pattern_part(patterns[CUI_SEARCH_PANEL],CUI_PART_BODY)->item_count==1);
    gtk_editable_set_text(GTK_EDITABLE(search->native),"missing");
    CHECK(cui_pattern_part(patterns[CUI_SEARCH_PANEL],CUI_PART_BODY)->item_count==0);
    const char *duplicates[]={"Same","Draft","first","Same","Active","second"};
    cui_widget *record_pattern=patterns[CUI_RECORDS_TABLE];
    CHECK(cui_pattern_set_records(record_pattern,duplicates,2));
    cui_widget *record_table=cui_pattern_part(record_pattern,CUI_PART_BODY);
    CHECK(cui_table_set_editable(record_table,2,1));
    cui__table_sort(record_table,1,0);
    cui__table_edit(record_table,0,2,"Edited second");
    cui_widget *record_search=cui_pattern_part(record_pattern,CUI_PART_INPUT);
    gtk_editable_set_text(GTK_EDITABLE(record_search->native),"Draft");
    cui_table_get_cell(record_table,0,2,text,sizeof(text));CHECK(!strcmp(text,"first"));
    gtk_editable_set_text(GTK_EDITABLE(record_search->native),"Active");
    cui_table_get_cell(record_table,0,2,text,sizeof(text));CHECK(!strcmp(text,"Edited second"));
    gtk_editable_set_text(GTK_EDITABLE(record_search->native),"");
    cui_table_get_cell(record_table,0,1,text,sizeof(text));CHECK(!strcmp(text,"Active"));
    cui_widget *sidebar=patterns[CUI_SIDEBAR],*tree=cui_pattern_part(sidebar,CUI_PART_BODY);
    const cui_tree_item nodes[]={{10,0,"Workspace",0},{20,10,"Sources",0},{30,20,"東京.c",0},{40,10,"Notes",0}};
    const char *details[]={"Root location","Source files","Unicode document","Release plan"};
    CHECK(cui_sidebar_set_items(sidebar,nodes,details,4));CHECK(tree->kind==CUI_TREE);
    cui_widget *sidebar_search=cui_pattern_part(sidebar,CUI_PART_INPUT);
    gtk_editable_set_text(GTK_EDITABLE(sidebar_search->native),"Unicode");
    CHECK(((cui_tree_model *)tree->payload)->count==3);
    CHECK(cui_tree_is_expanded(tree,10)&&cui_tree_is_expanded(tree,20));
    GtkSelectionModel *model=gtk_list_view_get_model(GTK_LIST_VIEW(tree->aux));
    gtk_single_selection_set_selected(GTK_SINGLE_SELECTION(model),2);
    CHECK(cui_tree_selected(tree)==30 && cui_pattern_event(sidebar)==CUI_EVENT_SELECT);
    cui_get_text(cui_pattern_part(sidebar,CUI_PART_DETAILS),text,sizeof(text));CHECK(!strcmp(text,"Unicode document"));
    gtk_editable_set_text(GTK_EDITABLE(sidebar_search->native),"");
    CHECK(!cui_tree_is_expanded(tree,10)&&!cui_tree_is_expanded(tree,20));
    CHECK(((cui_tree_model *)tree->payload)->count==4);
    CHECK(!cui_sidebar_set_items(sidebar,(cui_tree_item[]){{1,0,"Duplicate",0},{1,0,"Duplicate",0}},NULL,2));
    CHECK(((cui_tree_model *)tree->payload)->count==4);
    gtk_editable_set_text(GTK_EDITABLE(sidebar_search->native),"missing");
    CHECK(((cui_tree_model *)tree->payload)->count==0);
    gtk_editable_set_text(GTK_EDITABLE(sidebar_search->native),"");
    CHECK(cui_tree_select(tree,30));
    g_signal_emit_by_name(tree->aux,"activate",2);CHECK(cui_pattern_event(sidebar)==CUI_EVENT_OPEN);
    CHECK(cui_pattern_set_records(sidebar,records,2));CHECK(((cui_tree_model *)tree->payload)->count==2);
    cui_app_quit(app);
}
int main(void)
{
    app=cui_app_create(); CHECK(app);
    cui_window *window=cui_window_create(app,"Pattern integration",760,700); CHECK(window);
    cui_window_set_scrollable(window,1);
    for(int i=0;i<CUI_PATTERN_COUNT;++i) {
        patterns[i]=cui_pattern_create(cui_window_root(window),(cui_pattern)i,NULL); CHECK(patterns[i]);
        cui_on_action(patterns[i],action,NULL);
    }
    const double values[]={1,2,3};
    const cui_insight_series series[]={{1,"First","",values,NULL,3},{2,"Second","",values,NULL,3}};
    CHECK(cui_insights_set_series(patterns[CUI_INSIGHTS],series,2));
    CHECK(!cui_pattern_create(cui_window_root(window),CUI_PATTERN_COUNT,NULL));
    timer=cui_every(app,100,exercise,NULL); CHECK(timer);
    cui_window_show(window); cui_app_run(app); cui_app_destroy(app);
    puts("patterns: all 21 families, actions, timers, UTF-8 selections and data interactions passed");
    return 0;
}
