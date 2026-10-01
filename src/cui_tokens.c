#include "cui_tokens.h"
#include "cui_search_internal.h"
#include "cui_layouts.h"
#include "cui_desktop.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef struct token_state token_state;
typedef struct token_slot {token_state *owner;size_t index;cui_widget *button;} token_slot;
struct token_state {
    cui_widget *root,*parts[5];
    cui_choice_model *model;
    cui_item_id selected[128],changed;
    size_t count,limit,slots;
    token_slot buttons[128];
    cui_tokens_event event;
    cui_key_callback input_key;void *input_data;
};
static void dispose(void *data)
{token_state *s=data;cui__choices_free(s->model);free(s);}
static token_state *state(const cui_widget *w)
{return w&&w->destroy_payload==dispose?w->payload:NULL;}
static int contains(const cui_item_id *ids,size_t count,cui_item_id id)
{for(size_t i=0;i<count;++i)if(ids[i]==id)return 1;return 0;}
static int interactive(token_state *s)
{
    if(!s)return 0;
    for(cui_widget *w=s->root;w;w=w->parent)if(w->hidden||!w->enabled)return 0;
    return 1;
}
static void event(token_state *s,cui_tokens_event value,cui_item_id id)
{s->event=value;s->changed=id;cui__emit(s->root);}
static void removed(cui_widget *sender,void *data)
{
    (void)sender;token_slot *slot=data;token_state *s=slot->owner;
    if(slot->index<s->count)cui_tokens_remove(s->root,s->selected[slot->index]);
}
static int reserve(token_state *s,size_t count)
{
    while(s->slots<count){
        token_slot *slot=&s->buttons[s->slots];slot->owner=s;slot->index=s->slots;
        slot->button=cui_button(s->parts[CUI_TOKENS_CHIPS],"");if(!slot->button)return 0;
        cui_set_visible(slot->button,0);cui_on_action(slot->button,removed,slot);++s->slots;
    }
    return 1;
}
static void render(token_state *s)
{
    for(size_t i=0;i<s->slots;++i){
        cui_widget *button=s->buttons[i].button;cui_set_visible(button,i<s->count);
        if(i>=s->count)continue;
        cui_choice_entry *item=cui__choices_find(s->model,s->selected[i]);
        size_t n=strlen(item->label);char *label=malloc(n+10);
        if(label){snprintf(label,n+10,"%s ×",item->label);cui_set_text(button,label);snprintf(label,n+10,"Remove %s",item->label);cui_accessibility(button,label,"Remove this selection");free(label);}
    }
    char label[64];snprintf(label,sizeof(label),"%zu of %zu selected",s->count,s->limit);cui_set_text(s->parts[CUI_TOKENS_STATUS],label);
    cui_set_visible(s->parts[CUI_TOKENS_CHIPS],s->count>0);cui_set_enabled(s->parts[CUI_TOKENS_CLEAR_BUTTON],s->count>0);
}
static int valid_selection(token_state *s,cui_choice_model *model,const cui_item_id *ids,size_t count)
{
    if(count>s->limit || (count&&!ids))return 0;
    for(size_t i=0;i<count;++i){
        cui_choice_entry *item=cui__choices_find(model,ids[i]);
        if(!item || item->disabled || contains(ids,i,ids[i]))return 0;
    }
    return 1;
}
static int suggestions(token_state *s,cui_choice_model *model,const cui_item_id *ids,size_t count)
{
    if(count==s->limit)return cui_picker_set_items(s->parts[CUI_TOKENS_PICKER],NULL,0);
    size_t total=model?model->count:0,available=0;
    cui_choice *choices=total?calloc(total,sizeof(*choices)):NULL;if(total&&!choices)return 0;
    for(size_t i=0;i<total;++i){
        cui_choice_entry *item=model->items+i;if(contains(ids,count,item->id))continue;
        choices[available++]=(cui_choice){item->id,item->label,item->detail,item->keywords,item->disabled};
    }
    int okay=cui_picker_set_items(s->parts[CUI_TOKENS_PICKER],choices,available);free(choices);return okay;
}
static int apply(token_state *s,cui_choice_model *model,const cui_item_id *ids,size_t count)
{
    if(!valid_selection(s,model,ids,count)||!reserve(s,count)||!suggestions(s,model,ids,count))return 0;
    if(model!=s->model){cui__choices_free(s->model);s->model=model;}
    if(count)memmove(s->selected,ids,count*sizeof(*ids));
    s->count=count;render(s);return 1;
}
int cui_tokens_set_items(cui_widget *w,const cui_choice *items,size_t count)
{
    token_state *s=state(w);if(!s)return 0;
    cui_choice_model *model=cui__choices_copy(items,count);if(!model)return 0;
    cui_item_id keep[128];size_t n=0;
    for(size_t i=0;i<s->count;++i){cui_choice_entry *item=cui__choices_find(model,s->selected[i]);if(item&&!item->disabled)keep[n++]=item->id;}
    if(apply(s,model,keep,n))return 1;
    cui__choices_free(model);return 0;
}
int cui_tokens_set_selected(cui_widget *w,const cui_item_id *ids,size_t count)
{token_state *s=state(w);return s&&apply(s,s->model,ids,count);}
size_t cui_tokens_get_selected(const cui_widget *w,cui_item_id *ids,size_t capacity)
{token_state *s=state(w);if(!s)return 0;size_t n=s->count<capacity?s->count:capacity;if(ids&&n)memcpy(ids,s->selected,n*sizeof(*ids));return s->count;}
int cui_tokens_add(cui_widget *w,cui_item_id id)
{
    token_state *s=state(w);if(!interactive(s)||s->count==s->limit||contains(s->selected,s->count,id))return 0;
    cui_item_id ids[128];memcpy(ids,s->selected,s->count*sizeof(*ids));ids[s->count]=id;
    if(!apply(s,s->model,ids,s->count+1))return 0;
    cui_picker_set_query(s->parts[CUI_TOKENS_PICKER],"");event(s,CUI_TOKENS_ADD,id);return 1;
}
int cui_tokens_remove(cui_widget *w,cui_item_id id)
{
    token_state *s=state(w);if(!interactive(s)||!contains(s->selected,s->count,id))return 0;
    cui_item_id ids[128];size_t n=0;int restore=0;
    for(size_t i=0;i<s->count;++i){if(s->selected[i]!=id)ids[n++]=s->selected[i];else restore=cui_has_focus(s->buttons[i].button);}
    if(!apply(s,s->model,ids,n))return 0;
    if(restore)cui_focus(s->parts[CUI_TOKENS_INPUT]);
    event(s,CUI_TOKENS_REMOVE,id);return 1;
}
int cui_tokens_clear(cui_widget *w)
{
    token_state *s=state(w);if(!interactive(s)||!s->count||!apply(s,s->model,NULL,0))return 0;
    cui_focus(s->parts[CUI_TOKENS_INPUT]);event(s,CUI_TOKENS_CLEAR,0);return 1;
}
static void clear(cui_widget *sender,void *data)
{(void)sender;cui_tokens_clear(((token_state *)data)->root);}
static void picked(cui_widget *sender,void *data)
{
    token_state *s=data;switch(cui_picker_last_event(sender)){
    case CUI_PICKER_SELECT:cui_tokens_add(s->root,cui_picker_selected(sender));break;
    case CUI_PICKER_QUERY:event(s,CUI_TOKENS_QUERY,0);break;
    case CUI_PICKER_SUBMIT:event(s,CUI_TOKENS_SUBMIT,0);break;
    default:break;
    }
}
static int input_key(cui_widget *sender,cui_key key,unsigned mods,void *data)
{
    token_state *s=data;
    if(key==CUI_KEY_BACKSPACE&&!mods&&s->count&&!cui_get_text(sender,NULL,0))return cui_tokens_remove(s->root,s->selected[s->count-1]);
    return s->input_key?s->input_key(sender,key,mods,s->input_data):0;
}
cui_widget *cui_tokens(cui_widget *parent,const char *placeholder,size_t limit)
{
    if(!limit||limit>128)return NULL;
    token_state *s=calloc(1,sizeof(*s));if(!s)return NULL;
    cui_widget *w=cui_box(parent,CUI_VERTICAL,8);if(!w){free(s);return NULL;}
    w->payload=s;w->destroy_payload=dispose;s->root=w;s->limit=limit;
    s->parts[CUI_TOKENS_CHIPS]=cui_wrap(w,6);s->parts[CUI_TOKENS_PICKER]=cui_picker(w,CUI_AUTOCOMPLETE,placeholder);
    s->parts[CUI_TOKENS_INPUT]=cui_picker_get_part(s->parts[CUI_TOKENS_PICKER],CUI_PICKER_INPUT);
    cui_widget *footer=cui_box(w,CUI_HORIZONTAL,8);s->parts[CUI_TOKENS_STATUS]=cui_label(footer,"");s->parts[CUI_TOKENS_CLEAR_BUTTON]=cui_button(footer,"Clear all");
    for(int i=0;i<5;++i)if(!s->parts[i]){cui_set_visible(w,0);return NULL;}
    cui_expand(s->parts[CUI_TOKENS_STATUS],1);cui_set_role(s->parts[CUI_TOKENS_STATUS],CUI_ROLE_CAPTION);
    cui_on_action(s->parts[CUI_TOKENS_PICKER],picked,s);cui_on_action(s->parts[CUI_TOKENS_CLEAR_BUTTON],clear,s);
    cui_widget *input=s->parts[CUI_TOKENS_INPUT];s->input_key=input->key_callback;s->input_data=input->key_userdata;cui_on_key(input,input_key,s);
    render(s);return w;
}
cui_tokens_event cui_tokens_last_event(const cui_widget *w){token_state *s=state(w);return s?s->event:CUI_TOKENS_NONE;}
cui_item_id cui_tokens_changed(const cui_widget *w){token_state *s=state(w);return s?s->changed:0;}
cui_widget *cui_tokens_get_part(cui_widget *w,cui_tokens_part part){token_state *s=state(w);return s&&part>=0&&part<5?s->parts[part]:NULL;}
cui_widget *cui_tokens_remove_button(cui_widget *w,size_t index){token_state *s=state(w);return s&&index<s->count?s->buttons[index].button:NULL;}
