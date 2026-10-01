#include "cui_internal.h"
#include "cui_desktop.h"
#include "cui_search.h"
#include "cui_feedback.h"
#include "cui_tables.h"
#include "cui_tokens.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"search_feedback:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static cui_app *app;static cui_widget *picker,*palette,*toast,*outside;
static cui_widget *tokens;static int token_events[6];
static int events[5],feedback[4],ticks,phase;static double started;
static int press(cui_widget *w,guint key)
{
    gboolean handled=FALSE;gpointer controller=g_object_get_data(w->native,"cui-keys");CHECK(controller);
    g_signal_emit_by_name(controller,"key-pressed",key,0,0,&handled);return handled;
}
static void selected(cui_widget *w,void *data)
{(void)data;++events[cui_picker_last_event(w)];}
static void notified(cui_widget *w,void *data)
{
    (void)data;cui_feedback_event event=cui_feedback_last_event(w);++feedback[event];
    if(event==CUI_FEEDBACK_TIMEOUT&&feedback[event]==1)
        CHECK(cui_feedback_show(w,"Again","Reopened by timeout callback",CUI_ROLE_SUCCESS,"",120));
}
static void check_picker(void)
{
    char label[]="Café";cui_choice choices[]={{91,label,"Coffee","drink",0},{92,"Disabled","Unavailable","",1},{93,"Ελληνικά","Greek","language",0},{94,"Zebra","Animal","",0}};
    CHECK(cui_picker_set_items(picker,choices,4));label[0]='X';
    cui_choice duplicate[]={{1,"a","","",0},{1,"b","","",0}};
    CHECK(!cui_picker_set_items(picker,duplicate,2));CHECK(cui_picker_match_count(picker)==4);
    CHECK(!cui_picker_set_items(picker,NULL,1));CHECK(!cui_picker_select(picker,92));
    cui_widget *input=cui_picker_get_part(picker,CUI_PICKER_INPUT);
    gtk_editable_set_text(GTK_EDITABLE(input->native),"CAFÉ");CHECK(events[CUI_PICKER_QUERY]==1);CHECK(cui_picker_match_count(picker)==1);
    CHECK(cui_picker_is_open(picker));
    GtkEditable *delegate=gtk_editable_get_delegate(GTK_EDITABLE(input->native));CHECK(GTK_IS_TEXT(delegate));
    g_signal_emit_by_name(delegate,"preedit-changed","候補");CHECK(!press(input,GDK_KEY_Return));CHECK(!events[CUI_PICKER_SELECT]);
    g_signal_emit_by_name(delegate,"preedit-changed","");CHECK(press(input,GDK_KEY_Return));CHECK(cui_picker_selected(picker)==91);
    char text[64];cui_picker_get_query(picker,text,sizeof(text));CHECK(!strcmp(text,"Café"));
    CHECK(!cui_picker_is_open(picker));CHECK(events[CUI_PICKER_SELECT]==1);
    CHECK(cui_picker_set_query(picker,""));cui_picker_open(picker,outside);
    CHECK(press(input,GDK_KEY_Down));CHECK(press(input,GDK_KEY_Return));CHECK(cui_picker_selected(picker)==93);CHECK(cui_has_focus(outside));
    CHECK(cui_picker_set_query(picker,"language"));CHECK(cui_picker_match_count(picker)==1);
    CHECK(cui_picker_set_query(picker,"ΕΛΛΗΝΙΚΆ"));CHECK(cui_picker_match_count(picker)==1);
    CHECK(cui_picker_set_query(picker,"Custom value"));cui_picker_open(picker,outside);CHECK(press(input,GDK_KEY_Return));CHECK(events[CUI_PICKER_SUBMIT]==1);CHECK(!cui_picker_selected(picker));
    CHECK(cui_picker_set_query(picker,"drink"));cui_picker_open(picker,NULL);CHECK(!press(input,GDK_KEY_Tab));CHECK(cui_picker_selected(picker)==91);
    CHECK(cui_picker_set_query(picker,""));cui_picker_open(picker,outside);CHECK(press(input,GDK_KEY_Escape));CHECK(events[CUI_PICKER_CANCEL]==1);
    CHECK(cui_picker_set_items(palette,choices,4));CHECK(!cui_picker_is_open(palette));cui_picker_open(palette,outside);
    CHECK(cui_picker_set_query(palette,"missing"));CHECK(!cui_picker_accept(palette));
    CHECK(cui_picker_set_query(palette,""));cui_widget *results=cui_picker_get_part(palette,CUI_PICKER_RESULTS);
    CHECK(cui_table_sort(results,0,1,0));
    CHECK(press(cui_picker_get_part(palette,CUI_PICKER_INPUT),GDK_KEY_Down));CHECK(cui_picker_accept(palette));CHECK(cui_picker_selected(palette)==93);
    /* Replacing a large choice model retains copied IDs and virtualized rows. */
    size_t n=10000;cui_choice *many=calloc(n,sizeof(*many));CHECK(many);
    for(size_t i=0;i<n;++i)many[i]=(cui_choice){i+1,"Item","Shared label","",0};
    CHECK(cui_picker_set_query(palette,""));CHECK(cui_picker_set_items(palette,many,n));free(many);
    CHECK(cui_picker_match_count(palette)==n);CHECK(cui_picker_select(palette,10000));CHECK(cui_picker_selected(palette)==10000);
    CHECK(cui_picker_set_items(palette,NULL,0));CHECK(!cui_picker_selected(palette));
    cui_picker_open(palette,outside);cui_focus(outside); /* Focus monitor must close without stealing focus. */
}
static void token_changed(cui_widget *w,void *data)
{(void)data;++token_events[cui_tokens_last_event(w)];}
static size_t widget_count(cui_widget *w)
{size_t n=1;for(cui_widget *child=w->first;child;child=child->next)n+=widget_count(child);return n;}
static void check_tokens(void)
{
    cui_choice items[]={{11,"Design","Interface","ux",0},{12,"世界","International","locale",0},{13,"Read only","Disabled","",1},{14,"Code","Implementation","dev",0}};
    CHECK(cui_tokens_set_items(tokens,items,4));cui_item_id ids[]={11,12};
    CHECK(cui_tokens_set_selected(tokens,ids,2));CHECK(!token_events[CUI_TOKENS_ADD]);
    CHECK(!cui_tokens_add(tokens,14));CHECK(!cui_tokens_add(tokens,11));
    ids[1]=11;CHECK(!cui_tokens_set_selected(tokens,ids,2));CHECK(cui_tokens_get_selected(tokens,ids,2)==2&&ids[1]==12);
    size_t allocated=widget_count(tokens);
    for(int i=0;i<50;++i){CHECK(cui_tokens_set_selected(tokens,NULL,0));CHECK(cui_tokens_set_selected(tokens,ids,2));}
    CHECK(widget_count(tokens)==allocated);CHECK(!cui_tokens_remove_button(tokens,2));
    cui_widget *input=cui_tokens_get_part(tokens,CUI_TOKENS_INPUT),*picker=cui_tokens_get_part(tokens,CUI_TOKENS_PICKER);
    CHECK(press(input,GDK_KEY_BackSpace));CHECK(cui_tokens_changed(tokens)==12&&token_events[CUI_TOKENS_REMOVE]==1);
    CHECK(cui_picker_match_count(picker)==3);CHECK(!cui_tokens_add(tokens,13));
    CHECK(cui_tokens_add(tokens,12));CHECK(cui_tokens_changed(tokens)==12&&token_events[CUI_TOKENS_ADD]==1);
    cui_widget *remove=cui_tokens_remove_button(tokens,0);cui_focus(remove);CHECK(cui_activate(remove));CHECK(cui_has_focus(input));
    CHECK(cui_tokens_get_selected(tokens,ids,2)==1&&ids[0]==12);
    CHECK(cui_picker_set_query(picker,"ux"));cui_picker_open(picker,NULL);CHECK(press(input,GDK_KEY_Return));
    CHECK(token_events[CUI_TOKENS_ADD]==2);CHECK(cui_tokens_get_selected(tokens,ids,2)==2&&ids[1]==11);
    CHECK(cui_activate(cui_tokens_get_part(tokens,CUI_TOKENS_CLEAR_BUTTON)));CHECK(token_events[CUI_TOKENS_CLEAR]==1);CHECK(!cui_tokens_clear(tokens));
    gtk_editable_set_text(GTK_EDITABLE(input->native),"New label");CHECK(token_events[CUI_TOKENS_QUERY]==1);CHECK(press(input,GDK_KEY_Return));CHECK(token_events[CUI_TOKENS_SUBMIT]==1);
    CHECK(cui_tokens_set_selected(tokens,ids,2));items[0].disabled=1;CHECK(cui_tokens_set_items(tokens,items,4));
    CHECK(cui_tokens_get_selected(tokens,ids,2)==1&&ids[0]==12);CHECK(token_events[CUI_TOKENS_REMOVE]==2);
    cui_set_enabled(tokens,0);CHECK(!cui_tokens_remove(tokens,12));CHECK(!press(input,GDK_KEY_BackSpace));cui_set_enabled(tokens,1);
    CHECK(cui_tokens_clear(tokens));CHECK(!cui_tokens_set_items(tokens,NULL,1));
    cui_picker_close(picker);cui_focus(outside);
}
static void tick(void *data)
{
    (void)data;CHECK(++ticks<120);
    if(phase==0){
        cui_choice choices[1000];for(size_t i=0;i<1000;++i)choices[i]=(cui_choice){i+1,"Result","Scroll to highlighted row","",0};
        CHECK(cui_picker_set_items(palette,choices,1000));cui_picker_open(palette,outside);phase=10;return;
    }
    if(phase==10){
        cui_widget *input=cui_picker_get_part(palette,CUI_PICKER_INPUT);
        for(int i=0;i<200;++i)CHECK(press(input,GDK_KEY_Down));
        phase=11;return;
    }
    if(phase==11){
        cui_widget *table=cui_picker_get_part(palette,CUI_PICKER_RESULTS);
        GtkAdjustment *scroll=gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(table->native));
        CHECK(cui_get_selected(table)==200);CHECK(gtk_adjustment_get_value(scroll)>0);
        cui_picker_close(palette);CHECK(cui_picker_set_items(palette,NULL,0));
        check_tokens();check_picker();CHECK(cui_feedback_show(toast,"Saved","Persistent model",CUI_ROLE_SUCCESS,"Undo",120));
        cui_feedback_pause(toast,1);started=cui_time();phase=1;return;
    }
    if(phase==1&&cui_time()-started>.35){
        CHECK(cui_feedback_is_visible(toast));CHECK(!feedback[CUI_FEEDBACK_TIMEOUT]);CHECK(!cui_picker_is_open(palette));CHECK(cui_has_focus(outside));
        CHECK(cui_activate(cui_feedback_get_part(toast,CUI_FEEDBACK_ACTION_BUTTON)));CHECK(feedback[CUI_FEEDBACK_ACTION]==1);CHECK(cui_feedback_is_visible(toast));
        cui_focus(cui_feedback_get_part(toast,CUI_FEEDBACK_ACTION_BUTTON));cui_feedback_pause(toast,0);started=cui_time();phase=2;return;
    }
    if(phase==2&&cui_time()-started>.35){
        CHECK(cui_feedback_is_visible(toast));CHECK(!feedback[CUI_FEEDBACK_TIMEOUT]);cui_focus(outside);phase=3;return;
    }
    if(phase==3&&feedback[CUI_FEEDBACK_TIMEOUT]==2){
        CHECK(!cui_feedback_is_visible(toast));CHECK(cui_feedback_show(toast,"Dismiss","",CUI_ROLE_WARNING,"",0));
        CHECK(cui_activate(cui_feedback_get_part(toast,CUI_FEEDBACK_DISMISS_BUTTON)));CHECK(feedback[CUI_FEEDBACK_DISMISS]==1);CHECK(!cui_feedback_is_visible(toast));
        CHECK(!cui_feedback_show(toast,"Invalid","",CUI_ROLE_CARD,"",100));
        cui_app_quit(app);
    }
}
int main(void)
{
    app=cui_app_create();CHECK(app);cui_window *window=cui_window_create(app,"Search and feedback",780,900);CHECK(window);
    const char *scale=getenv("CUI_TEXT_SCALE");if(scale)CHECK(cui_app_set_text_scale(app,atof(scale)));
    cui_window_set_scrollable(window,1);cui_widget *root=cui_window_root(window);cui_box_set_padding(root,24);
    outside=cui_button(root,"Outside focus");picker=cui_picker(root,CUI_AUTOCOMPLETE,"Find a suggestion");palette=cui_picker(root,CUI_COMMAND_PALETTE,"Find a command");toast=cui_feedback(root,CUI_TOAST);
    tokens=cui_tokens(root,"Choose labels…",2);CHECK(tokens);cui_on_action(tokens,token_changed,NULL);
    CHECK(picker&&palette&&toast);cui_on_action(picker,selected,NULL);cui_on_action(toast,notified,NULL);
    CHECK(cui_every(app,100,tick,NULL));cui_window_show(window);cui_app_run(app);cui_app_destroy(app);
    puts("search/feedback: copied models, keyboard/IME, Unicode search, focus, reusable expiry and pause passed");return 0;
}
