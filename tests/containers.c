#include "cui_internal.h"
#include "cui_layouts.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"containers:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static cui_app *app;static cui_window *window;static cui_widget *split,*grid,*wrap,*stack,*overlay,*base,*overlay_button;static int changes;
static void changed(cui_widget *sender,void *data){(void)sender;(void)data;++changes;}
static void verify(void *data)
{
    cui_timer_stop((cui_timer *)data);
    CHECK(GTK_IS_PANED(split->native)&&GTK_IS_GRID(grid->native)&&GTK_IS_FLOW_BOX(wrap->native));
    CHECK(gtk_widget_get_width(GTK_WIDGET(split->first->native))>0);
    CHECK(gtk_widget_get_width(GTK_WIDGET(split->last->native))>0);
    CHECK(GTK_IS_OVERLAY(stack->native));
    CHECK(gtk_overlay_get_child(GTK_OVERLAY(stack->native))==base->native);
    CHECK(gtk_widget_get_width(GTK_WIDGET(overlay->native))>=220);
    CHECK(cui_stack_layer(stack,(cui_layer_alignment)99,0,0,0)==NULL);
    CHECK(cui_stack_layer(stack,CUI_LAYER_CENTER,-1,0,0)==NULL);
    cui_set_enabled(base,0);CHECK(cui_activate(overlay_button));
    cui_set_visible(overlay,0);CHECK(!cui_activate(overlay_button));
    cui_set_visible(overlay,1);CHECK(cui_activate(overlay_button));
    cui_set_enabled(base,1);
    changes=0;cui_split_set_position(split,0.6);CHECK(changes==0);
    gpointer keys=g_object_get_data(G_OBJECT(split->native),"cui-split-keys");
    gboolean handled=FALSE;
    g_signal_emit_by_name(keys,"key-pressed",GDK_KEY_Right,0,0,&handled);
    gtk_paned_set_position(GTK_PANED(split->native),180);CHECK(changes>0&&cui_split_get_position(split)>0);
    g_signal_emit_by_name(keys,"key-released",GDK_KEY_Right,0,0);
    CHECK(cui_grid_cell(grid,0,0,1,1)==NULL);
    CHECK(cui_grid_cell(grid,0,2,1,1)==NULL);
    CHECK(cui_split_pane(split,2)==NULL);
    cui_app_quit(app);
}
static cui_timer *timer;
static void tick(void *data){(void)data;verify(timer);}
int main(void)
{
    app=cui_app_create();CHECK(app);window=cui_window_create(app,"Container integration",1000,700);CHECK(window);
    stack=cui_stack(cui_window_root(window));CHECK(stack);
    base=cui_stack_layer(stack,CUI_LAYER_FILL,0,0,0);CHECK(base);
    overlay=cui_stack_layer(stack,CUI_LAYER_CENTER,220,100,12);CHECK(overlay);
    overlay_button=cui_button(overlay,"Overlay action");cui_on_action(overlay_button,changed,NULL);
    split=cui_split(base,CUI_HORIZONTAL,0.35);CHECK(split);cui_on_action(split,changed,NULL);
    grid=cui_grid(cui_split_pane(split,0),2,12);CHECK(grid);
    cui_label(cui_grid_cell(grid,0,0,1,2),"Spanning heading");
    cui_button(cui_grid_cell(grid,1,0,1,1),"Left");cui_button(cui_grid_cell(grid,1,1,1,1),"Right");
    wrap=cui_wrap(cui_split_pane(split,1),8);CHECK(wrap);
    for(int i=0;i<12;++i)cui_button(wrap,"Wrap item");
    timer=cui_every(app,200,tick,NULL);cui_window_show(window);cui_app_run(app);cui_app_destroy(app);
    puts("containers: grid spans, overlap rejection, native wrapping, splitter position and callbacks passed");return 0;
}
