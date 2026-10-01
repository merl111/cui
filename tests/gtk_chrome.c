#include "cui_internal.h"
#include "gtk_find.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"chrome:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static cui_app *app;
static cui_window *window;
static GtkWidget *reference;
static int is_close_button(GtkWidget *widget,const void *unused)
{ (void)unused; return GTK_IS_BUTTON(widget) && gtk_widget_has_css_class(widget,"close"); }
static void install_header(GtkWidget *target)
{
    GtkWidget *header=gtk_header_bar_new();
    gtk_header_bar_set_decoration_layout(GTK_HEADER_BAR(header),":minimize,maximize,close");
    gtk_window_set_titlebar(GTK_WINDOW(target),header);
}
static void verify(void *unused)
{
    (void)unused;
    GtkWidget *actual=cui_test_find_widget(GTK_WIDGET(window->native),is_close_button,NULL);
    GtkWidget *expected=cui_test_find_widget(reference,is_close_button,NULL);
    CHECK(actual && expected);
    GtkBorder a,b;
    gtk_style_context_get_padding(gtk_widget_get_style_context(actual),&a);
    gtk_style_context_get_padding(gtk_widget_get_style_context(expected),&b);
    CHECK(a.top==b.top && a.bottom==b.bottom && a.left==b.left && a.right==b.right);
    int aw,ah,bw,bh;
    gtk_widget_measure(actual,GTK_ORIENTATION_HORIZONTAL,-1,&aw,&ah,NULL,NULL);
    gtk_widget_measure(expected,GTK_ORIENTATION_HORIZONTAL,-1,&bw,&bh,NULL,NULL);
    CHECK(aw==bw && ah==bh);
    cui_app_quit(app);
}
int main(void)
{
    app=cui_app_create(); CHECK(app);
    window=cui_window_create(app,"CUI chrome",640,480); CHECK(window);
    install_header(GTK_WIDGET(window->native));
    reference=gtk_window_new(); install_header(reference); gtk_window_present(GTK_WINDOW(reference));
    cui_widget *root=cui_window_root(window);
    cui_set_role(root,CUI_ROLE_AMBIENT); CHECK(root->role==CUI_ROLE_AMBIENT);
    cui_widget *button=cui_button(root,"Application control");
    cui_set_role(button,CUI_ROLE_AMBIENT); CHECK(button->role==CUI_ROLE_BODY);
    CHECK(gtk_widget_has_css_class(GTK_WIDGET(root->native),"cui-content"));
    CHECK(gtk_widget_has_css_class(GTK_WIDGET(root->native),"cui-ambient"));
    cui_every(app,300,verify,NULL); cui_window_show(window); cui_app_run(app);
    cui_app_set_theme(app,CUI_THEME_DARK); cui_app_run(app);
    gtk_window_destroy(GTK_WINDOW(reference)); cui_app_destroy(app);
    puts("gtk chrome: native title-button sizing preserved in light/dark; ambient role validated");
    return 0;
}
