#include "cui_patterns_internal.h"
#include "cui_desktop_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct item_slot { pattern_state *owner; size_t index; int ready; cui_widget *root,*parts[CUI_PART_COUNT]; } item_slot;
struct cui_pattern_collection {
    cui_pattern_item *items; size_t count,capacity;
    item_slot *slots[256]; cui_widget *root; cui_item_id event_id;
};
static int supports(pattern_state *s)
{return s&&(s->kind==CUI_TOOL_CHIPS||s->kind==CUI_TASK_ROWS||s->kind==CUI_CHAT||s->kind==CUI_CONTEXT);}
static void free_items(cui_pattern_item *items,size_t count)
{
    for(size_t i=0;i<count;++i){free((void *)items[i].title);free((void *)items[i].body);free((void *)items[i].detail);free((void *)items[i].badge);}
    free(items);
}
void cui__collection_free(struct cui_pattern_collection *c)
{
    if(!c)return;
    free_items(c->items,c->count);for(size_t i=0;i<c->capacity;++i)free(c->slots[i]);free(c);
}
static int copy_text(const char **out,const char *in,size_t limit)
{
    if(!in)in="";
    if(strlen(in)>limit)return 0;
    *out=cui__desktop_copy(in);return *out!=NULL;
}
static int copy_item(cui_pattern_item *out,const cui_pattern_item *in)
{
    if(!in->id||!in->title||!*in->title||!isfinite(in->progress)||in->progress<0||in->progress>1||(in->flags&~15u))return 0;
    if(in->tone!=CUI_ROLE_SUBTLE&&in->tone!=CUI_ROLE_SUCCESS&&in->tone!=CUI_ROLE_WARNING&&in->tone!=CUI_ROLE_DANGER)return 0;
    out->id=in->id;out->progress=in->progress;out->tone=in->tone;out->flags=in->flags;
    return copy_text(&out->title,in->title,255)&&copy_text(&out->body,in->body,65536)&&copy_text(&out->detail,in->detail,4096)&&copy_text(&out->badge,in->badge,63);
}
static cui_pattern_item *copy_items(const cui_pattern_item *items,size_t count)
{
    cui_pattern_item *copy=calloc(count?count:1,sizeof(*copy));if(!copy)return NULL;
    for(size_t i=0;i<count;++i){
        if(!copy_item(copy+i,items+i)){free_items(copy,count);return NULL;}
        for(size_t j=0;j<i;++j)if(items[j].id==items[i].id){free_items(copy,count);return NULL;}
    }
    return copy;
}
static cui_event primary_action(item_slot *slot,cui_pattern_item *item)
{
    cui_widget *primary=slot->parts[CUI_PART_PRIMARY];
    switch(slot->owner->kind){
    case CUI_TASK_ROWS:
        item->flags=(item->flags&~CUI_ITEM_COMPLETE)|(cui_get_checked(primary)?CUI_ITEM_COMPLETE:0);
        item->progress=(item->flags&CUI_ITEM_COMPLETE)?1:0;
        cui_set_value(slot->parts[CUI_PART_PROGRESS],item->progress);
        return CUI_EVENT_CHANGE;
    case CUI_CONTEXT:
        item->flags=(item->flags&~CUI_ITEM_SELECTED)|(cui_get_checked(primary)?CUI_ITEM_SELECTED:0);
        return CUI_EVENT_SELECT;
    case CUI_CHAT:
        cui_clipboard_set_text(slot->owner->root->window,item->body);
        return CUI_EVENT_SELECT;
    default:return CUI_EVENT_OPEN;
    }
}
static void item_action(cui_widget *sender,void *data)
{
    item_slot *slot=data;pattern_state *s=slot->owner;struct cui_pattern_collection *c=s->collection;
    if(slot->index>=c->count)return;
    cui_pattern_item *item=c->items+slot->index;cui_event event=CUI_EVENT_OPEN;
    if(sender==slot->parts[CUI_PART_DETAILS]){
        item->flags=(item->flags&~CUI_ITEM_EXPANDED)|(cui_get_expanded(sender)?CUI_ITEM_EXPANDED:0);
        event=CUI_EVENT_CHANGE;
    }else if(sender==slot->parts[CUI_PART_SECONDARY])event=CUI_EVENT_CANCEL;
    else if(sender==slot->parts[CUI_PART_PRIMARY])event=primary_action(slot,item);
    c->event_id=item->id;cui__pattern_emit(s,event);
}
static cui_widget *slot_body_parent(item_slot *slot)
{
    int disclosure=slot->owner->kind==CUI_TOOL_CHIPS||slot->owner->kind==CUI_TASK_ROWS;
    if(!disclosure){
        if(!slot->parts[CUI_PART_TITLE])slot->parts[CUI_PART_TITLE]=cui_button(slot->root,"");
        cui_on_action(slot->parts[CUI_PART_TITLE],item_action,slot);
        return slot->root;
    }
    if(!slot->parts[CUI_PART_DETAILS])slot->parts[CUI_PART_DETAILS]=cui_disclosure(slot->root,"",0);
    cui_widget *parent=cui_disclosure_content(slot->parts[CUI_PART_DETAILS]);
    if(!parent)return NULL;
    slot->parts[CUI_PART_TITLE]=slot->parts[CUI_PART_DETAILS]->first;
    cui_on_action(slot->parts[CUI_PART_DETAILS],item_action,slot);
    return parent;
}
static void slot_content(item_slot *slot,cui_widget *parent)
{
    if(!slot->parts[CUI_PART_STATUS])slot->parts[CUI_PART_STATUS]=cui_badge(slot->root,"",CUI_ROLE_SUBTLE);
    if(!slot->parts[CUI_PART_BODY]){
        slot->parts[CUI_PART_BODY]=cui_textarea(parent,"");
        cui_set_read_only(slot->parts[CUI_PART_BODY],1);
    }
    if(!slot->parts[CUI_PART_AUXILIARY])slot->parts[CUI_PART_AUXILIARY]=cui_label(parent,"");
    cui_set_role(slot->parts[CUI_PART_AUXILIARY],CUI_ROLE_CAPTION);
    if(slot->owner->kind==CUI_TASK_ROWS&&!slot->parts[CUI_PART_PROGRESS])slot->parts[CUI_PART_PROGRESS]=cui_progress(parent,0);
}
static void slot_actions(item_slot *slot)
{
    cui_pattern kind=slot->owner->kind;
    if(!slot->parts[CUI_PART_PRIMARY]){
        if(kind==CUI_TASK_ROWS)slot->parts[CUI_PART_PRIMARY]=cui_checkbox(slot->root,"Completed",0);
        else if(kind==CUI_CONTEXT)slot->parts[CUI_PART_PRIMARY]=cui_checkbox(slot->root,"Use this source",0);
        else slot->parts[CUI_PART_PRIMARY]=cui_button(slot->root,kind==CUI_CHAT?"Copy message":"Open tool result");
    }
    if(!slot->parts[CUI_PART_SECONDARY])slot->parts[CUI_PART_SECONDARY]=cui_button(slot->root,"Remove");
    cui_on_action(slot->parts[CUI_PART_PRIMARY],item_action,slot);
    cui_on_action(slot->parts[CUI_PART_SECONDARY],item_action,slot);
}
static int slot_complete(item_slot *slot)
{
    const cui_part required[]={CUI_PART_TITLE,CUI_PART_BODY,CUI_PART_STATUS,CUI_PART_AUXILIARY,CUI_PART_PRIMARY,CUI_PART_SECONDARY};
    for(size_t i=0;i<sizeof(required)/sizeof(required[0]);++i)if(!slot->parts[required[i]])return 0;
    return slot->owner->kind!=CUI_TASK_ROWS||slot->parts[CUI_PART_PROGRESS];
}
static int make_slot(struct cui_pattern_collection *c,pattern_state *s,size_t index)
{
    item_slot *slot=c->slots[index];
    if(slot&&slot->ready)return 1;
    if(!slot){
        slot=calloc(1,sizeof(*slot));if(!slot)return 0;
        c->slots[index]=slot;slot->owner=s;slot->index=index;c->capacity=index+1;
    }
    if(!slot->root)slot->root=cui_box(c->root,CUI_VERTICAL,8);
    if(!slot->root)return 0;
    cui_set_visible(slot->root,0);cui_box_set_padding(slot->root,12);cui_set_role(slot->root,CUI_ROLE_CARD);
    cui_widget *parent=slot_body_parent(slot);if(!parent)return 0;
    slot_content(slot,parent);slot_actions(slot);
    slot->ready=slot_complete(slot);return slot->ready;
}
static void show_item(item_slot *slot,const cui_pattern_item *item)
{
    cui_set_text(slot->parts[CUI_PART_TITLE],item->title);cui_set_text(slot->parts[CUI_PART_BODY],item->body);
    cui_set_text(slot->parts[CUI_PART_AUXILIARY],item->detail);cui_set_visible(slot->parts[CUI_PART_AUXILIARY],!!*item->detail);
    cui_set_text(slot->parts[CUI_PART_STATUS],item->badge);cui_set_role(slot->parts[CUI_PART_STATUS],item->tone);
    cui_set_visible(slot->parts[CUI_PART_STATUS],!!*item->badge);cui_set_value(slot->parts[CUI_PART_PROGRESS],item->progress);
    cui_set_expanded(slot->parts[CUI_PART_DETAILS],!!(item->flags&CUI_ITEM_EXPANDED));
    if(slot->owner->kind==CUI_TASK_ROWS)cui_set_checked(slot->parts[CUI_PART_PRIMARY],!!(item->flags&CUI_ITEM_COMPLETE));
    if(slot->owner->kind==CUI_CONTEXT)cui_set_checked(slot->parts[CUI_PART_PRIMARY],!!(item->flags&CUI_ITEM_SELECTED));
    cui_set_enabled(slot->root,!(item->flags&CUI_ITEM_DISABLED));cui_accessibility(slot->root,item->title,item->detail);
    cui_accessibility(slot->parts[CUI_PART_PRIMARY],item->title,"Item action");
    cui_accessibility(slot->parts[CUI_PART_SECONDARY],item->title,"Request removal");
    cui_set_visible(slot->root,1);
}
static void hide_legacy(pattern_state *s,struct cui_pattern_collection *c)
{
    if(s->kind==CUI_CHAT||s->kind==CUI_CONTEXT){cui_set_visible(s->parts[CUI_PART_BODY],0);if(s->kind==CUI_CONTEXT)cui_set_visible(s->parts[CUI_PART_DETAILS],0);}
    else for(cui_widget *child=s->root->first;child;child=child->next)
        if(child!=s->parts[CUI_PART_TITLE]&&child!=s->parts[CUI_PART_STATUS]&&child!=s->spinner&&child!=c->root)cui_set_visible(child,0);
    s->parts[CUI_PART_BODY]=c->root;cui_set_visible(c->root,1);
}
static int item_query_match(const cui_pattern_item *item,const char *query)
{
    if(!query||!*query)return 1;
    const char *fields[]={item->title,item->body,item->detail};
    for(size_t i=0;i<3;++i){
        char *key=cui__search_key(fields[i]);if(!key)return -1;
        int match=strstr(key,query)!=NULL;free(key);if(match)return 1;
    }
    return 0;
}
static char *collection_query(pattern_state *s)
{
    cui_widget *input=s->parts[CUI_PART_INPUT];
    size_t n=cui_get_text(input,NULL,0)+1;char *text=malloc(n);if(!text)return NULL;
    cui_get_text(input,text,n);char *query=cui__search_key(text);free(text);return query;
}
static int collection_matches(pattern_state *s,const cui_pattern_item *item,const char *query)
{
    if(s->kind!=CUI_CONTEXT)return 1;
    int match=item_query_match(item,query);if(match!=1)return match;
    const char *row[]={item->title,item->badge,item->detail};
    return cui__pattern_matches(s,row);
}
static void show_slots(struct cui_pattern_collection *c,const unsigned char *visible)
{
    for(size_t i=0;i<c->capacity;++i){
        item_slot *slot=c->slots[i];if(!slot->ready)continue;
        if(i<c->count)show_item(slot,c->items+i);
        cui_set_visible(slot->root,i<c->count&&visible[i]);
    }
}
int cui__collection_refresh(pattern_state *s)
{
    struct cui_pattern_collection *c=s->collection;if(!c)return 1;
    char *query=s->kind==CUI_CONTEXT?collection_query(s):NULL;
    if(s->kind==CUI_CONTEXT&&!query)return 0;
    size_t matches=0;unsigned char visible[256]={0};
    for(size_t i=0;i<c->count;++i){
        int match=collection_matches(s,c->items+i,query);
        if(match<0){free(query);return 0;}
        visible[i]=(unsigned char)match;matches+=visible[i];
    }
    free(query);show_slots(c,visible);
    char status[80];snprintf(status,sizeof(status),"%zu item%s",matches,matches==1?"":"s");
    cui_set_text(s->parts[CUI_PART_STATUS],matches?status:"No items");return 1;
}
static struct cui_pattern_collection *ensure_collection(pattern_state *s)
{
    if(s->collection)return s->collection;
    struct cui_pattern_collection *c=calloc(1,sizeof(*c));if(!c)return NULL;
    cui_widget *parent=s->kind==CUI_CHAT?s->parts[CUI_PART_BODY]->parent:s->root;
    c->root=cui_box(parent,CUI_VERTICAL,12);if(!c->root){free(c);return NULL;}
    cui_set_visible(c->root,0);s->collection=c;return c;
}
int cui_pattern_set_items(cui_widget *w,const cui_pattern_item *items,size_t count)
{
    pattern_state *s=cui__pattern_state(w);if(!supports(s)||count>256||(count&&!items))return 0;
    cui_pattern_item *copy=copy_items(items,count);if(!copy)return 0;
    struct cui_pattern_collection *c=ensure_collection(s);
    if(!c){free_items(copy,count);return 0;}
    for(size_t i=0;i<count;++i)if(!make_slot(c,s,i)){free_items(copy,count);return 0;}
    cui_pattern_item *previous=c->items;size_t previous_count=c->count;
    c->items=copy;c->count=count;
    if(!cui__collection_refresh(s)){c->items=previous;c->count=previous_count;free_items(copy,count);return 0;}
    if(s->parts[CUI_PART_BODY]!=c->root)hide_legacy(s,c);
    c->event_id=0;free_items(previous,previous_count);return 1;
}
size_t cui_pattern_item_count(const cui_widget *w)
{pattern_state *s=cui__pattern_state(w);return s&&s->collection?s->collection->count:0;}
int cui_pattern_item_at(const cui_widget *w,size_t index,cui_pattern_item *out)
{
    pattern_state *s=cui__pattern_state(w);if(!out||!s||!s->collection||index>=s->collection->count)return 0;
    *out=s->collection->items[index];return 1;
}
int cui_pattern_upsert_item(cui_widget *w,const cui_pattern_item *item)
{
    pattern_state *s=cui__pattern_state(w);if(!supports(s)||!item)return 0;
    size_t count=cui_pattern_item_count(w),index=count;cui_pattern_item items[256];
    for(size_t i=0;i<count;++i){items[i]=s->collection->items[i];if(items[i].id==item->id)index=i;}
    if(index==256)return 0;
    items[index]=*item;return cui_pattern_set_items(w,items,count+(index==count));
}
int cui_pattern_remove_item(cui_widget *w,cui_item_id id)
{
    pattern_state *s=cui__pattern_state(w);if(!supports(s)||!s->collection)return 0;
    size_t count=0;cui_pattern_item items[256];
    for(size_t i=0;i<s->collection->count;++i)if(s->collection->items[i].id!=id)items[count++]=s->collection->items[i];
    return count<s->collection->count&&cui_pattern_set_items(w,items,count);
}
cui_item_id cui_pattern_item_event_id(const cui_widget *w)
{pattern_state *s=cui__pattern_state(w);return s&&s->collection?s->collection->event_id:0;}
cui_widget *cui_pattern_item_part(cui_widget *w,cui_item_id id,cui_part part)
{
    pattern_state *s=cui__pattern_state(w);if(!s||!s->collection||part<0||part>=CUI_PART_COUNT)return NULL;
    for(size_t i=0;i<s->collection->count;++i)if(s->collection->items[i].id==id){item_slot *slot=s->collection->slots[i];return part==CUI_PART_PREVIEW?slot->root:slot->parts[part];}
    return NULL;
}

int cui__collection_active(pattern_state *s)
{return s&&s->collection&&s->parts[CUI_PART_BODY]==s->collection->root;}
