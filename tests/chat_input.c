#include "cui_internal.h"
#include "cui_desktop.h"
#include "cui_draw.h"
#include <gtk/gtk.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
static int sends,cancels;
static int keys(cui_widget *w,cui_key key,unsigned mods,void *data)
{
    (void)w;(void)data;
    if(key==CUI_KEY_ENTER && !(mods&CUI_MOD_SHIFT)){++sends;return 1;}
    if(key==CUI_KEY_ESCAPE){++cancels;return 1;}
    return 0;
}
static int press(cui_widget *w,guint key,GdkModifierType mods)
{
    gboolean handled=FALSE;
    gpointer controller=g_object_get_data(w->native,"cui-keys");assert(controller);
    g_signal_emit_by_name(controller,"key-pressed",key,0,mods,&handled);
    return handled;
}
static cui_app *app;static cui_widget *input,*canvas;
static void verify(void *data)
{
    (void)data;
    int width=0,height=0;
    int sized=cui_widget_get_size(input,&width,&height);
    /* GTK reports the content box, excluding the scrolled editor border. */
    assert(sized&&width>0&&height>0);
    int requested=0;gtk_widget_get_size_request(input->native,NULL,&requested);assert(requested==48);
    assert(cui_widget_get_size(canvas,&width,&height));
    assert(press(input,GDK_KEY_Return,0)&&sends==1);
    assert(!press(input,GDK_KEY_Return,GDK_SHIFT_MASK)&&sends==1);
    g_signal_emit_by_name(input->aux,"preedit-changed","日本語");
    assert(!press(input,GDK_KEY_Return,0)&&sends==1);
    g_signal_emit_by_name(input->aux,"preedit-changed","");
    assert(press(input,GDK_KEY_Return,0)&&sends==2);
    assert(press(input,GDK_KEY_Escape,0)&&cancels==1);
    cui_set_enabled(input,0);assert(!press(input,GDK_KEY_Return,0));cui_set_enabled(input,1);
    cui_set_visible(input,0);assert(!press(input,GDK_KEY_Return,0));cui_set_visible(input,1);
    assert(cui_on_key(input,NULL,NULL));assert(!press(input,GDK_KEY_Return,0));
    double narrow=0,tall=0,wide=0,large=0;
    assert(cui_text_measure("Hello",NULL,14,400,&narrow,&tall));
    assert(cui_text_measure("Hello",NULL,28,400,&wide,&large));
    assert(wide>narrow&&large>tall);
    assert(cui_text_measure("日本語 café",NULL,14,400,&wide,&large)&&wide>0);
    double before=wide;assert(!cui_text_measure("x",NULL,NAN,400,&wide,&large)&&wide==before);
    assert(!cui_text_measure(NULL,NULL,14,400,&wide,&large));
    assert(!cui_widget_get_size(NULL,&width,&height));
    cui_app_quit(app);
}
int main(void)
{
    app=cui_app_create();assert(app);
    cui_window *w=cui_window_create(app,"Chat input integration",600,480);
    input=cui_textarea(cui_window_root(w),"");assert(input);
    assert(cui_textarea_set_height(input,48));assert(!cui_textarea_set_height(input,0));
    assert(cui_on_key(input,keys,NULL));
    canvas=cui_canvas(cui_window_root(w));assert(canvas);
    assert(!cui_textarea_set_height(canvas,48));assert(cui_on_key(canvas,keys,NULL));
    cui_every(app,200,verify,NULL);cui_window_show(w);cui_app_run(app);cui_app_destroy(app);
    puts("chat_input: native textarea Enter/Shift+Enter/Escape, IME, disabled/hidden, dimensions and text measurement passed");
    return 0;
}
