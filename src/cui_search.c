#include "cui_search_internal.h"
#include "cui_desktop.h"
#include "cui_tables_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef struct picker_state {
    cui_widget *root,*parts[5],*return_focus;
    cui_picker_kind kind;cui_picker_event event;
    cui_choice_model *model;
    size_t *matches,count;
    cui_item_id selected;
    cui_timer *timer;
    int open,headings,status,actions;
} picker_state;
static void dispose(void *data)
{picker_state *s=data;cui__choices_free(s->model);free(s->matches);free(s);}
static picker_state *state(const cui_widget *w)
{return w&&w->destroy_payload==dispose?w->payload:NULL;}
static void emit(picker_state *s,cui_picker_event event)
{s->event=event;cui__emit(s->root);}
static cui_choice_entry *at_row(picker_state *s,int row)
{
    if(row<0 || !s->model || !s->model->items || !s->matches)return NULL;
    size_t original=cui_table_source_row(s->parts[CUI_PICKER_RESULTS],(size_t)row);
    if(original>=s->count || s->matches[original]>=s->model->count)return NULL;
    return s->model->items+s->matches[original];
}
static void visibility(picker_state *s)
{
    if(s->kind==CUI_COMMAND_PALETTE)cui_set_visible(s->root,s->open);
    cui_set_visible(s->parts[CUI_PICKER_STATUS],s->open&&s->status);
    cui_set_visible(s->parts[CUI_PICKER_ACCEPT]->parent,s->open&&s->actions);
    cui_set_visible(s->parts[CUI_PICKER_RESULTS],s->open&&s->count);
}
static size_t match_choices(picker_state *s,const char *key,size_t *map,const char **cells,int *selected)
{
    size_t total=s->model?s->model->count:0,count=0;
    for(size_t i=0;i<total;++i){
        cui_choice_entry *item=s->model->items+i;
        if(!strstr(item->key,key))continue;
        if(*selected<0&&!item->disabled)*selected=(int)count;
        map[count]=i;cells[count*2]=item->label;cells[count*2+1]=item->disabled?"Unavailable":item->detail;++count;
    }
    return count;
}
static int refresh(picker_state *s,const char *query)
{
    char *key=cui__search_key(query);if(!key)return 0;
    size_t total=s->model?s->model->count:0,count=0;
    size_t *map=total?malloc(total*sizeof(*map)):NULL;
    const char **cells=total?malloc(total*2*sizeof(*cells)):NULL;
    if(total&&(!map||!cells)){free(key);free(map);free(cells);return 0;}
    int selected=-1;
    count=match_choices(s,key,map,cells,&selected);
    free(key);
    if(!cui_table_set_rows(s->parts[CUI_PICKER_RESULTS],cells,count)){free(map);free(cells);return 0;}
    free(cells);free(s->matches);s->matches=map;s->count=count;s->selected=0;
    cui_set_selected(s->parts[CUI_PICKER_RESULTS],selected);
    if(selected>=0)cui__backend_table_reveal(s->parts[CUI_PICKER_RESULTS],(size_t)selected);
    cui_set_enabled(s->parts[CUI_PICKER_ACCEPT],selected>=0);
    char status[96];snprintf(status,sizeof(status),"%zu result%s · ↑ ↓ to navigate · Enter to choose",count,count==1?"":"s");
    cui_set_text(s->parts[CUI_PICKER_STATUS],count?status:"No matches");visibility(s);return 1;
}
static char *query_text(picker_state *s)
{
    size_t n=cui_get_text(s->parts[0],NULL,0);char *text=malloc(n+1);
    if(text)cui_get_text(s->parts[0],text,n+1);
    return text;
}
int cui_picker_set_items(cui_widget *w,const cui_choice *items,size_t count)
{
    picker_state *s=state(w);if(!s)return 0;
    cui_choice_model *model=cui__choices_copy(items,count);if(!model)return 0;
    char *query=query_text(s);if(!query){cui__choices_free(model);return 0;}
    cui_choice_model *previous=s->model;s->model=model;
    int okay=refresh(s,query);free(query);
    if(!okay){s->model=previous;cui__choices_free(model);return 0;}
    cui__choices_free(previous);return 1;
}
int cui_picker_set_query(cui_widget *w,const char *query)
{
    picker_state *s=state(w);if(!s || !query || !refresh(s,query))return 0;
    cui_set_text(s->parts[0],query);return 1;
}
size_t cui_picker_get_query(const cui_widget *w,char *buffer,size_t capacity)
{picker_state *s=state(w);return cui_get_text(s?s->parts[0]:NULL,buffer,capacity);}
void cui_picker_close(cui_widget *w)
{
    picker_state *s=state(w);if(!s)return;
    int restore=cui__contains_focus(w);s->open=0;visibility(s);cui_timer_stop(s->timer);
    if(restore&&s->return_focus)cui_focus(s->return_focus);
}
static void focus_tick(void *data)
{
    picker_state *s=data;
    if(!s->root->window->visible)return;
    if(!cui__contains_focus(s->root)){cui_picker_close(s->root);emit(s,CUI_PICKER_CANCEL);}
}
void cui_picker_open(cui_widget *w,cui_widget *return_focus)
{
    picker_state *s=state(w);if(!s || (return_focus&&return_focus->window!=w->window))return;
    s->return_focus=return_focus;s->open=1;visibility(s);cui_focus(s->parts[0]);
    if(!s->timer)s->timer=cui_every(w->window->app,100,focus_tick,s);
    else cui_timer_start(s->timer);
}
int cui_picker_select(cui_widget *w,cui_item_id id)
{
    picker_state *s=state(w);if(!s)return 0;
    cui_choice_entry *item=cui__choices_find(s->model,id);if(!item||item->disabled)return 0;
    if(!cui_picker_set_query(w,item->label))return 0;
    s->selected=id;cui_picker_close(w);return 1;
}
int cui_picker_accept(cui_widget *w)
{
    picker_state *s=state(w);if(!s||!s->open)return 0;
    for(cui_widget *p=w;p;p=p->parent)if(p->hidden||!p->enabled)return 0;
    cui_choice_entry *item=at_row(s,cui_get_selected(s->parts[CUI_PICKER_RESULTS]));
    if(item&&!item->disabled){cui_item_id id=item->id;if(!cui_picker_select(w,id))return 0;emit(s,CUI_PICKER_SELECT);return 1;}
    if(s->kind==CUI_AUTOCOMPLETE && cui_get_text(s->parts[0],NULL,0)){
        s->selected=0;cui_picker_close(w);emit(s,CUI_PICKER_SUBMIT);return 1;
    }
    return 0;
}
static void accept(cui_widget *sender,void *data)
{(void)sender;cui_picker_accept(((picker_state *)data)->root);}
static void close_action(cui_widget *sender,void *data)
{(void)sender;picker_state *s=data;cui_picker_close(s->root);emit(s,CUI_PICKER_CANCEL);}
static void edited(cui_widget *sender,void *data)
{
    (void)sender;picker_state *s=data;char *query=query_text(s);if(!query)return;
    if(refresh(s,query)){
        if(!s->open)cui_picker_open(s->root,NULL);
        emit(s,CUI_PICKER_QUERY);
    }
    free(query);
}
static void result_selected(cui_widget *sender,void *data)
{
    picker_state *s=data;int row,column;
    if(cui_table_last_event(sender,&row,&column)!=CUI_TABLE_SELECTION)return;
    cui_choice_entry *item=at_row(s,cui_get_selected(sender));
    if(!item||item->disabled){cui_set_selected(sender,-1);return;}
    cui_picker_accept(s->root);
}
static void move(picker_state *s,int direction)
{
    int row=cui_get_selected(s->parts[CUI_PICKER_RESULTS]);
    if(row<0)row=direction>0?-1:(int)s->count;
    for(row+=direction;row>=0&&(size_t)row<s->count;row+=direction){
        cui_choice_entry *item=at_row(s,row);if(!item||item->disabled)continue;
        cui_set_selected(s->parts[CUI_PICKER_RESULTS],row);cui__backend_table_reveal(s->parts[CUI_PICKER_RESULTS],(size_t)row);return;
    }
}
static void accept_tab(picker_state *s)
{
    if(s->kind!=CUI_AUTOCOMPLETE||!s->open)return;
    cui_choice_entry *item=at_row(s,cui_get_selected(s->parts[CUI_PICKER_RESULTS]));
    if(item&&!item->disabled)cui_picker_accept(s->root);
    else cui_picker_close(s->root);
}
static int key(cui_widget *sender,cui_key key,unsigned modifiers,void *data)
{
    (void)sender;picker_state *s=data;if(modifiers)return 0;
    switch(key){
    case CUI_KEY_DOWN:case CUI_KEY_UP:
        if(!s->open)cui_picker_open(s->root,NULL);else move(s,key==CUI_KEY_DOWN?1:-1);
        return 1;
    case CUI_KEY_ESCAPE:if(s->open){close_action(NULL,s);return 1;}return 0;
    case CUI_KEY_ENTER:return cui_picker_accept(s->root);
    case CUI_KEY_TAB:accept_tab(s);return 0;
    default:return 0;
    }
}
cui_widget *cui_picker(cui_widget *parent,cui_picker_kind kind,const char *placeholder)
{
    if(kind<CUI_AUTOCOMPLETE || kind>CUI_COMMAND_PALETTE)return NULL;
    picker_state *s=calloc(1,sizeof(*s));if(!s)return NULL;
    cui_widget *w=cui_box(parent,CUI_VERTICAL,8);if(!w){free(s);return NULL;}
    w->payload=s;w->destroy_payload=dispose;s->root=w;s->kind=kind;s->headings=s->status=s->actions=1;
    if(kind==CUI_COMMAND_PALETTE){cui_set_role(w,CUI_ROLE_CARD);cui_box_set_padding(w,16);}
    s->parts[0]=cui_search(w,placeholder);const char *headers[]={"Name","Details"};
    s->parts[1]=cui_table(w,headers,2);s->parts[2]=cui_label(w,"No matches");cui_set_role(s->parts[2],CUI_ROLE_CAPTION);
    cui_widget *actions=cui_box(w,CUI_HORIZONTAL,8);s->parts[3]=cui_button(actions,"Choose");s->parts[4]=cui_button(actions,"Close");
    for(int i=0;i<5;++i)if(!s->parts[i]){cui_set_visible(w,0);return NULL;}
    cui_set_role(s->parts[3],CUI_ROLE_PRIMARY);cui_set_enabled(s->parts[3],0);
    cui_on_action(s->parts[0],edited,s);cui_on_action(s->parts[1],result_selected,s);
    cui_on_action(s->parts[3],accept,s);cui_on_action(s->parts[4],close_action,s);
    cui_on_key(s->parts[0],key,s);cui_on_key(s->parts[1],key,s);
    cui_accessibility(s->parts[0],kind==CUI_COMMAND_PALETTE?"Search commands":"Search suggestions","Use arrow keys to choose a result, Enter to accept, Escape to close");
    visibility(s);return w;
}
int cui_picker_set_chrome(cui_widget *w,int headings,int status,int actions)
{
    picker_state *s=state(w);
    if(!s||(headings!=0&&headings!=1)||(status!=0&&status!=1)||(actions!=0&&actions!=1))return 0;
    s->headings=headings;s->status=status;s->actions=actions;
    cui__backend_table_headers(s->parts[CUI_PICKER_RESULTS],headings);
    visibility(s);return 1;
}
int cui_picker_is_open(const cui_widget *w){picker_state *s=state(w);return s&&s->open;}
cui_item_id cui_picker_selected(const cui_widget *w){picker_state *s=state(w);return s?s->selected:0;}
size_t cui_picker_match_count(const cui_widget *w){picker_state *s=state(w);return s?s->count:0;}
cui_picker_event cui_picker_last_event(const cui_widget *w){picker_state *s=state(w);return s?s->event:CUI_PICKER_NONE;}
cui_widget *cui_picker_get_part(cui_widget *w,cui_picker_part part)
{picker_state *s=state(w);return s&&part>=0&&part<5?s->parts[part]:NULL;}
