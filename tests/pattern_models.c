#include "cui_internal.h"
#include "cui_patterns.h"
#include "cui_tables.h"
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"pattern_models:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int calls;
static cui_item_id last_id;
static void action(cui_widget *w,void *data)
{
    (void)data;++calls;last_id=cui_pattern_item_event_id(w);
    if(cui_pattern_event(w)==CUI_EVENT_CANCEL)CHECK(cui_pattern_remove_item(w,last_id));
}
static void insights(cui_widget *root)
{
    cui_widget *w=cui_pattern_create(root,CUI_INSIGHTS,NULL);CHECK(w);cui_on_action(w,action,NULL);
    CHECK(!cui_activate(cui_pattern_part(w,CUI_PART_PRIMARY)));
    double points[]={2,4,8};const char *labels[]={"Mon","Tue","Wed"};
    cui_insight_series series[]={{20,"Usage","Requests",points,labels,3},{40,"Empty",NULL,NULL,NULL,0}};
    CHECK(cui_insights_set_series(w,series,2));points[2]=99;
    CHECK(cui_insights_select(w,20,2));size_t point=99;double value=0;
    CHECK(cui_insights_selection(w,&point,&value)==20&&point==2&&value==8);
    CHECK(!cui_insights_select(w,20,3));CHECK(!cui_insights_select(w,999,0));
    CHECK(cui_activate(cui_pattern_part(w,CUI_PART_PRIMARY)));
    CHECK(cui_insights_selection(w,&point,&value)==40&&point==0&&value==0);
    CHECK(!cui_activate(cui_pattern_part(w,CUI_PART_PRIMARY)));
    CHECK(cui_activate(cui_pattern_part(w,CUI_PART_SECONDARY)));
    int before=calls;gtk_range_set_value(GTK_RANGE(cui_pattern_part(w,CUI_PART_PROGRESS)->native),1);
    CHECK(calls==before+1);CHECK(cui_insights_selection(w,&point,&value)==20&&value==8);
    cui_insight_series swapped[]={series[1],series[0]};swapped[1].count=1;
    CHECK(cui_insights_set_series(w,swapped,2));CHECK(cui_insights_selection(w,&point,&value)==20&&point==0&&value==2);
    series[1].id=20;CHECK(!cui_insights_set_series(w,series,2));
    points[0]=NAN;CHECK(!cui_insights_set_series(w,series,1));CHECK(cui_insights_selection(w,NULL,NULL)==20);
    CHECK(cui_insights_set_series(w,NULL,0));point=42;value=17;
    CHECK(!cui_insights_selection(w,&point,&value)&&point==42&&value==17);
}
static void filters(cui_widget *root)
{
    cui_widget *w=cui_pattern_create(root,CUI_FILTER_TABLE,NULL),*body=cui_pattern_part(w,CUI_PART_BODY);
    const char *cells[]={"Éclair","Active","10","Beta","Draft","20","Gamma","Active",""};
    CHECK(cui_pattern_set_records(w,cells,3));cui_on_action(w,action,NULL);int before=calls;
    cui_record_filter f={0,CUI_FILTER_EQUALS,"éCLAIR"};CHECK(cui_pattern_set_filters(w,&f,1,1));CHECK(body->item_count==3);
    CHECK(cui_pattern_record_source(w,0)==0&&cui_pattern_record_source(w,1)==(size_t)-1);
    const cui_filter_op ops[]={CUI_FILTER_CONTAINS,CUI_FILTER_NOT_EQUALS,CUI_FILTER_STARTS_WITH,CUI_FILTER_ENDS_WITH,CUI_FILTER_LESS,CUI_FILTER_LESS_EQUAL,CUI_FILTER_GREATER,CUI_FILTER_GREATER_EQUAL,CUI_FILTER_EMPTY,CUI_FILTER_NOT_EMPTY};
    const char *values[]={"a","Beta","g","MA","20","20","10","20","",""};
    const size_t columns[]={0,0,0,0,2,2,2,2,2,2}, counts[]={3,2,1,1,1,2,1,1,1,2};
    for(size_t i=0;i<10;++i){f=(cui_record_filter){columns[i],ops[i],values[i]};CHECK(cui_pattern_set_filters(w,&f,1,1));CHECK(body->item_count==counts[i]*3);}
    cui_record_filter pair[]={{1,CUI_FILTER_EQUALS,"Active"},{2,CUI_FILTER_GREATER,"15"}};
    CHECK(cui_pattern_set_filters(w,pair,2,1));CHECK(body->item_count==0);
    CHECK(cui_pattern_set_filters(w,pair,2,0));CHECK(body->item_count==9);
    CHECK(cui_pattern_set_query(w,"beta"));CHECK(body->item_count==3&&cui_pattern_record_source(w,0)==1);
    f=(cui_record_filter){2,CUI_FILTER_GREATER,"invalid"};CHECK(!cui_pattern_set_filters(w,&f,1,1));CHECK(body->item_count==3);
    CHECK(cui_pattern_set_query(w,""));CHECK(cui_pattern_set_filters(w,NULL,0,1));
    CHECK(cui_table_sort(body,0,1,0));CHECK(cui_pattern_record_source(w,1)==2);CHECK(calls==before);
}
static size_t children(cui_widget *w){size_t n=0;for(cui_widget *c=w->first;c;c=c->next)++n;return n;}
static void collections(cui_widget *root)
{
    const cui_pattern kinds[]={CUI_TOOL_CHIPS,CUI_TASK_ROWS,CUI_CHAT,CUI_CONTEXT};
    for(size_t k=0;k<4;++k){
        cui_widget *w=cui_pattern_create(root,kinds[k],NULL);CHECK(w);cui_on_action(w,action,NULL);
        char title[]="First";
        cui_pattern_item items[]={{7,title,"Body","Details","Ready",.25,CUI_ROLE_SUCCESS,CUI_ITEM_EXPANDED},{9,"Second","Résumé","20","",0,CUI_ROLE_SUBTLE,0}};
        int before=calls;CHECK(cui_pattern_set_items(w,items,2));title[0]='X';cui_pattern_item item;
        CHECK(cui_pattern_item_count(w)==2&&cui_pattern_item_at(w,0,&item)&&!strcmp(item.title,"First"));
        CHECK(calls==before);
        cui_widget *disclosure=cui_pattern_item_part(w,7,CUI_PART_DETAILS);
        if(disclosure){
            CHECK(cui_get_expanded(disclosure));CHECK(cui_activate(disclosure->first));
            CHECK(cui_pattern_item_at(w,0,&item)&&!(item.flags&CUI_ITEM_EXPANDED));
            CHECK(last_id==7);before=calls;
        }
        cui_widget *body=cui_pattern_part(w,CUI_PART_BODY);size_t slots=children(body);
        cui_widget *primary=cui_pattern_item_part(w,7,CUI_PART_PRIMARY);
        if(kinds[k]==CUI_TASK_ROWS||kinds[k]==CUI_CONTEXT)gtk_check_button_set_active(GTK_CHECK_BUTTON(primary->native),TRUE);
        else CHECK(cui_activate(primary));
        CHECK(last_id==7&&calls==before+1);CHECK(cui_pattern_item_at(w,0,&item));
        if(kinds[k]==CUI_TASK_ROWS)CHECK((item.flags&CUI_ITEM_COMPLETE)&&item.progress==1);
        if(kinds[k]==CUI_CONTEXT){
            CHECK(item.flags&CUI_ITEM_SELECTED);CHECK(cui_pattern_set_query(w,"RÉSUMÉ"));
            CHECK(cui_pattern_item_part(w,7,CUI_PART_PREVIEW)->hidden&&!cui_pattern_item_part(w,9,CUI_PART_PREVIEW)->hidden);
            cui_record_filter f={2,CUI_FILTER_GREATER,"25"};CHECK(cui_pattern_set_filters(w,&f,1,1));CHECK(cui_pattern_item_part(w,9,CUI_PART_PREVIEW)->hidden);
            CHECK(cui_pattern_set_filters(w,NULL,0,1));CHECK(cui_pattern_set_query(w,""));
            CHECK(!cui_pattern_set_records(w,NULL,0));CHECK(cui_pattern_record_source(w,0)==(size_t)-1);
        }
        cui_pattern_item invalid[]={items[0],items[0]};CHECK(!cui_pattern_set_items(w,invalid,2));CHECK(cui_pattern_item_count(w)==2);
        items[0].progress=NAN;CHECK(!cui_pattern_upsert_item(w,items));
        CHECK(cui_activate(cui_pattern_item_part(w,7,CUI_PART_SECONDARY)));CHECK(last_id==7&&cui_pattern_item_count(w)==1);
        CHECK(!cui_pattern_item_part(w,7,CUI_PART_PRIMARY));CHECK(cui_pattern_item_at(w,0,&item)&&item.id==9);
        item.flags|=CUI_ITEM_DISABLED;CHECK(cui_pattern_upsert_item(w,&item));CHECK(!cui_activate(cui_pattern_item_part(w,9,CUI_PART_SECONDARY)));
        CHECK(cui_pattern_item_at(w,0,&item));item.flags=0;item.id=11;CHECK(cui_pattern_upsert_item(w,&item));CHECK(children(body)==slots);
        CHECK(cui_pattern_set_items(w,NULL,0)&&cui_pattern_item_count(w)==0);CHECK(!cui_pattern_remove_item(w,999));
    }
}
int main(void)
{
    cui_app *app=cui_app_create();CHECK(app);if(getenv("CUI_LARGE_TEXT"))CHECK(cui_app_set_text_scale(app,1.5));
    cui_window *window=cui_window_create(app,"Pattern model tests",900,700);CHECK(window);
    cui_widget *root=cui_window_root(window);insights(root);filters(root);collections(root);
    cui_app_destroy(app);puts("pattern_models: copied models, filtering, IDs, native actions and reentrant removal passed");return 0;
}
