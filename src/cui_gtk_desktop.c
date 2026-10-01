#include "cui_desktop_internal.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
/* Retain GTK 4.6 compatibility; these native dialogs remain available in GTK 4. */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static GtkWidget *input(cui_widget *w)
{return GTK_WIDGET((w->kind==CUI_TEXTAREA||w->kind==CUI_CODE||w->kind==CUI_LIST||w->kind==CUI_TABLE||w->kind==CUI_TREE||w->kind==CUI_SWITCH)?w->aux:w->native);}
int cui_focus(cui_widget *w){if(!w)return 0;for(cui_widget *p=w;p;p=p->parent)if(p->hidden||!p->enabled)return 0;return gtk_widget_grab_focus(input(w));}
int cui_has_focus(const cui_widget *w)
{
    if(!w)return 0;
    GtkWidget *focused=gtk_window_get_focus(GTK_WINDOW(w->window->native));
    return focused&&(focused==GTK_WIDGET(w->native)||gtk_widget_is_ancestor(focused,GTK_WIDGET(w->native)));
}
void cui_accessibility(cui_widget *w,const char *label,const char *description)
{if(w)gtk_accessible_update_property(GTK_ACCESSIBLE(input(w)),GTK_ACCESSIBLE_PROPERTY_LABEL,label?label:"",GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,description?description:"",-1);}
void cui_set_read_only(cui_widget *w,int read_only)
{
    if(!w || !(w->kind==CUI_ENTRY || w->kind==CUI_PASSWORD || w->kind==CUI_SEARCH || w->kind==CUI_TEXTAREA))return;
    w->read_only=!!read_only;
    if(w->kind==CUI_TEXTAREA)gtk_text_view_set_editable(GTK_TEXT_VIEW(w->aux),!read_only);
    else if(GTK_IS_EDITABLE(w->native))gtk_editable_set_editable(GTK_EDITABLE(w->native),!read_only);
}
void cui_undo(cui_widget *w)
{
    if(!w)return;
    if(w->kind==CUI_TEXTAREA){GtkTextBuffer *b=gtk_text_view_get_buffer(GTK_TEXT_VIEW(w->aux));if(gtk_text_buffer_get_can_undo(b))gtk_text_buffer_undo(b);}
    else gtk_widget_activate_action(input(w),"text.undo",NULL);
}
void cui_redo(cui_widget *w)
{
    if(!w)return;
    if(w->kind==CUI_TEXTAREA){GtkTextBuffer *b=gtk_text_view_get_buffer(GTK_TEXT_VIEW(w->aux));if(gtk_text_buffer_get_can_redo(b))gtk_text_buffer_redo(b);}
    else gtk_widget_activate_action(input(w),"text.redo",NULL);
}
static void alert_response(GtkDialog *native,int response,gpointer data)
{
    cui_dialog *d=data;g_signal_handlers_disconnect_by_data(native,d);gtk_window_destroy(GTK_WINDOW(native));d->native=NULL;
    cui__desktop_finish(d,response==GTK_RESPONSE_ACCEPT?CUI_DIALOG_ACCEPTED:CUI_DIALOG_CANCELLED,"");
}
void cui__desktop_open(cui_dialog *d)
{
    if(d->kind==CUI_DIALOG_COLOR || d->kind==CUI_DIALOG_FONT){cui__picker_open(d);return;}
    if(d->kind==CUI_DIALOG_ALERT){
        GtkWidget *alert=gtk_message_dialog_new(GTK_WINDOW(d->parent->native),GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,GTK_BUTTONS_NONE,"%s",d->title);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(alert),"%s",d->message);
        gtk_dialog_add_buttons(GTK_DIALOG(alert),"Cancel",GTK_RESPONSE_CANCEL,d->accept,GTK_RESPONSE_ACCEPT,NULL);
        gtk_dialog_set_default_response(GTK_DIALOG(alert),GTK_RESPONSE_ACCEPT);
        d->native=alert;g_signal_connect(alert,"response",G_CALLBACK(alert_response),d);gtk_window_present(GTK_WINDOW(alert));return;
    }
    cui__file_open_native(d);
}
void cui__desktop_cancel(cui_dialog *d)
{
    if(!d->native)return;
    if(d->kind==CUI_DIALOG_COLOR || d->kind==CUI_DIALOG_FONT){cui__picker_cancel(d);return;}
    g_signal_handlers_disconnect_by_data(d->native,d);
    if(d->kind==CUI_DIALOG_ALERT)gtk_window_destroy(GTK_WINDOW(d->native));
    else{gtk_native_dialog_destroy(GTK_NATIVE_DIALOG(d->native));g_object_unref(d->native);}
    d->native=NULL;cui__desktop_finish(d,CUI_DIALOG_CANCELLED,"");
}
static void command_action(GSimpleAction *action,GVariant *value,gpointer data)
{(void)action;(void)value;cui_command_invoke(data);}
void cui__desktop_command(cui_command *c)
{
    if(!c->app->desktop_native)c->app->desktop_native=g_simple_action_group_new();
    char name[32];snprintf(name,sizeof(name),"command%u",c->id);
    if(c->native && !!g_action_get_state_type(G_ACTION(c->native)) != !!c->checkable){g_action_map_remove_action(G_ACTION_MAP(c->app->desktop_native),name);c->native=NULL;}
    if(!c->native){c->native=c->checkable?g_simple_action_new_stateful(name,NULL,g_variant_new_boolean(c->checked)):g_simple_action_new(name,NULL);
        g_signal_connect(c->native,"activate",G_CALLBACK(command_action),c);g_action_map_add_action(G_ACTION_MAP(c->app->desktop_native),G_ACTION(c->native));g_object_unref(c->native);}

    g_simple_action_set_enabled(G_SIMPLE_ACTION(c->native),c->enabled);
    if(c->checkable)g_simple_action_set_state(G_SIMPLE_ACTION(c->native),g_variant_new_boolean(c->checked));
}
static gboolean key_pressed(GtkEventControllerKey *controller,guint key,guint code,GdkModifierType modifiers,gpointer data)
{
    cui_window *w=data;(void)controller;(void)code;
    unsigned mods=0;if(modifiers&GDK_SHIFT_MASK)mods|=CUI_MOD_SHIFT;if(modifiers&GDK_ALT_MASK)mods|=CUI_MOD_ALT;if(modifiers&GDK_CONTROL_MASK)mods|=CUI_MOD_CONTROL;
    key=gdk_keyval_to_upper(key);
    if(key==GDK_KEY_Escape)key=CUI_KEY_ESCAPE;
    else if(key==GDK_KEY_Return||key==GDK_KEY_KP_Enter)key=CUI_KEY_ENTER;
    else if(key==GDK_KEY_Tab)key=CUI_KEY_TAB;
    for(cui_command *c=w->app->commands;c;c=c->next){unsigned expected=(c->modifiers&~CUI_MOD_PRIMARY)|((c->modifiers&CUI_MOD_PRIMARY)?CUI_MOD_CONTROL:0);
        if(c->key&&key==c->key&&mods==expected)return cui_command_invoke(c);}
    return FALSE;
}
void cui__desktop_window(cui_window *w)
{
    if(w->app->desktop_native)gtk_widget_insert_action_group(GTK_WIDGET(w->native),"cui",G_ACTION_GROUP(w->app->desktop_native));
    if(!g_object_get_data(G_OBJECT(w->native),"cui-shortcuts")){
        GtkEventController *keys=gtk_event_controller_key_new();gtk_event_controller_set_propagation_phase(keys,GTK_PHASE_CAPTURE);
        g_signal_connect(keys,"key-pressed",G_CALLBACK(key_pressed),w);gtk_widget_add_controller(GTK_WIDGET(w->native),keys);
        g_object_set_data(G_OBJECT(w->native),"cui-shortcuts",keys);
    }
}
static GMenu *menu_model(cui_menu *menu)
{
    GMenu *model=g_menu_new(),*section=g_menu_new();
    for(size_t i=0;i<menu->count;++i){cui_menu_item *item=menu->items+i;
        if(item->submenu){GMenu *child=menu_model(item->submenu);g_menu_append_submenu(section,item->label,G_MENU_MODEL(child));g_object_unref(child);}
        else if(item->command){char name[40];snprintf(name,sizeof(name),"cui.command%u",item->command->id);GMenuItem *entry=g_menu_item_new(item->command->label,name);
            if(!item->command->checkable)g_menu_item_set_attribute(entry,"role","s","normal");
            g_menu_append_item(section,entry);g_object_unref(entry);
        }else{if(g_menu_model_get_n_items(G_MENU_MODEL(section)))g_menu_append_section(model,NULL,G_MENU_MODEL(section));g_object_unref(section);section=g_menu_new();}
    }
    if(g_menu_model_get_n_items(G_MENU_MODEL(section)))g_menu_append_section(model,NULL,G_MENU_MODEL(section));
    g_object_unref(section);return model;
}
void cui__desktop_menu(cui_window *w)
{
    cui__desktop_window(w);
    if(w->menu_native){gtk_box_remove(GTK_BOX(w->root->native),GTK_WIDGET(w->menu_native));w->menu_native=NULL;}
    if(!w->menu)return;
    GMenu *model=g_menu_new();
    for(size_t i=0;i<w->menu->count;++i){
        cui_menu_item *item=w->menu->items+i;
        GMenu *child=menu_model(item->submenu);
        g_menu_append_submenu(model,item->label,G_MENU_MODEL(child));g_object_unref(child);
    }
    w->menu_native=gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(model));g_object_unref(model);
    gtk_box_prepend(GTK_BOX(w->root->native),GTK_WIDGET(w->menu_native));
}
void cui_menu_popup(cui_menu *m,cui_widget *anchor)
{
    if(!m||!anchor||m->app!=anchor->window->app)return;
    if(m->native){gtk_widget_unparent(GTK_WIDGET(m->native));g_object_unref(m->native);}
    GMenu *model=menu_model(m);GtkWidget *popup=gtk_popover_menu_new_from_model(G_MENU_MODEL(model));g_object_unref(model);
    g_object_ref_sink(popup);m->native=popup;gtk_widget_set_parent(popup,GTK_WIDGET(anchor->native));gtk_popover_popup(GTK_POPOVER(popup));
}
void cui__desktop_dispose(cui_app *app)
{
    for(cui_menu *m=app->menus;m;m=m->next)if(m->native){if(gtk_widget_get_parent(GTK_WIDGET(m->native)))gtk_widget_unparent(GTK_WIDGET(m->native));g_object_unref(m->native);}
    if(app->desktop_native)g_object_unref(app->desktop_native);
}

G_GNUC_END_IGNORE_DEPRECATIONS
