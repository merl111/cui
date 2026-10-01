#include "cui_desktop_internal.h"
#include "cui_draw_internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
int cui_menu_popup_at(cui_menu *menu, cui_widget *anchor,
                     double x, double y, double width, double height)
{
    if (!menu || !anchor || menu->app != anchor->window->app ||
        !isfinite(x) || !isfinite(y) || !isfinite(width) || !isfinite(height) ||
        fabs(x)>1e6 || fabs(y)>1e6 || width<=0 || height<=0 || width>1e6 || height>1e6)
        return 0;
    for (cui_widget *w=anchor; w; w=w->parent)
        if (w->hidden || !w->enabled) return 0;
    return cui__desktop_popup_at(menu,anchor,x,y,width,height);
}
int cui_menu_popup_region(cui_menu *menu, cui_widget *canvas, unsigned region)
{
    cui_canvas_state *state=cui__canvas_state(canvas);
    if (!state || !region) return 0;
    for (size_t i=0;i<state->count;++i) {
        const cui_canvas_region *r=state->regions+i;
        if (r->id==region && r->enabled)
            return cui_menu_popup_at(menu,canvas,r->x,r->y,r->width,r->height);
    }
    return 0;
}
int cui_window_popup_at(cui_window *panel,cui_widget *anchor,double x,double y,double width,double height)
{
    if(!panel||!anchor||panel->decorated||panel->app!=anchor->window->app||
       !isfinite(x)||!isfinite(y)||!isfinite(width)||!isfinite(height)||
       fabs(x)>1e6||fabs(y)>1e6||width<=0||height<=0||width>1e6||height>1e6)return 0;
    for(cui_window *w=anchor->window;w;w=w->anchor_parent)if(w==panel)return 0;
    for(cui_widget *w=anchor;w;w=w->parent)if(w->hidden||!w->enabled)return 0;
    panel->popup=1;
    if(!cui__backend_popup_anchor(panel,anchor,x,y,width,height))return 0;
    cui_window_show(panel);return 1;
}
int cui_window_popup_region(cui_window *panel,cui_widget *canvas,unsigned region)
{
    cui_canvas_state *state=cui__canvas_state(canvas);
    if(!state||!region)return 0;
    for(size_t i=0;i<state->count;++i){
        const cui_canvas_region *r=state->regions+i;
        if(r->id==region&&r->enabled)return cui_window_popup_at(panel,canvas,r->x,r->y,r->width,r->height);
    }
    return 0;
}
char *cui__desktop_copy(const char *text)
{
    if (!text) text = "";
    char *copy = (char *)malloc(strlen(text)+1);
    if (copy) strcpy(copy,text);
    return copy;
}
void cui__desktop_finish(cui_dialog *d, cui_dialog_result result, const char *path)
{
    if (!d || d->finished) return;
    d->finished = 1; d->result = result;
    if (d->callback && !d->parent->app->destroying) d->callback(d, result, path ? path : "", d->userdata);
}
static void start_dialog(void *data)
{
    cui_dialog *d = (cui_dialog *)data;
    cui_timer_stop(d->timer);
    if (d->cancelled) cui__desktop_finish(d,CUI_DIALOG_CANCELLED,"");
    else cui__desktop_open(d);
}
cui_dialog *cui__dialog_create(cui_window *parent, cui_dialog_kind kind, const char *title,
    const char *path, const char *message, const char *accept, cui_dialog_callback callback, void *data)
{
    if (!parent || parent->app->destroying || !callback || kind < CUI_DIALOG_OPEN || kind > CUI_DIALOG_FONT) return NULL;
    cui_dialog *d = (cui_dialog *)calloc(1,sizeof(*d)); if (!d) return NULL;
    d->references=1;d->parent=parent; d->kind=kind; d->callback=callback; d->userdata=data;
    d->title=cui__desktop_copy(title); d->path=cui__desktop_copy(path);
    d->message=cui__desktop_copy(message); d->accept=cui__desktop_copy(accept);
    if (!d->title || !d->path || !d->message || !d->accept) {
        free(d->title); free(d->path); free(d->message); free(d->accept); free(d); return NULL;
    }
    d->timer=cui_every(parent->app,10,start_dialog,d);
    if (!d->timer) {free(d->title);free(d->path);free(d->message);free(d->accept);free(d);return NULL;}
    d->next=parent->app->dialogs; parent->app->dialogs=d; return d;
}
cui_dialog *cui_file_dialog(cui_window *parent, cui_dialog_kind kind, const char *title,
    const char *initial_path, cui_dialog_callback callback, void *userdata)
{
    if (kind < CUI_DIALOG_OPEN || kind > CUI_DIALOG_FOLDER) return NULL;
    return cui__dialog_create(parent,kind,title,initial_path,NULL,NULL,callback,userdata);
}
cui_dialog *cui_alert(cui_window *parent, const char *title, const char *message,
    const char *accept, cui_dialog_callback callback, void *userdata)
{ return cui__dialog_create(parent,CUI_DIALOG_ALERT,title,NULL,message,accept ? accept : "OK",callback,userdata); }
void cui_dialog_cancel(cui_dialog *d)
{
    if (!d || d->finished) return;
    d->cancelled=1;
    if (d->native) cui__desktop_cancel(d);
}
int cui__font_valid(const cui_font_value *font)
{
    if (!font || !memchr(font->family, 0, sizeof(font->family)) || !*font->family ||
        !isfinite(font->points) || font->points < 6 || font->points > 200 ||
        font->weight < 100 || font->weight > 900 || (font->italic != 0 && font->italic != 1)) return 0;
    for (const unsigned char *p=(const unsigned char *)font->family; *p; ++p)
        if (*p < 32 || *p == '\\' || *p == '"') return 0;
    return 1;
}
cui_dialog *cui_color_dialog(cui_window *parent, const char *title, unsigned rgb,
    cui_dialog_callback callback, void *data)
{
    if (rgb > 0xffffff) return NULL;
    cui_dialog *d=cui__dialog_create(parent,CUI_DIALOG_COLOR,title,NULL,NULL,NULL,callback,data);
    if(d)d->color=rgb;
    return d;
}
cui_dialog *cui_font_dialog(cui_window *parent, const char *title, const cui_font_value *font,
    cui_dialog_callback callback, void *data)
{
    if(!cui__font_valid(font))return NULL;
    cui_dialog *d=cui__dialog_create(parent,CUI_DIALOG_FONT,title,NULL,NULL,NULL,callback,data);
    if(d)d->font=*font;
    return d;
}
int cui_dialog_color(const cui_dialog *d,unsigned *rgb)
{
    if(!d || !rgb || d->kind!=CUI_DIALOG_COLOR || !d->finished || d->result!=CUI_DIALOG_ACCEPTED)return 0;
    *rgb=d->color;return 1;
}
int cui_dialog_font(const cui_dialog *d,cui_font_value *font)
{
    if(!d || !font || d->kind!=CUI_DIALOG_FONT || !d->finished || d->result!=CUI_DIALOG_ACCEPTED)return 0;
    *font=d->font;return 1;
}
int cui_font_apply(cui_widget *w,const cui_font_value *font)
{
    if(!w || !cui__font_valid(font))return 0;
    int previous=w->font_style; w->font_style=font->italic?2:1;
    if(cui_set_font(w,font->family,font->points,font->weight))return 1;
    w->font_style=previous;return 0;
}
static void toolbar_states(cui_widget *w,cui_command *command)
{
    if(w->command==command){cui_set_enabled(w,command->enabled);cui_set_checked(w,command->checked);}
    for(cui_widget *child=w->first;child;child=child->next)toolbar_states(child,command);
}
static void update_command(cui_command *command)
{
    cui__desktop_command(command);
    for(cui_window *w=command->app->windows;w;w=w->next)toolbar_states(w->root,command);
}
static void refresh_menus(cui_app *app)
{ for (cui_window *w=app->windows;w;w=w->next) if(w->menu) cui__desktop_menu(w); }
cui_command *cui_command_create(cui_app *app, const char *label, unsigned key, unsigned modifiers, cui_task action, void *data)
{
    if (!app || !action || key > 127 || modifiers > 15 || app->next_command >= 50000) return NULL;
    cui_command *c=(cui_command *)calloc(1,sizeof(*c)); if(!c) return NULL;
    c->label=cui__desktop_copy(label); if(!c->label) {free(c);return NULL;}
    c->app=app;c->key=(unsigned)toupper((int)key);c->modifiers=modifiers;c->action=action;c->userdata=data;c->enabled=1;
    c->id=10000+app->next_command++;c->next=app->commands;app->commands=c;
    cui__desktop_command(c);
    for(cui_window *w=app->windows;w;w=w->next) cui__desktop_window(w);
    return c;
}
void cui_command_set_enabled(cui_command *c,int enabled)
{ if(c){c->enabled=!!enabled;update_command(c);refresh_menus(c->app);} }
void cui_command_set_checked(cui_command *c,int checked)
{ if(c){c->checkable=1;c->checked=!!checked;update_command(c);refresh_menus(c->app);} }
int cui_command_invoke(cui_command *c)
{
    if(!c||!c->enabled||c->app->destroying) return 0;
    c->action(c->userdata);return 1;
}
cui_menu *cui_menu_create(cui_app *app)
{
    if(!app)return NULL;
    cui_menu *m=(cui_menu *)calloc(1,sizeof(*m));if(!m)return NULL;
    m->app=app;m->next=app->menus;app->menus=m;return m;
}
static int contains_menu(cui_menu *root,cui_menu *target,unsigned depth)
{
    if(root==target || depth>64)return 1;
    for(size_t i=0;i<root->count;++i)if(root->items[i].submenu&&contains_menu(root->items[i].submenu,target,depth+1))return 1;
    return 0;
}
static int append_item(cui_menu *m,cui_command *c,cui_menu *child,const char *label)
{
    if(!m||m->count>=4096||(m->attached&&!child))return 0;
    if((c&&c->app!=m->app)||(child&&(child->app!=m->app||contains_menu(child,m,0))))return 0;
    char *copy=cui__desktop_copy(label);if(!copy)return 0;
    cui_menu_item *items=(cui_menu_item *)realloc(m->items,(m->count+1)*sizeof(*items));
    if(!items){free(copy);return 0;}
    m->items=items;m->items[m->count++]=(cui_menu_item){c,child,copy};refresh_menus(m->app);return 1;
}
int cui_menu_add(cui_menu *m,cui_command *c){return c&&append_item(m,c,NULL,NULL);}
int cui_menu_add_submenu(cui_menu *m,const char *label,cui_menu *child){return child&&append_item(m,NULL,child,label);}
int cui_menu_add_separator(cui_menu *m){return append_item(m,NULL,NULL,NULL);}
void cui_window_set_menu(cui_window *w,cui_menu *m)
{
    if(!w || (m&&w->app!=m->app))return;
    if(m){
        for(size_t i=0;i<m->count;++i)if(!m->items[i].submenu){w->app->error="A menu bar contains named submenus";return;}
        m->attached=1;
    }
    w->menu=m;cui__desktop_menu(w);
}
static void toolbar_action(cui_widget *sender,void *data){cui_command *command=data;cui_command_invoke(command);cui_set_checked(sender,command->checked);}
cui_widget *cui_toolbar(cui_widget *parent,cui_command *const *commands,size_t count)
{
    if(!parent||(count&&!commands))return NULL;
    for(size_t i=0;i<count;++i)if(!commands[i]||commands[i]->app!=parent->window->app)return NULL;
    cui_widget *bar=cui_box(parent,CUI_HORIZONTAL,8);if(!bar)return NULL;
    for(size_t i=0;i<count;++i){cui_widget *b=cui_toggle(bar,commands[i]->label,commands[i]->checked);if(!b)return NULL;b->command=commands[i];cui_set_enabled(b,commands[i]->enabled);cui_on_action(b,toolbar_action,commands[i]);}
    return bar;
}
void cui__desktop_destroy(cui_app *app)
{
    for(cui_dialog *d=app->dialogs;d;d=d->next){d->callback=NULL;if(d->native)cui__desktop_cancel(d);}
    /* Native objects keep references until backend disposal after windows close. */
}
void cui__dialog_retain(cui_dialog *d){++d->references;}
void cui__dialog_release(cui_dialog *d)
{
    if(--d->references)return;
    cui__file_settings_free(d->files);cui__free_strings(d->paths,d->path_count);
    free(d->title);free(d->path);free(d->message);free(d->accept);free(d);
}
void cui__desktop_free(cui_app *app)
{
    while(app->dialogs){cui_dialog *d=app->dialogs;app->dialogs=d->next;cui__dialog_release(d);}
    while(app->commands){cui_command *c=app->commands;app->commands=c->next;free(c->label);free(c);}
    while(app->menus){cui_menu *m=app->menus;app->menus=m->next;for(size_t i=0;i<m->count;++i)free(m->items[i].label);free(m->items);free(m);}
}

int cui__contains_focus(cui_widget *w)
{
    if(cui_has_focus(w))return 1;
    for(cui_widget *child=w->first;child;child=child->next)if(cui__contains_focus(child))return 1;
    return 0;
}
