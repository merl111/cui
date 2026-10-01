#include "cui_tables_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

static cui_table_state *state(const cui_widget *w)
{ return w && w->kind == CUI_TABLE ? w->payload : NULL; }
static void free_table(void *data)
{ cui_table_state *s=data; free(s->selected); free(s->source_rows); free(s); }
int cui__table_init(cui_widget *w)
{
    cui_table_state *s=calloc(1,sizeof(*s));
    if(!s)return 0;
    s->sort_column=s->event_row=s->event_column=-1;
    w->payload=s;w->destroy_payload=free_table;return 1;
}
int cui__table_resize(cui_widget *w,size_t rows)
{
    cui_table_state *s=state(w);
    unsigned char *selected=rows?calloc(rows,1):NULL;
    size_t *source=rows?malloc(rows*sizeof(*source)):NULL;
    if(rows&&(!selected||!source)){free(selected);free(source);return 0;}
    for(size_t i=0;i<rows;++i)source[i]=i;
    free(s->source_rows);s->source_rows=source;
    free(s->selected);s->selected=selected;s->rows=rows;s->sort_column=-1;
    return 1;
}
static void clear_selection(cui_widget *w)
{ cui_table_state *s=state(w);if(s->rows)memset(s->selected,0,s->rows);w->selected=-1; }
static void first_selected(cui_widget *w)
{
    cui_table_state *s=state(w);w->selected=-1;
    for(size_t i=0;i<s->rows;++i)if(s->selected[i]){w->selected=(int)i;break;}
}
void cui__table_select(cui_widget *w,int row)
{
    cui_table_state *s=state(w);
    if(row < -1 || (row>=0 && (size_t)row>=s->rows))return;
    clear_selection(w);
    if(row>=0)s->selected[row]=1;
    w->selected=row;
    ++w->updating;cui__backend_table_selection(w);--w->updating;
}
void cui_table_set_multiple(cui_widget *w,int multiple)
{
    cui_table_state *s=state(w);if(!s)return;
    multiple=!!multiple;if(s->multiple==multiple)return;
    s->multiple=multiple;
    if(!multiple){int selected=w->selected;clear_selection(w);if(selected>=0)s->selected[selected]=1;w->selected=selected;}
    ++w->updating;cui__backend_items(w);cui__backend_table_selection(w);--w->updating;
}
int cui_table_select_row(cui_widget *w,size_t row,int selected)
{
    cui_table_state *s=state(w);if(!s||row>=s->rows)return 0;
    if(!s->multiple&&selected)clear_selection(w);
    s->selected[row]=!!selected;first_selected(w);
    ++w->updating;cui__backend_table_selection(w);--w->updating;return 1;
}
size_t cui_table_selected_rows(const cui_widget *w,size_t *rows,size_t capacity)
{
    cui_table_state *s=state(w);size_t count=0;if(!s)return 0;
    for(size_t i=0;i<s->rows;++i)if(s->selected[i]){if(rows&&count<capacity)rows[count]=i;++count;}
    return count;
}
int cui_table_set_editable(cui_widget *w,size_t column,int editable)
{
    cui_table_state *s=state(w);if(!s||column>=w->columns)return 0;
    if(s->editable[column]==!!editable)return 1;
    s->editable[column]=!!editable;
    ++w->updating;cui__backend_items(w);cui__backend_table_selection(w);--w->updating;return 1;
}
int cui_table_set_cell(cui_widget *w,size_t row,size_t column,const char *text)
{
    cui_table_state *s=state(w);if(!s||row>=s->rows||column>=w->columns)return 0;
    if(!text)text="";
    char *copy=malloc(strlen(text)+1);if(!copy)return 0;strcpy(copy,text);
    size_t index=row*w->columns+column;
    free(w->items[index]);w->items[index]=copy;
    ++w->updating;cui__backend_table_cell(w,row,column);--w->updating;return 1;
}
size_t cui_table_get_cell(const cui_widget *w,size_t row,size_t column,char *buffer,size_t capacity)
{
    cui_table_state *s=state(w);
    return cui__copy_text(s&&row<s->rows&&column<w->columns?w->items[row*w->columns+column]:"",buffer,capacity);
}
static int number(const char *text,double *value)
{
    char *end;*value=strtod(text,&end);if(end==text)return 0;
    while(isspace((unsigned char)*end))++end;
    return !*end&&isfinite(*value);
}
static int compare(const char *a,const char *b,int descending,int numeric)
{
    int result;
    if(numeric){
        double x,y;int nx=number(a,&x),ny=number(b,&y);
        if(nx!=ny)return nx?-1:1;
        result=nx?(x>y)-(x<y):0;
    }else result=strcmp(a,b);
    return descending?(result>0?-1:result<0?1:0):result;
}
typedef struct table_sort_context {
    cui_widget *widget; char **target; size_t *source;
    size_t column; int descending,numeric;
} table_sort_context;
static void merge_rows(table_sort_context *sort,size_t start,size_t middle,size_t end)
{
    cui_widget *w=sort->widget;cui_table_state *s=state(w);
    size_t left=start,right=middle;
    for(size_t out=start;out<end;++out){
        int take_left=right==end || (left<middle && compare(w->items[left*w->columns+sort->column],w->items[right*w->columns+sort->column],sort->descending,sort->numeric)<=0);
        size_t row=take_left?left++:right++;
        memcpy(sort->target+out*w->columns,w->items+row*w->columns,w->columns*sizeof(*sort->target));
        sort->source[out]=s->source_rows[row];
    }
}
size_t cui_table_source_row(const cui_widget *w,size_t row)
{ cui_table_state *s=state(w);return s&&row<s->rows?s->source_rows[row]:(size_t)-1; }
int cui_table_sort(cui_widget *w,size_t column,int descending,int numeric)
{
    cui_table_state *s=state(w);if(!s||column>=w->columns)return 0;
    table_sort_context sort={w,NULL,NULL,column,descending,numeric};
    sort.target=w->item_count?malloc(w->item_count*sizeof(*sort.target)):NULL;
    sort.source=s->rows?malloc(s->rows*sizeof(*sort.source)):NULL;
    if(s->rows&&(!sort.target||!sort.source)){free(sort.target);free(sort.source);return 0;}
    for(size_t width=1;width<s->rows;width*=2){
        for(size_t start=0;start<s->rows;start+=2*width){
            size_t middle=start+width<s->rows?start+width:s->rows;
            size_t end=start+2*width<s->rows?start+2*width:s->rows;
            merge_rows(&sort,start,middle,end);
        }
        char **swap=w->items;w->items=sort.target;sort.target=swap;
        size_t *ids=s->source_rows;s->source_rows=sort.source;sort.source=ids;
    }
    free(sort.target);free(sort.source);
    s->sort_column=(int)column;s->descending=!!descending;s->numeric=!!numeric;clear_selection(w);
    ++w->updating;cui__backend_items(w);cui__backend_table_selection(w);--w->updating;return 1;
}
static void emit(cui_widget *w,cui_table_event event,int row,int column)
{
    cui_table_state *s=state(w);s->event=event;s->event_row=row;s->event_column=column;cui__emit(w);
}
void cui__table_selection_changed(cui_widget *w)
{ if(!w->updating){first_selected(w);emit(w,CUI_TABLE_SELECTION,-1,-1);} }
void cui__table_edit(cui_widget *w,size_t row,size_t column,const char *text)
{
    cui_table_state *s=state(w);
    if(!s||w->updating||w->window->app->destroying||row>=s->rows||column>=w->columns||!s->editable[column])return;
    if(strcmp(w->items[row*w->columns+column],text)&&cui_table_set_cell(w,row,column,text))emit(w,CUI_TABLE_EDIT,(int)row,(int)column);
}
void cui__table_sort(cui_widget *w,size_t column,int descending)
{ if(!w->updating&&cui_table_sort(w,column,descending,0))emit(w,CUI_TABLE_SORT,-1,(int)column); }
cui_table_event cui_table_last_event(const cui_widget *w,int *row,int *column)
{
    cui_table_state *s=state(w);
    if(row)*row=s?s->event_row:-1;
    if(column)*column=s?s->event_column:-1;
    return s?s->event:CUI_TABLE_NONE;
}
