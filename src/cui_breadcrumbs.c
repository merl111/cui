#include "cui_navigation_internal.h"
#include "cui_desktop_internal.h"
#include "cui_layouts.h"
#include <stdlib.h>
#include <string.h>
#define MAX_CRUMBS 128
typedef struct crumb_slot { cui_widget *row,*button,*label; } crumb_slot;
typedef struct breadcrumbs {
    crumb_slot slots[MAX_CRUMBS];
    cui_item_id ids[MAX_CRUMBS],activated;
    size_t count,capacity;
} breadcrumbs;
static breadcrumbs *state(const cui_widget *w)
{ return w && w->component==CUI_COMPONENT_BREADCRUMBS?w->payload:NULL; }
static void clicked(cui_widget *button,void *data)
{
    cui_widget *w=data;breadcrumbs *s=state(w);size_t index=(size_t)button->index;
    if(!s || index+1>=s->count)return;
    s->activated=s->ids[index];cui__emit(w);
}
static int ensure_slots(cui_widget *w,size_t count)
{
    breadcrumbs *s=state(w);
    while(s->capacity<count){
        size_t i=s->capacity;crumb_slot *slot=s->slots+i;
        if(!slot->row){slot->row=cui_box(w,CUI_HORIZONTAL,6);if(!slot->row)return 0;cui_set_visible(slot->row,0);}
        if(!slot->row->first && !cui_label(slot->row,i?"›":""))return 0;
        if(!slot->button){slot->button=cui_button(slot->row,"");if(!slot->button)return 0;}
        if(!slot->label){slot->label=cui_label(slot->row,"");if(!slot->label)return 0;}
        cui_set_role(slot->button,CUI_ROLE_SUBTLE);cui_set_role(slot->label,CUI_ROLE_HEADING);
        slot->button->index=(int)i;cui_on_action(slot->button,clicked,w);++s->capacity;
    }
    return 1;
}
int cui_breadcrumbs_set_items(cui_widget *w,const cui_breadcrumb_item *items,size_t count)
{
    breadcrumbs *s=state(w);char *texts[MAX_CRUMBS]={0};int okay=0;
    if(!s || count>MAX_CRUMBS || (count&&!items))return 0;
    for(size_t i=0;i<count;++i){
        if(!items[i].id || !items[i].text || !*items[i].text)goto cleanup;
        for(size_t j=0;j<i;++j)if(items[j].id==items[i].id)goto cleanup;
        texts[i]=cui__desktop_copy(items[i].text);if(!texts[i])goto cleanup;
    }
    if(!ensure_slots(w,count))goto cleanup;
    /* Keep keyboard focus useful when its path segment becomes current/hidden. */
    int refocus=0;
    for(size_t i=0;i<s->count;++i)if(cui_has_focus(s->slots[i].button))refocus=1;
    for(size_t i=0;i<s->capacity;++i){
        crumb_slot *slot=s->slots+i;cui_set_visible(slot->row,i<count);
        if(i>=count)continue;
        s->ids[i]=items[i].id;cui_set_text(slot->button,texts[i]);cui_set_text(slot->label,texts[i]);
        cui_accessibility(slot->button,texts[i],"Navigate to this location");
        cui_accessibility(slot->label,texts[i],"Current location");
        cui_set_visible(slot->button,i+1<count);cui_set_visible(slot->label,i+1==count);
    }
    s->count=count;s->activated=0;
    if(refocus && count>1)cui_focus(s->slots[count-2].button);
    okay=1;
cleanup:
    for(size_t i=0;i<count;++i)free(texts[i]);
    return okay;
}
cui_widget *cui_breadcrumbs(cui_widget *parent,const cui_breadcrumb_item *items,size_t count)
{
    breadcrumbs *s=calloc(1,sizeof(*s));if(!s)return NULL;
    cui_widget *w=cui_wrap(parent,6);if(!w){free(s);return NULL;}
    w->component=CUI_COMPONENT_BREADCRUMBS;w->payload=s;w->destroy_payload=free;
    if(!cui_breadcrumbs_set_items(w,items,count)){cui_set_visible(w,0);return NULL;}
    return w;
}
cui_item_id cui_breadcrumbs_current(const cui_widget *w)
{ breadcrumbs *s=state(w);return s&&s->count?s->ids[s->count-1]:0; }
cui_item_id cui_breadcrumbs_activated(const cui_widget *w)
{ breadcrumbs *s=state(w);return s?s->activated:0; }
int cui_breadcrumbs_activate(cui_widget *w,cui_item_id id)
{
    breadcrumbs *s=state(w);if(!s)return 0;
    for(size_t i=0;i+1<s->count;++i)if(s->ids[i]==id)return cui_activate(s->slots[i].button);
    return 0;
}
