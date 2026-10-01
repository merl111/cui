#include "cui_internal.h"
int cui_on_key(cui_widget *w,cui_key_callback callback,void *userdata)
{
    if(!w || (w->kind!=CUI_ENTRY && w->kind!=CUI_SEARCH && w->kind!=CUI_PASSWORD && w->kind!=CUI_TEXTAREA && w->kind!=CUI_CANVAS && w->kind!=CUI_LIST && w->kind!=CUI_TABLE))return 0;
    if(callback && !cui__backend_keys(w))return 0;
    w->key_callback=callback;w->key_userdata=userdata;return 1;
}
int cui__key(cui_widget *w,cui_key key,unsigned modifiers)
{
    if(!w || !w->key_callback || w->updating || w->composing || w->window->app->destroying)return 0;
    for(cui_widget *p=w;p;p=p->parent)if(p->hidden || !p->enabled)return 0;
    return !!w->key_callback(w,key,modifiers,w->key_userdata);
}
