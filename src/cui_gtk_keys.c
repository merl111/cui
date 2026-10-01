#include "cui_desktop_internal.h"
#include <gtk/gtk.h>
static void preedit(GtkText *text,const char *value,gpointer data)
{(void)text;((cui_widget *)data)->composing=value&&*value;}
static gboolean pressed(GtkEventControllerKey *controller,guint key,guint code,GdkModifierType flags,gpointer data)
{
    (void)controller;(void)code;cui_key value;
    switch(key){
    case GDK_KEY_Return:case GDK_KEY_KP_Enter:value=CUI_KEY_ENTER;break;
    case GDK_KEY_Escape:value=CUI_KEY_ESCAPE;break;
    case GDK_KEY_BackSpace:value=CUI_KEY_BACKSPACE;break;
    case GDK_KEY_Tab:case GDK_KEY_ISO_Left_Tab:value=CUI_KEY_TAB;break;
    case GDK_KEY_Up:case GDK_KEY_KP_Up:value=CUI_KEY_UP;break;
    case GDK_KEY_Down:case GDK_KEY_KP_Down:value=CUI_KEY_DOWN;break;
    case GDK_KEY_Home:value=CUI_KEY_HOME;break;
    case GDK_KEY_End:value=CUI_KEY_END;break;
    case GDK_KEY_Page_Up:value=CUI_KEY_PAGE_UP;break;
    case GDK_KEY_Page_Down:value=CUI_KEY_PAGE_DOWN;break;
    default:return FALSE;
    }
    if(flags&(GDK_SUPER_MASK|GDK_HYPER_MASK|GDK_META_MASK))return FALSE;
    unsigned mods=0;
    if(flags&GDK_SHIFT_MASK)mods|=CUI_MOD_SHIFT;
    if(flags&GDK_ALT_MASK)mods|=CUI_MOD_ALT;
    if(flags&GDK_CONTROL_MASK)mods|=CUI_MOD_CONTROL|CUI_MOD_PRIMARY;
    return cui__key(data,value,mods);
}
int cui__backend_keys(cui_widget *w)
{
    if(g_object_get_data(w->native,"cui-keys"))return 1;
    GtkEventController *keys=gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys,GTK_PHASE_CAPTURE);
    g_signal_connect(keys,"key-pressed",G_CALLBACK(pressed),w);gtk_widget_add_controller(w->native,keys);
    g_object_set_data(w->native,"cui-keys",keys);
    GtkEditable *editable=GTK_IS_EDITABLE(w->native)?gtk_editable_get_delegate(GTK_EDITABLE(w->native)):NULL;
    if(editable && GTK_IS_TEXT(editable))g_signal_connect(editable,"preedit-changed",G_CALLBACK(preedit),w);
    if(w->kind==CUI_TEXTAREA)g_signal_connect(w->aux,"preedit-changed",G_CALLBACK(preedit),w);
    return 1;
}
int cui__backend_hover(const cui_widget *w)
{
    GtkNative *native=gtk_widget_get_native(GTK_WIDGET(w->native));if(!native)return 0;
    GdkSurface *surface=gtk_native_get_surface(native);
    GdkSeat *seat=gdk_display_get_default_seat(gdk_surface_get_display(surface));
    GdkDevice *pointer=seat?gdk_seat_get_pointer(seat):NULL;
    double x,y;if(!pointer || !gdk_surface_get_device_position(surface,pointer,&x,&y,NULL))return 0;
    double offset_x,offset_y;gtk_native_get_surface_transform(native,&offset_x,&offset_y);
    graphene_point_t from={(float)(x+offset_x),(float)(y+offset_y)},to;
    if(!gtk_widget_compute_point(GTK_WIDGET(native),GTK_WIDGET(w->native),&from,&to))return 0;
    return to.x>=0&&to.y>=0&&to.x<gtk_widget_get_width(w->native)&&to.y<gtk_widget_get_height(w->native);
}
void cui__backend_announce(cui_widget *w,const char *text,int urgent)
{
#if GTK_CHECK_VERSION(4,14,0)
    gtk_accessible_announce(GTK_ACCESSIBLE(w->native),text,urgent?GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_HIGH:GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM);
#else
    (void)urgent;cui_accessibility(w,text,"");
#endif
}

char *cui__search_key(const char *text)
{
    if(!g_utf8_validate(text,-1,NULL))return NULL;
    char *folded=g_utf8_casefold(text,-1),*normalized=g_utf8_normalize(folded,-1,G_NORMALIZE_ALL_COMPOSE);
    g_free(folded);char *copy=normalized?cui__desktop_copy(normalized):NULL;g_free(normalized);return copy;
}
