#include "cui_desktop_internal.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#define CHECK(x) do{if(!(x)){fprintf(stderr,"desktop:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static cui_app *app;static cui_window *window;static cui_widget *entry,*editor,*toolbar;static cui_command *command;
static cui_dialog *dialog;static cui_timer *timer;static int calls,phase,ticks,results;
static const char *fixture="/tmp/cui-desktop-open-fixture.txt";
static void invoked(void *data){(void)data;++calls;}
static void result(cui_dialog *d,cui_dialog_result response,const char *path,void *data)
{
    (void)data;++results;
    if(d->kind==CUI_DIALOG_ALERT)CHECK(response==CUI_DIALOG_ACCEPTED);
    else if(d->kind==CUI_DIALOG_OPEN&&!d->cancelled){CHECK(response==CUI_DIALOG_ACCEPTED);CHECK(!strcmp(path,fixture));}
    else CHECK(response==CUI_DIALOG_CANCELLED);
}
static void tick(void *data)
{
    (void)data;CHECK(++ticks<100);
    if(phase==0){
        CHECK(cui_focus(entry));CHECK(cui_has_focus(entry));
        cui_accessibility(entry,"Project name","A UTF-8 name for this project");
        cui_set_read_only(entry,1);CHECK(!gtk_editable_get_editable(GTK_EDITABLE(entry->native)));cui_set_enabled(entry,0);cui_set_enabled(entry,1);CHECK(!gtk_editable_get_editable(GTK_EDITABLE(entry->native)));cui_set_read_only(entry,0);
        GtkTextBuffer *buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor->aux));
        gtk_text_buffer_begin_user_action(buffer);gtk_text_buffer_insert_at_cursor(buffer," world",-1);gtk_text_buffer_end_user_action(buffer);
        CHECK(gtk_text_buffer_get_can_undo(buffer));cui_undo(editor);char text[64];cui_get_text(editor,text,sizeof(text));CHECK(!strcmp(text,"Hello"));
        cui_redo(editor);cui_get_text(editor,text,sizeof(text));CHECK(!strcmp(text,"Hello world"));
        CHECK(cui_command_invoke(command)&&calls==1);cui_command_set_enabled(command,0);
        CHECK(!cui_command_invoke(command));CHECK(!cui_activate(toolbar->first));
        cui_command_set_enabled(command,1);CHECK(cui_activate(toolbar->first)&&calls==2);
        cui_command_set_checked(command,1);CHECK(cui_get_checked(toolbar->first));
        GActionGroup *group=G_ACTION_GROUP(app->desktop_native);char name[32];snprintf(name,sizeof(name),"command%u",command->id);
        g_action_group_activate_action(group,name,NULL);CHECK(calls==3);
        GtkEventController *keys=g_object_get_data(G_OBJECT(window->native),"cui-shortcuts");gboolean handled=FALSE;
        g_signal_emit_by_name(keys,"key-pressed",GDK_KEY_k,0,GDK_CONTROL_MASK,&handled);CHECK(handled&&calls==4);
        dialog=cui_alert(window,"Confirm","Native alert callback","Continue",result,NULL);CHECK(dialog);phase=1;return;
    }
    if(!dialog->native&&!dialog->finished)return;
    if(phase==1){gtk_dialog_response(GTK_DIALOG(dialog->native),GTK_RESPONSE_ACCEPT);CHECK(results==1);
        dialog=cui_file_dialog(window,CUI_DIALOG_OPEN,"Open",fixture,result,NULL);CHECK(dialog);phase=2;return;}
    if(phase==2){
        GFile *file=gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog->native));if(!file)return;g_object_unref(file);
        GListModel *windows=gtk_window_get_toplevels();gboolean accepted=FALSE;
        for(guint i=0;i<g_list_model_get_n_items(windows);++i){GtkWindow *native=g_list_model_get_item(windows,i);
            if(GTK_IS_FILE_CHOOSER(native)&&GTK_IS_DIALOG(native)){gtk_dialog_response(GTK_DIALOG(native),GTK_RESPONSE_ACCEPT);accepted=TRUE;}
            g_object_unref(native);
        }
        if(!accepted)return;
        if(results!=2)return;
        dialog=cui_file_dialog(window,CUI_DIALOG_SAVE,"Save","/tmp/new-document.txt",result,NULL);CHECK(dialog);phase=3;return;
    }
    if(phase==3){cui_dialog_cancel(dialog);CHECK(results==3);dialog=cui_file_dialog(window,CUI_DIALOG_FOLDER,"Folder","/tmp",result,NULL);CHECK(dialog);phase=4;return;}
    if(phase==4){cui_dialog_cancel(dialog);CHECK(results==4);dialog=cui_file_dialog(window,CUI_DIALOG_OPEN,"Cancel before opening",NULL,result,NULL);cui_dialog_cancel(dialog);phase=5;return;}
    CHECK(dialog->finished&&results==5);cui_timer_stop(timer);cui_app_quit(app);
}
int main(void)
{
    g_set_prgname("cui-desktop-test");
    CHECK(g_file_set_contents(fixture,"CUI test fixture",-1,NULL));
    app=cui_app_create();CHECK(app);window=cui_window_create(app,"Desktop integration",640,480);CHECK(window);
    command=cui_command_create(app,"Run action",'K',CUI_MOD_PRIMARY,invoked,NULL);CHECK(command);
    cui_menu *bar=cui_menu_create(app),*file=cui_menu_create(app);CHECK(cui_menu_add(file,command));CHECK(cui_menu_add_submenu(bar,"File",file));CHECK(!cui_menu_add_submenu(file,"Cycle",bar));
    cui_window_set_menu(window,bar);CHECK(window->menu_native);
    CHECK(g_menu_model_get_n_items(gtk_popover_menu_bar_get_menu_model(GTK_POPOVER_MENU_BAR(window->menu_native)))==1);
    CHECK(!cui_menu_add(bar,command));
    toolbar=cui_toolbar(cui_window_root(window),&command,1);CHECK(toolbar);
    entry=cui_entry(cui_window_root(window),"Project");editor=cui_textarea(cui_window_root(window),"Hello");
    timer=cui_every(app,100,tick,NULL);CHECK(timer);cui_window_show(window);cui_app_run(app);cui_app_destroy(app);
    remove(fixture);puts("desktop: dialogs, menus, shortcuts, command state, focus, read-only, undo/redo passed");return 0;
}
G_GNUC_END_IGNORE_DEPRECATIONS
