#include "cui_internal.h"
#include "cui_desktop.h"
#include "cui_feedback.h"
#include <stdlib.h>
typedef struct feedback_state {
    cui_widget *root,*badge,*parts[4];
    cui_feedback_kind kind;
    cui_feedback_event event;
    cui_timer *timer;
    double remaining,last;
    int paused;
} feedback_state;
static void dispose(void *data){free(data);}
static feedback_state *state(const cui_widget *w)
{return w&&w->destroy_payload==dispose?w->payload:NULL;}
static void event(feedback_state *s,cui_feedback_event value)
{s->event=value;cui__emit(s->root);}
void cui_feedback_dismiss(cui_widget *w)
{
    feedback_state *s=state(w);if(!s)return;
    cui_set_visible(w,0);cui_timer_stop(s->timer);s->remaining=0;
}
static void dismiss(cui_widget *sender,void *data)
{(void)sender;feedback_state *s=data;cui_feedback_dismiss(s->root);event(s,CUI_FEEDBACK_DISMISS);}
static void action(cui_widget *sender,void *data)
{(void)sender;event(data,CUI_FEEDBACK_ACTION);}
static int paused(feedback_state *s)
{
    if(s->paused || !s->root->window->visible)return 1;
    for(cui_widget *p=s->root;p;p=p->parent)if(p->hidden || !p->enabled)return 1;
    return cui__contains_focus(s->root)||cui__backend_hover(s->root);
}
static void tick(void *data)
{
    feedback_state *s=data;double now=cui_time(),elapsed=now-s->last;s->last=now;
    if(paused(s))return;
    s->remaining-=elapsed;
    if(s->remaining<=0){cui_feedback_dismiss(s->root);event(s,CUI_FEEDBACK_TIMEOUT);}
}
cui_widget *cui_feedback(cui_widget *parent,cui_feedback_kind kind)
{
    if(kind<CUI_BANNER||kind>CUI_ERROR_STATE)return NULL;
    feedback_state *s=calloc(1,sizeof(*s));if(!s)return NULL;
    cui_widget *w=cui_box(parent,CUI_VERTICAL,8);if(!w){free(s);return NULL;}
    s->root=w;s->kind=kind;w->payload=s;w->destroy_payload=dispose;
    cui_box_set_padding(w,16);cui_set_role(w,CUI_ROLE_CARD);
    cui_widget *heading=cui_box(w,CUI_HORIZONTAL,10);
    s->badge=cui_badge(heading,"",CUI_ROLE_SUBTLE);
    s->parts[CUI_FEEDBACK_TITLE]=cui_label(heading,"");cui_expand(s->parts[0],1);cui_set_role(s->parts[0],CUI_ROLE_HEADING);
    s->parts[CUI_FEEDBACK_DISMISS_BUTTON]=cui_button(heading,"Dismiss");
    s->parts[CUI_FEEDBACK_MESSAGE]=cui_label(w,"");
    s->parts[CUI_FEEDBACK_ACTION_BUTTON]=cui_button(w,"");
    if(!heading||!s->badge||!s->parts[0]||!s->parts[1]||!s->parts[2]||!s->parts[3]){cui_set_visible(w,0);return NULL;}
    cui_set_role(s->parts[2],CUI_ROLE_PRIMARY);cui_on_action(s->parts[2],action,s);cui_on_action(s->parts[3],dismiss,s);
    cui_accessibility(s->parts[3],"Dismiss notification","");
    cui_set_visible(w,0);return w;
}
static void part_text(feedback_state *s,cui_feedback_part part,const char *text)
{cui_set_text(s->parts[part],text);cui_set_visible(s->parts[part],text&&*text);}
static const char *tone_label(cui_role tone)
{
    switch(tone){
    case CUI_ROLE_SUCCESS:return "Success";case CUI_ROLE_WARNING:return "Attention";
    case CUI_ROLE_DANGER:return "Error";case CUI_ROLE_SUBTLE:return "Information";
    default:return NULL;
    }
}
int cui_feedback_show(cui_widget *w,const char *title,const char *message,cui_role tone,const char *label,unsigned timeout)
{
    feedback_state *s=state(w);
    if(!s || timeout>86400000 || (timeout&&s->kind!=CUI_TOAST))return 0;
    const char *badge=tone_label(tone);if(!badge)return 0;
    if(timeout){
        if(!s->timer)s->timer=cui_every(w->window->app,50,tick,s);
        else if(!cui_timer_start(s->timer))return 0;
        if(!s->timer)return 0;
    }else cui_timer_stop(s->timer);
    s->remaining=timeout/1000.0;s->last=cui_time();s->paused=0;s->event=CUI_FEEDBACK_NONE;
    cui_set_text(s->badge,badge);cui_set_role(s->badge,tone);cui_set_visible(s->badge,s->kind!=CUI_EMPTY_STATE);
    part_text(s,CUI_FEEDBACK_TITLE,title);part_text(s,CUI_FEEDBACK_MESSAGE,message);part_text(s,CUI_FEEDBACK_ACTION_BUTTON,label);
    cui_set_visible(s->parts[3],s->kind==CUI_BANNER||s->kind==CUI_TOAST);
    cui_set_visible(w,1);
    cui_widget *announcement=s->parts[CUI_FEEDBACK_MESSAGE];
    const char *spoken=message;
    if(announcement->hidden){announcement=s->parts[CUI_FEEDBACK_TITLE];spoken=title;}
    cui__backend_announce(announcement,spoken?spoken:"",tone==CUI_ROLE_DANGER);
    return 1;
}
void cui_feedback_pause(cui_widget *w,int value)
{feedback_state *s=state(w);if(s){s->paused=!!value;s->last=cui_time();}}
int cui_feedback_is_visible(const cui_widget *w)
{ return state(w)&&!w->hidden; }
cui_feedback_event cui_feedback_last_event(const cui_widget *w)
{feedback_state *s=state(w);return s?s->event:CUI_FEEDBACK_NONE;}
cui_widget *cui_feedback_get_part(cui_widget *w,cui_feedback_part part)
{feedback_state *s=state(w);return s&&part>=0&&part<4?s->parts[part]:NULL;}
