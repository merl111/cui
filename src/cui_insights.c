#include "cui_patterns_internal.h"
#include "cui_desktop_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct insight_series { cui_item_id id; char *title,*detail; double *values; char **labels; size_t count; } insight_series;
struct cui_insight_model { insight_series *series; size_t count; };
void cui__insights_free(struct cui_insight_model *m)
{
    if(!m)return;
    for(size_t i=0;i<m->count;++i){
        insight_series *s=m->series+i;free(s->title);free(s->detail);free(s->values);cui__free_strings(s->labels,s->labels?s->count:0);
    }
    free(m->series);free(m);
}
static int copy_points(insight_series *out,const cui_insight_series *in)
{
    if(!in->count)return 1;
    out->values=malloc(in->count*sizeof(*out->values));if(!out->values)return 0;
    for(size_t i=0;i<in->count;++i){if(!isfinite(in->values[i]))return 0;out->values[i]=in->values[i];}
    if(!in->labels)return 1;
    out->labels=calloc(in->count,sizeof(*out->labels));if(!out->labels)return 0;
    for(size_t i=0;i<in->count;++i){
        if(!in->labels[i]||strlen(in->labels[i])>255)return 0;
        out->labels[i]=cui__desktop_copy(in->labels[i]);if(!out->labels[i])return 0;
    }
    return 1;
}
static int copy_series(insight_series *out,const cui_insight_series *in)
{
    if(!in->id||!in->title||!*in->title||strlen(in->title)>255||(in->count&&!in->values))return 0;
    const char *detail=in->detail?in->detail:"";if(strlen(detail)>65536)return 0;
    out->id=in->id;out->count=in->count;out->title=cui__desktop_copy(in->title);out->detail=cui__desktop_copy(detail);
    return out->title&&out->detail&&copy_points(out,in);
}
static int series_ids_unique(const cui_insight_series *items,size_t count)
{
    for(size_t i=0;i<count;++i)for(size_t j=0;j<i;++j)if(items[j].id==items[i].id)return 0;
    return 1;
}
static struct cui_insight_model *copy_model(const cui_insight_series *items,size_t count)
{
    if(count>256||(count&&!items)||!series_ids_unique(items,count))return NULL;
    struct cui_insight_model *m=calloc(1,sizeof(*m));if(!m)return NULL;
    m->series=count?calloc(count,sizeof(*m->series)):NULL;
    if(count&&!m->series){free(m);return NULL;}m->count=count;
    size_t total=0;
    for(size_t i=0;i<count;++i){
        if(items[i].count>65536-total||!copy_series(m->series+i,items+i)){cui__insights_free(m);return NULL;}
        total+=items[i].count;
    }
    return m;
}
void cui__insight_empty(pattern_state *s)
{
    cui_set_enabled(s->parts[CUI_PART_PRIMARY],0);cui_set_enabled(s->parts[CUI_PART_SECONDARY],0);
    cui_set_enabled(s->parts[CUI_PART_PROGRESS],0);cui_set_value(s->parts[CUI_PART_PROGRESS],0);
    cui_set_text(s->parts[CUI_PART_BODY],"No insight datasets.");cui_set_text(s->parts[CUI_PART_STATUS],"No data");
}
static int present(pattern_state *s,size_t page,size_t point)
{
    if(!s->insights||!s->insights->count){
        if(!cui_chart_set_values(s->parts[CUI_PART_CHART],NULL,0))return 0;
        cui__insight_empty(s);s->page=0;s->point=0;return 1;
    }
    insight_series *series=s->insights->series+page;
    if(!cui_chart_set_values(s->parts[CUI_PART_CHART],series->values,series->count))return 0;
    s->page=(int)page;s->point=point;
    cui_set_text(s->parts[CUI_PART_BODY],series->title);
    cui_set_tooltip(s->parts[CUI_PART_CHART],series->detail);
    cui_accessibility(s->parts[CUI_PART_CHART],series->title,series->detail);
    cui_set_enabled(s->parts[CUI_PART_PRIMARY],page+1<s->insights->count);
    cui_set_enabled(s->parts[CUI_PART_SECONDARY],page>0);
    cui_set_enabled(s->parts[CUI_PART_PROGRESS],series->count>1);
    cui_set_value(s->parts[CUI_PART_PROGRESS],series->count>1?(double)point/(series->count-1):0);
    char text[1024];
    if(series->count)snprintf(text,sizeof(text),"%s · %s · point %zu of %zu · %.8g",series->title,series->labels?series->labels[point]:"Value",point+1,series->count,series->values[point]);
    else snprintf(text,sizeof(text),"%s · No data points",series->title);
    cui_set_text(s->parts[CUI_PART_STATUS],text);return 1;
}
int cui_insights_set_series(cui_widget *w,const cui_insight_series *items,size_t count)
{
    pattern_state *s=cui__pattern_state(w);if(!s||s->kind!=CUI_INSIGHTS)return 0;
    struct cui_insight_model *copy=copy_model(items,count);if(!copy)return 0;
    struct cui_insight_model *previous=s->insights;
    cui_item_id selected=cui_insights_selection(w,NULL,NULL);size_t page=0,point=0;
    for(size_t i=0;i<count;++i)if(copy->series[i].id==selected){page=i;point=s->point;break;}
    size_t length=count?copy->series[page].count:0;if(point>=length)point=length?length-1:0;
    s->insights=copy;
    if(!present(s,page,point)){s->insights=previous;cui__insights_free(copy);return 0;}
    cui__insights_free(previous);return 1;
}
int cui_insights_select(cui_widget *w,cui_item_id id,size_t point)
{
    pattern_state *s=cui__pattern_state(w);if(!s||!s->insights)return 0;
    for(size_t i=0;i<s->insights->count;++i)if(s->insights->series[i].id==id){
        size_t count=s->insights->series[i].count;if(count?point>=count:point!=0)return 0;
        return present(s,i,point);
    }
    return 0;
}
cui_item_id cui_insights_selection(const cui_widget *w,size_t *point,double *value)
{
    pattern_state *s=cui__pattern_state(w);if(!s||!s->insights||!s->insights->count)return 0;
    insight_series *series=s->insights->series+s->page;
    if(point)*point=s->point;
    if(value)*value=series->count?series->values[s->point]:0;
    return series->id;
}
void cui__insight_action(cui_widget *sender,void *data)
{
    pattern_state *s=data;if(!s->insights||!s->insights->count)return;
    size_t page=(size_t)s->page,point;
    if(sender==s->parts[CUI_PART_PRIMARY]){if(page+1>=s->insights->count)return;++page;point=0;}
    else if(sender==s->parts[CUI_PART_SECONDARY]){if(!page)return;--page;point=0;}
    else{
        size_t count=s->insights->series[page].count;
        point=count?(size_t)(cui_get_value(sender)*(count-1)+0.5):0;
    }
    if(page==(size_t)s->page&&point==s->point)return;
    if(present(s,page,point))cui__pattern_emit(s,CUI_EVENT_CHANGE);
}
