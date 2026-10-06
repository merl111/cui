#include "cui_gtk.h"
#include "cui_layouts.h"
typedef struct {
    int extent, start_visible, end_visible, interacting;
    GdkFrameClock *clock;
    gulong handler;
} split_state;
static void split_free(gpointer data)
{
    split_state *s=data;
    if(s->clock){
        if(g_signal_handler_is_connected(s->clock,s->handler))g_signal_handler_disconnect(s->clock,s->handler);
        g_object_unref(s->clock);
    }
    g_free(s);
}
static int split_extent(cui_widget *w)
{return w->axis==CUI_HORIZONTAL?gtk_widget_get_width(GTK_WIDGET(w->native)):gtk_widget_get_height(GTK_WIDGET(w->native));}
static void split_changed(GObject *object,GParamSpec *spec,gpointer data)
{
    cui_widget *w=data;(void)object;(void)spec;
    split_state *state=g_object_get_data(G_OBJECT(w->native),"cui-split");
    /* GtkPaned also notifies while allocating, clamping and hiding children.
     * Those changes must not replace the application's preferred fraction. */
    if(w->updating||!state->interacting)return;
    int size=split_extent(w);
    if(size>0){w->value=(double)gtk_paned_get_position(GTK_PANED(w->native))/size;cui__emit(w);}
}
static gboolean split_pointer(GtkEventControllerLegacy *controller,GdkEvent *event,gpointer data)
{
    (void)controller;split_state *s=data;
    if(gdk_event_get_event_type(event)==GDK_BUTTON_PRESS)s->interacting=1;
    if(gdk_event_get_event_type(event)==GDK_BUTTON_RELEASE)s->interacting=0;
    return FALSE;
}
static gboolean split_key(GtkEventControllerKey *key,guint val,guint code,GdkModifierType mods,gpointer data)
{(void)key;(void)val;(void)code;(void)mods;((split_state*)data)->interacting=1;return FALSE;}
static void split_key_up(GtkEventControllerKey *key,guint val,guint code,GdkModifierType mods,gpointer data)
{(void)key;(void)val;(void)code;(void)mods;((split_state*)data)->interacting=0;}
static void split_layout(GdkFrameClock *clock,GtkWidget *native)
{
    (void)clock;cui_widget *w=g_object_get_data(G_OBJECT(native),"cui-split-widget");split_state *s=g_object_get_data(G_OBJECT(native),"cui-split");
    GtkWidget *a=gtk_paned_get_start_child(GTK_PANED(native)),*b=gtk_paned_get_end_child(GTK_PANED(native));
    int size=split_extent(w),av=a&&gtk_widget_get_visible(a),bv=b&&gtk_widget_get_visible(b);
    if(size!=s->extent||av!=s->start_visible||bv!=s->end_visible){
        s->extent=size;s->start_visible=av;s->end_visible=bv;
        if(size>0&&av&&bv){++w->updating;cui__backend_split_position(w);--w->updating;}
    }
}
static void split_mapped(GtkWidget *native,gpointer data)
{
    cui_widget *w=data;split_state *s=g_object_get_data(G_OBJECT(native),"cui-split");
    GdkFrameClock *clock=gtk_widget_get_frame_clock(native);
    if(clock!=s->clock){
        if(s->clock){g_signal_handler_disconnect(s->clock,s->handler);g_object_unref(s->clock);}
        s->clock=g_object_ref(clock);
        s->handler=g_signal_connect_object(clock,"after-paint",G_CALLBACK(split_layout),native,0);
    }
    s->extent=0;
    ++w->updating;cui__backend_split_position(w);--w->updating;
}
GtkWidget *cui__gtk_container(cui_widget *w)
{
    if(w->kind==CUI_STACK)return gtk_overlay_new();
    if(w->kind==CUI_GRID)return gtk_grid_new();
    if(w->kind==CUI_WRAP){GtkWidget *flow=gtk_flow_box_new();gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow),GTK_SELECTION_NONE);gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow),65535);return flow;}
    GtkWidget *paned=gtk_paned_new(w->axis==CUI_HORIZONTAL?GTK_ORIENTATION_HORIZONTAL:GTK_ORIENTATION_VERTICAL);
    split_state *state=g_new0(split_state,1);
    g_object_set_data_full(G_OBJECT(paned),"cui-split",state,split_free);
    g_object_set_data(G_OBJECT(paned),"cui-split-widget",w);
    GtkEventController *pointer=gtk_event_controller_legacy_new();
    gtk_event_controller_set_propagation_phase(pointer,GTK_PHASE_CAPTURE);
    g_signal_connect(pointer,"event",G_CALLBACK(split_pointer),state);
    gtk_widget_add_controller(paned,pointer);
    GtkEventController *keys=gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys,GTK_PHASE_CAPTURE);
    g_signal_connect(keys,"key-pressed",G_CALLBACK(split_key),state);
    g_signal_connect(keys,"key-released",G_CALLBACK(split_key_up),state);
    gtk_widget_add_controller(paned,keys);
    g_object_set_data(G_OBJECT(paned),"cui-split-keys",keys);
    gtk_paned_set_wide_handle(GTK_PANED(paned),TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned),FALSE);gtk_paned_set_shrink_end_child(GTK_PANED(paned),FALSE);
    gtk_widget_set_size_request(paned,320,200);
    g_signal_connect(paned,"notify::position",G_CALLBACK(split_changed),w);g_signal_connect(paned,"map",G_CALLBACK(split_mapped),w);return paned;
}
void cui__gtk_append(cui_widget *parent,cui_widget *child)
{
    GtkWidget *native=GTK_WIDGET(child->native);
    if(parent->kind==CUI_STACK){
        if(!parent->first)gtk_overlay_set_child(GTK_OVERLAY(parent->native),native);
        else gtk_overlay_add_overlay(GTK_OVERLAY(parent->native),native);
    }else if(parent->kind==CUI_GRID){
        gtk_grid_attach(GTK_GRID(parent->native),native,(int)child->grid_column,(int)child->grid_row,1,1);
    }else if(parent->kind==CUI_WRAP)gtk_flow_box_insert(GTK_FLOW_BOX(parent->native),native,-1);
    else if(parent->kind==CUI_SPLIT){if(!parent->first)gtk_paned_set_start_child(GTK_PANED(parent->native),native);else gtk_paned_set_end_child(GTK_PANED(parent->native),native);}
    else gtk_box_append(GTK_BOX(parent->native),native);
}
void cui__backend_container(cui_widget *w)
{
    if(w->parent&&w->parent->kind==CUI_STACK){
        GtkWidget *native=GTK_WIDGET(w->native);
        int align=w->layer_alignment;
        gtk_widget_set_halign(native,align==CUI_LAYER_FILL?GTK_ALIGN_FILL:align==CUI_LAYER_BOTTOM_RIGHT?GTK_ALIGN_END:GTK_ALIGN_CENTER);
        gtk_widget_set_valign(native,align==CUI_LAYER_FILL?GTK_ALIGN_FILL:align==CUI_LAYER_TOP?GTK_ALIGN_START:align==CUI_LAYER_CENTER?GTK_ALIGN_CENTER:GTK_ALIGN_END);
        gtk_widget_set_hexpand(native,align==CUI_LAYER_FILL);gtk_widget_set_vexpand(native,align==CUI_LAYER_FILL);
        gtk_widget_set_margin_start(native,w->layer_margin);gtk_widget_set_margin_end(native,w->layer_margin);
        gtk_widget_set_margin_top(native,w->layer_margin);gtk_widget_set_margin_bottom(native,w->layer_margin);
        gtk_widget_set_size_request(native,w->layer_width?w->layer_width:-1,w->layer_height?w->layer_height:-1);
    }
    if(w->kind==CUI_GRID){gtk_grid_set_row_spacing(GTK_GRID(w->native),w->gap);gtk_grid_set_column_spacing(GTK_GRID(w->native),w->gap);gtk_grid_set_column_homogeneous(GTK_GRID(w->native),TRUE);}
    else if(w->kind==CUI_WRAP){gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(w->native),w->gap);gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(w->native),w->gap);}
}
void cui__backend_grid_cell(cui_widget *w)
{
    g_object_ref(w->native);gtk_grid_remove(GTK_GRID(w->parent->native),GTK_WIDGET(w->native));
    gtk_grid_attach(GTK_GRID(w->parent->native),GTK_WIDGET(w->native),(int)w->grid_column,(int)w->grid_row,(int)w->grid_column_span,(int)w->grid_row_span);g_object_unref(w->native);
}
void cui__backend_split_position(cui_widget *w)
{
    int size=w->axis==CUI_HORIZONTAL?gtk_widget_get_width(GTK_WIDGET(w->native)):gtk_widget_get_height(GTK_WIDGET(w->native));
    if(size>0)gtk_paned_set_position(GTK_PANED(w->native),(int)(size*w->value));
}
