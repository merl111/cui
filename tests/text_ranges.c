#include "cui_internal.h"
#include "cui_layouts.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"text_ranges:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int changes;
static void changed(cui_widget *w, void *data) { (void)w; (void)data; ++changes; }
int main(void)
{
    cui_app *app=cui_app_create(); CHECK(app);
    cui_window *window=cui_window_create(app,"Text ranges",600,400); CHECK(window);
    cui_widget *split=cui_split(cui_window_root(window),CUI_HORIZONTAL,0.6); CHECK(split);
    cui_widget *pane=cui_split_pane(split,0); CHECK(pane);
    CHECK(cui_split_set_scrollable(split,0,1));
    CHECK(cui_split_set_scrollable(split,0,1));
    CHECK(cui_split_set_scrollable(split,0,0));
    CHECK(cui_split_set_scrollable(split,0,1));
    CHECK(cui_split_pane(split,0)==pane);
    CHECK(!cui_split_set_scrollable(split,2,1));
    CHECK(!cui_split_set_scrollable(pane,0,1));
    cui_widget *text=cui_textarea(pane,"Élan ✨. Again. Again."); CHECK(text);
    cui_on_action(text,changed,NULL);
    size_t start=0,end=0;
    const char *value="Élan ✨. Again. Again.";
    size_t second=(size_t)(strrchr(value,'A')-value);
    CHECK(cui_text_set_range(text,second,second+6));
    CHECK(cui_text_get_range(text,&start,&end) && start==second && end==second+6);
    CHECK(!cui_text_set_range(text,1,2));
    CHECK(cui_text_get_range(text,&start,&end) && start==second && end==second+6);
    CHECK(!cui_text_set_range(text,0,strlen(value)+1));
    CHECK(!cui_text_get_range(text,NULL,&end));
    CHECK(cui_text_set_range(text,strlen(value),strlen(value)));
    CHECK(cui_text_get_range(text,&start,&end) && start==strlen(value) && end==start);
    CHECK(cui_text_set_emphasis(text,0,strlen("Élan"),3));
    GtkTextBuffer *buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(text->aux));
    GtkTextIter iter; gtk_text_buffer_get_start_iter(buffer,&iter);
    GtkTextTagTable *table=gtk_text_buffer_get_tag_table(buffer);
    CHECK(gtk_text_iter_has_tag(&iter,gtk_text_tag_table_lookup(table,"cui-emphasis")));
    CHECK(gtk_text_iter_has_tag(&iter,gtk_text_tag_table_lookup(table,"cui-strong")));
    CHECK(!cui_text_set_emphasis(text,0,2,9));
    CHECK(cui_text_set_emphasis(text,0,strlen(value),0));
    CHECK(!gtk_text_iter_has_tag(&iter,gtk_text_tag_table_lookup(table,"cui-emphasis")));
    CHECK(cui_text_set_spacing(text,8,12));
    CHECK(gtk_text_view_get_pixels_inside_wrap(GTK_TEXT_VIEW(text->aux))==8);
    CHECK(gtk_text_view_get_pixels_below_lines(GTK_TEXT_VIEW(text->aux))==12);
    CHECK(!cui_text_set_spacing(text,-1,12));
    CHECK(!cui_text_set_spacing(text,8,65));
    CHECK(!cui_text_set_spacing(cui_label(cui_window_root(window),"label"),8,12));
    CHECK(cui_text_decorate(text,0,strlen("Élan"),0xa02020ff,0xffeeeeff,3));
    GtkTextTag *decoration=gtk_text_tag_table_lookup(table,"cui-decoration-0");
    CHECK(decoration && gtk_text_iter_has_tag(&iter,decoration));
    CHECK(!cui_text_decorate(text,1,2,0,0,0));
    CHECK(gtk_text_iter_has_tag(&iter,decoration));
    CHECK(cui_text_decorate(text,0,strlen(value),0,0,0));
    CHECK(!gtk_text_iter_has_tag(&iter,decoration));
    CHECK(!cui_text_decorate(text,0,2,0,0,4));
    CHECK(changes==0);
    CHECK(cui_text_set_native_undo(text,0));
    gtk_text_buffer_insert_at_cursor(buffer," More.",-1);
    CHECK(changes==1 && !gtk_text_buffer_get_can_undo(buffer));
    CHECK(cui_text_set_native_undo(text,1));
    CHECK(!gtk_text_buffer_get_can_undo(buffer));
    CHECK(cui_text_set_emphasis(text,0,strlen("Élan"),2));
    CHECK(!gtk_text_buffer_get_can_undo(buffer));
    CHECK(!cui_text_get_range(cui_label(cui_window_root(window),"label"),&start,&end));
    cui_window_show(window);
    for(int i=0;i<15;++i){while(g_main_context_iteration(NULL,FALSE));g_usleep(10000);}
    double rect[4]={-1,-1,-1,-1};
    CHECK(cui_text_rect(text,0,rect));CHECK(rect[2]>0 && rect[3]>0);
    CHECK(!cui_text_rect(text,1,rect));
    CHECK(!cui_text_pointer(NULL,&start,rect));
    cui_window_set_close_deferred(window,1);
    cui_window_close(window);
    CHECK(cui_window_close_requested(window));
    CHECK(gtk_widget_get_visible(GTK_WIDGET(window->native)));
    cui_window_set_close_deferred(window,1);
    CHECK(!cui_window_close_requested(window));
    cui_window_set_close_deferred(window,0);
    cui_window_close(window);
    CHECK(!gtk_widget_get_visible(GTK_WIDGET(window->native)));
    cui_app_destroy(app);
    puts("text ranges: UTF-8 offsets, repeated text, emphasis, undo ownership, close deferral passed");
    return 0;
}
