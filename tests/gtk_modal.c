#include "cui_internal.h"
#include "cui_layouts.h"
#include "cui_desktop.h"
#include <gtk/gtk.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"modal:%d: %s\n",__LINE__,#x);exit(1);} } while(0)
static cui_app *app;
static cui_window *window;
static cui_widget *backdrop,*sheet;
static int clicks;
static void dismiss(cui_widget *w,void *data){(void)w;(void)data;++clicks;}
static void background(cui_widget *w,int alpha)
{
    GtkSnapshot *snapshot=gtk_snapshot_new();
    gtk_snapshot_render_background(snapshot,gtk_widget_get_style_context(w->native),0,0,64,64);
    GskRenderNode *node=gtk_snapshot_free_to_node(snapshot);CHECK(node);
    graphene_rect_t bounds=GRAPHENE_RECT_INIT(0,0,64,64);
    GdkTexture *texture=gsk_renderer_render_texture(gtk_native_get_renderer(GTK_NATIVE(window->native)),node,&bounds);CHECK(texture);
    unsigned char pixels[64*64*4];gdk_texture_download(texture,pixels,64*4);
    CHECK(abs((int)pixels[(32*64+32)*4+3]-alpha)<=1);
    g_object_unref(texture);gsk_render_node_unref(node);
}
static void verify(void *data)
{
    (void)data;
    background(backdrop,102);background(sheet,255);
    graphene_rect_t bounds;CHECK(gtk_widget_compute_bounds(sheet->native,window->native,&bounds));
    GtkWidget *picked=gtk_widget_pick(window->native,bounds.origin.x+10,bounds.origin.y+10,GTK_PICK_DEFAULT);
    CHECK(picked==sheet->native||gtk_widget_is_ancestor(picked,sheet->native));
    CHECK(gtk_widget_compute_bounds(backdrop->native,window->native,&bounds));
    GtkWidget *outside=gtk_widget_pick(window->native,bounds.origin.x+5,bounds.origin.y+5,GTK_PICK_DEFAULT);
    CHECK(outside==backdrop->native||gtk_widget_is_ancestor(outside,backdrop->native));
    int before=clicks;g_signal_emit_by_name(backdrop->native,"clicked");CHECK(clicks==before+1);
    GdkRGBA color;gtk_widget_get_color(sheet->native,&color);CHECK(fabs(color.red-0x12/255.)<.01);
    CHECK(!strcmp(localeconv()->decimal_point,","));
    cui_window *popup=cui_window_create(app,"Popup",160,90);CHECK(popup);
    CHECK(!cui_window_popup_at(popup,sheet,0,0,20,20));
    CHECK(cui_window_set_frame(popup,0,0,16));
    CHECK(!cui_window_popup_region(popup,sheet,1));
    CHECK(!cui_window_popup_at(popup,sheet,0,0,0,20));
    CHECK(cui_window_popup_at(popup,sheet,0,0,20,20));
    CHECK(cui_window_is_visible(popup));
    CHECK(gtk_popover_get_autohide(GTK_POPOVER(popup->attached_native)));
    CHECK(gtk_popover_get_position(GTK_POPOVER(popup->attached_native))==GTK_POS_BOTTOM);
    cui_window_close(window);CHECK(!cui_window_is_visible(popup));
    cui_window_show(window);
    cui_app_quit(app);
}
int main(void)
{
    app=cui_app_create();CHECK(app);
    if(!setlocale(LC_NUMERIC,"de_AT.UTF-8")&&!setlocale(LC_NUMERIC,"de_DE.UTF-8")){cui_app_destroy(app);return 77;}
    window=cui_window_create(app,"Modal regression",640,480);CHECK(window);
    cui_widget *stack=cui_stack(cui_window_root(window));CHECK(stack);
    CHECK(!cui_stack_backdrop(stack,"Close"));
    CHECK(cui_stack_layer(stack,CUI_LAYER_FILL,0,0,0));
    backdrop=cui_stack_backdrop(stack,"Close dialog");CHECK(backdrop);cui_on_action(backdrop,dismiss,NULL);
    sheet=cui_stack_layer(stack,CUI_LAYER_CENTER,240,160,20);CHECK(sheet);
    cui_widget_style style={0};
    style.background=0xfafeffff;style.foreground=0x123456ff;style.radius=23.5;style.border_width=1.25;style.border=0x12345680;
    CHECK(cui_set_style(sheet,&style));
    cui_window_show(window);cui_every(app,250,verify,NULL);cui_app_run(app);
    cui_app_set_theme(app,CUI_THEME_DARK);cui_app_run(app);
    cui_app_destroy(app);puts("modal: locale-safe opaque surface, dimming and layer hit testing passed");return 0;
}
