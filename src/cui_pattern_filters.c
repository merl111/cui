#include "cui_patterns_internal.h"
#include "cui_desktop_internal.h"
#include "cui_tables.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct predicate { size_t column; cui_filter_op op; char *value; double number; } predicate;
struct cui_pattern_filters { predicate items[32]; size_t count; int all; };
static int record_pattern(pattern_state *s)
{return s&&s->kind>=CUI_CONTEXT&&s->kind<=CUI_SEARCH_PANEL&&s->kind!=CUI_SIDEBAR;}
static int numeric(const char *text,double *value)
{
    char *end;*value=strtod(text,&end);if(end==text)return 0;
    while(isspace((unsigned char)*end))++end;
    return !*end&&isfinite(*value);
}
void cui__pattern_filters_free(struct cui_pattern_filters *filters)
{
    if(!filters)return;
    for(size_t i=0;i<filters->count;++i)free(filters->items[i].value);
    free(filters);
}
static int copy_predicate(predicate *out,const cui_record_filter *in)
{
    if(in->column>2||in->operation<CUI_FILTER_CONTAINS||in->operation>CUI_FILTER_NOT_EMPTY)return 0;
    const char *value=in->value?in->value:"";if(strlen(value)>65536)return 0;
    out->column=in->column;out->op=in->operation;out->value=cui__search_key(value);
    if(!out->value)return 0;
    if(out->op>=CUI_FILTER_LESS&&out->op<=CUI_FILTER_GREATER_EQUAL)return numeric(value,&out->number);
    return 1;
}
int cui_pattern_set_filters(cui_widget *w,const cui_record_filter *items,size_t count,int all)
{
    pattern_state *s=cui__pattern_state(w);
    if(!record_pattern(s)||count>32||(count&&!items))return 0;
    struct cui_pattern_filters *copy=calloc(1,sizeof(*copy));if(!copy)return 0;
    copy->count=count;copy->all=!!all;
    for(size_t i=0;i<count;++i)if(!copy_predicate(copy->items+i,items+i)){cui__pattern_filters_free(copy);return 0;}
    struct cui_pattern_filters *previous=s->filters;s->filters=copy;
    if(!cui__pattern_refresh_records(s)){s->filters=previous;cui__pattern_filters_free(copy);return 0;}
    cui__pattern_filters_free(previous);return 1;
}
static int numeric_match(const predicate *p,const char *cell)
{
    double n;if(!numeric(cell,&n))return 0;
    switch(p->op){
    case CUI_FILTER_LESS:return n<p->number;
    case CUI_FILTER_LESS_EQUAL:return n<=p->number;
    case CUI_FILTER_GREATER:return n>p->number;
    default:return n>=p->number;
    }
}
static int text_match(const predicate *p,const char *key)
{
    size_t a=strlen(key),b=strlen(p->value);
    switch(p->op){
    case CUI_FILTER_CONTAINS:return strstr(key,p->value)!=NULL;
    case CUI_FILTER_EQUALS:return !strcmp(key,p->value);
    case CUI_FILTER_NOT_EQUALS:return strcmp(key,p->value)!=0;
    case CUI_FILTER_STARTS_WITH:return a>=b&&!strncmp(key,p->value,b);
    default:return a>=b&&!strcmp(key+a-b,p->value);
    }
}
static int matches(const predicate *p,const char *cell)
{
    if(p->op==CUI_FILTER_EMPTY)return !*cell;
    if(p->op==CUI_FILTER_NOT_EMPTY)return !!*cell;
    if(p->op>=CUI_FILTER_LESS&&p->op<=CUI_FILTER_GREATER_EQUAL)return numeric_match(p,cell);
    char *key=cui__search_key(cell);if(!key)return -1;
    int result=text_match(p,key);free(key);return result;
}
int cui__pattern_matches(pattern_state *s,const char *const *row)
{
    struct cui_pattern_filters *f=s->filters;if(!f||!f->count)return 1;
    for(size_t i=0;i<f->count;++i){
        int result=matches(f->items+i,row[f->items[i].column]);if(result<0)return -1;
        if(result!=f->all)return result;
    }
    return f->all;
}
int cui_pattern_set_query(cui_widget *w,const char *query)
{
    pattern_state *s=cui__pattern_state(w);if(!s||!query||strlen(query)>65536)return 0;
    if(s->kind<CUI_CONTEXT||s->kind>CUI_SEARCH_PANEL)return 0;
    size_t size=cui_get_text(s->parts[CUI_PART_INPUT],NULL,0)+1;
    char *previous=malloc(size);if(!previous)return 0;
    cui_get_text(s->parts[CUI_PART_INPUT],previous,size);cui_set_text(s->parts[CUI_PART_INPUT],query);
    int result=cui__pattern_refresh_records(s);if(!result)cui_set_text(s->parts[CUI_PART_INPUT],previous);
    free(previous);return result;
}
size_t cui_pattern_record_source(const cui_widget *w,size_t displayed)
{
    pattern_state *s=cui__pattern_state(w);if(!record_pattern(s)||cui__collection_active(s)||!s->visible_rows)return (size_t)-1;
    cui_widget *body=s->parts[CUI_PART_BODY];
    size_t row=body->kind==CUI_TABLE?cui_table_source_row(body,displayed):displayed;
    size_t count=body->kind==CUI_TABLE?body->item_count/body->columns:body->item_count;
    return row<count?s->visible_rows[row]:(size_t)-1;
}
