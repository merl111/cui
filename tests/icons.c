#include "cui_internal.h"
#include <gtk/gtk.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
static int calls;
static cui_widget *raster_view;
static void action(cui_widget *w,void *data){(void)w;(void)data;++calls;}
static void finish(void *data)
{
    GtkWidget *view=GTK_WIDGET(raster_view->native);
    GdkPaintable *paintable=gtk_widget_paintable_new(view);GtkSnapshot *snapshot=gtk_snapshot_new();
    gdk_paintable_snapshot(paintable,snapshot,96,96);GskRenderNode *node=gtk_snapshot_free_to_node(snapshot);assert(node);
    GdkTexture *texture=gsk_renderer_render_texture(gtk_native_get_renderer(gtk_widget_get_native(view)),node,NULL);assert(texture);
#if GTK_CHECK_VERSION(4,20,0)
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#endif
    GdkPixbuf *pixels=gdk_pixbuf_get_from_texture(texture);assert(pixels);
#if GTK_CHECK_VERSION(4,20,0)
    G_GNUC_END_IGNORE_DEPRECATIONS
#endif
    const unsigned char *center=gdk_pixbuf_read_pixels(pixels)+gdk_pixbuf_get_rowstride(pixels)*(gdk_pixbuf_get_height(pixels)/2)+(gdk_pixbuf_get_width(pixels)/2)*gdk_pixbuf_get_n_channels(pixels);
    assert(center[0]>200 && center[1]<70 && center[2]<120); /* deferred Cairo replay preserves raster bytes */
    g_object_unref(pixels);g_object_unref(texture);gsk_render_node_unref(node);g_object_unref(paintable);
    cui_app_quit(data);
}
int main(void)
{
    cui_icon_command commands[]={
        {CUI_ICON_MOVE,{1,1},0,0},{CUI_ICON_CUBIC,{1,20,23,20,23,1},0,0},
        {CUI_ICON_CLOSE,{0},0,0},{CUI_ICON_FILL,{1},0xea5344ff,0},
        {CUI_ICON_MOVE,{2,12},0,0},{CUI_ICON_LINE,{22,12},0,0},
        {CUI_ICON_STROKE,{2,1,1},0,1}};
    assert(!cui_icon_vector(NAN,24,commands,7));
    assert(!cui_icon_vector(24,24,commands,2));
    assert(!cui_icon_decode("CUIICON1",8));
    assert(!cui_icon_rgba(NULL,24,24));
    cui_icon_asset *a=cui_icon_vector(24,24,commands,7);assert(a);
    commands[0].values[0]=99;assert(a->commands[0].values[0]==1);
    cui_app *app=cui_app_create();assert(app);
    cui_window *window=cui_window_create(app,"Icon integration",620,380);
    cui_widget *root=cui_window_root(window);
    cui_widget *row=cui_box(root,CUI_HORIZONTAL,12);
    cui_widget *view=cui_icon(row,a),*button=cui_icon_button(row,a,"Play track");
    assert(view&&button&&a->refs==3);
    cui_icon_release(a);a=cui_get_icon(button);assert(a&&a->refs==2);
    assert(cui_set_icon(button,a));assert(a->refs==2); /* self assignment */
    assert(cui_set_icon_size(view,96));assert(!cui_set_icon_size(view,513));
    assert(!cui_set_icon_size(root,24));assert(!cui_set_icon(root,a));
    assert(!cui_icon_button(root,a,""));
    char label[64];cui_set_text(button,"Pause track");
    assert(cui_get_text(button,label,sizeof(label))==11&&!strcmp(label,"Pause track"));
    cui_set_text(button,"");cui_get_text(button,label,sizeof(label));assert(!strcmp(label,"Pause track"));
    cui_on_action(button,action,NULL);assert(cui_activate(button)&&calls==1);
    cui_set_enabled(button,0);assert(!cui_activate(button)&&calls==1);cui_set_enabled(button,1);
    assert(cui_set_icon_only(button,0));assert(cui_set_icon(button,NULL));
    cui_get_text(button,label,sizeof(label));assert(!strcmp(label,"Pause track"));
    cui_widget *toggle=cui_toggle(root,"Repeat",0);assert(cui_set_icon(toggle,a));assert(cui_set_icon_only(toggle,1));
    cui_set_checked(toggle,1);assert(cui_get_checked(toggle));
    unsigned char pixel[]={255,40,80,128};a=cui_icon_rgba(pixel,1,1);assert(a);
    assert(cui_set_icon(button,a));raster_view=cui_icon(row,a);assert(cui_set_icon_size(raster_view,96));cui_icon_release(a);assert(cui_set_icon_only(button,1));
    for(int symbol=CUI_SYMBOL_PLAY;symbol<CUI_SYMBOL_COUNT;symbol++){
        a=cui_icon_symbol((cui_symbol)symbol);assert(a);cui_icon_asset *copy=cui_icon_retain(a);assert(copy==a);cui_icon_release(copy);cui_icon_release(a);
    }
    cui_app_set_theme(app,CUI_THEME_DARK);cui_window_show(window);
    cui_every(app,100,finish,app);cui_app_run(app);cui_app_destroy(app);
    puts("Icon assets, ownership, validation, labels and native controls passed");return 0;
}
