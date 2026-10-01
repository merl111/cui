#include "cui_internal.h"
#include "../examples/settings_ui.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)

static int actions;
static settings_demo demo;
static cui_window *second_window;

static void check_bounds(cui_widget *widget)
{
    cui_widget *child;
    graphene_rect_t bounds;
    GtkWidget *window = GTK_WIDGET(widget->window->native);
    CHECK(gtk_widget_compute_bounds(GTK_WIDGET(widget->native), window, &bounds));
    CHECK(bounds.origin.x >= -1 && bounds.origin.y >= -1);
    CHECK(bounds.origin.x + bounds.size.width <= gtk_widget_get_width(window) + 1);
    CHECK(bounds.origin.y + bounds.size.height <= gtk_widget_get_height(window) + 1);
    for (child = widget->first; child; child = child->next) check_bounds(child);
}

static void count_action(cui_widget *widget, void *data)
{ (void)widget; (void)data; ++actions; }

static void save_snapshot(const char *path)
{
    GtkWidget *widget = GTK_WIDGET(demo.window->native);
    GdkPaintable *paintable = gtk_widget_paintable_new(widget);
    GtkSnapshot *snapshot = gtk_snapshot_new();
    GskRenderNode *node;
    GdkTexture *texture;
    int width = gtk_widget_get_width(widget), height = gtk_widget_get_height(widget);
    CHECK(width >= 600 && height >= 600);
    gdk_paintable_snapshot(paintable, snapshot, width, height);
    node = gtk_snapshot_free_to_node(snapshot);
    CHECK(node != NULL);
    texture = gsk_renderer_render_texture(gtk_native_get_renderer(GTK_NATIVE(widget)), node, NULL);
    CHECK(texture != NULL);
    CHECK(gdk_texture_save_to_png(texture, path));
    g_object_unref(texture);
    gsk_render_node_unref(node);
    g_object_unref(paintable);
}

static gboolean close_windows(gpointer data)
{
    cui_app *app = (cui_app *)data;
    const char *path = g_getenv("CUI_SCREENSHOT");
    graphene_rect_t bounds;
    CHECK(gtk_widget_get_width(GTK_WIDGET(demo.name->native)) > 200);
    CHECK(gtk_widget_compute_bounds(GTK_WIDGET(demo.save->native), GTK_WIDGET(demo.window->native), &bounds));
    CHECK(bounds.size.height >= 30);
    CHECK(gtk_widget_get_scale_factor(GTK_WIDGET(demo.window->native)) == atoi(g_getenv("GDK_SCALE") ? g_getenv("GDK_SCALE") : "1"));
    check_bounds(cui_window_root(demo.window));
    if (path) save_snapshot(path);
    /* Exercise the native close-request path with two visible windows. */
    gtk_window_close(GTK_WINDOW(second_window->native));
    CHECK(app->running && !second_window->visible);
    gtk_window_close(GTK_WINDOW(demo.window->native));
    CHECK(!app->running && !demo.window->visible);
    /* Destruction is rejected while the event callback is still on the stack. */
    cui_app_destroy(app);
    CHECK(strstr(cui_app_error(app), "after cui_app_run") != NULL);
    return G_SOURCE_REMOVE;
}

static gboolean close_reopened(gpointer data)
{
    cui_window_close((cui_window *)data);
    return G_SOURCE_REMOVE;
}

int main(void)
{
    cui_app *app = cui_app_create();
    cui_widget *root, *box, *child;
    char text[128], short_text[3];
    const char *unicode = "Gr\303\274\303\237e \344\270\226\347\225\214";
    CHECK(app != NULL);
    CHECK(cui_app_create() == NULL);
    CHECK(settings_demo_create(&demo, app));
    CHECK(cui_button(demo.name, "invalid parent") == NULL);
    cui_on_action(demo.name, count_action, NULL);
    cui_set_text(demo.name, unicode);
    CHECK(actions == 0);
    CHECK(cui_get_text(demo.name, NULL, 0) == strlen(unicode));
    CHECK(cui_get_text(demo.name, text, sizeof(text)) == strlen(unicode));
    CHECK(strcmp(text, unicode) == 0);
    CHECK(cui_get_text(demo.name, short_text, sizeof(short_text)) == strlen(unicode));
    CHECK(short_text[2] == '\0');
    gtk_editable_set_text(GTK_EDITABLE(demo.name->native), "Alex Morgan");
    CHECK(actions > 0);
    actions = 0;
    cui_on_action(demo.notifications, count_action, NULL);
    cui_set_checked(demo.notifications, 0);
    CHECK(!cui_get_checked(demo.notifications) && actions == 0);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(demo.notifications->native), TRUE);
    CHECK(cui_get_checked(demo.notifications) && actions == 1);
    g_signal_emit_by_name(demo.save->native, "clicked");
    CHECK(!gtk_widget_get_sensitive(GTK_WIDGET(demo.save->native)));
    cui_get_text(demo.status, text, sizeof(text));
    CHECK(!strcmp(text, "Changes applied for this session."));
    cui_set_enabled(demo.save, 1);
    cui_set_text(demo.status, "Everything is up to date.");
    cui_app_set_theme(app, CUI_THEME_DARK);
    CHECK(gtk_widget_has_css_class(GTK_WIDGET(demo.window->native), "cui-dark"));
    cui_app_set_theme(app, CUI_THEME_LIGHT);
    CHECK(!gtk_widget_has_css_class(GTK_WIDGET(demo.window->native), "cui-dark"));
    {
        char *original = NULL;
        GtkSettings *settings = gtk_settings_get_default();
        g_object_get(settings, "gtk-theme-name", &original, NULL);
        g_object_set(settings, "gtk-theme-name", "HighContrast", NULL);
        CHECK(!gtk_widget_has_css_class(GTK_WIDGET(demo.window->native), "cui-window"));
        g_object_set(settings, "gtk-theme-name", original, NULL);
        g_free(original);
        CHECK(gtk_widget_has_css_class(GTK_WIDGET(demo.window->native), "cui-window"));
    }
    if (g_getenv("CUI_TEST_DARK")) g_signal_emit_by_name(demo.dark->native, "clicked");

    second_window = cui_window_create(app, "Lifecycle test", 300, 200);
    CHECK(second_window != NULL);
    root = cui_window_root(second_window);
    box = cui_box(root, CUI_VERTICAL, 0);
    cui_set_enabled(box, 0);
    child = cui_button(box, "Inherited disabled state");
    CHECK(!gtk_widget_is_sensitive(GTK_WIDGET(child->native)));
    cui_set_enabled(child, 0);
    cui_set_enabled(box, 1);
    CHECK(!gtk_widget_is_sensitive(GTK_WIDGET(child->native)));
    cui_set_enabled(child, 1);
    CHECK(gtk_widget_is_sensitive(GTK_WIDGET(child->native)));
    cui_window_show(second_window);
    cui_window_show(demo.window);
    g_timeout_add(500, close_windows, app);
    cui_app_run(app);
    cui_window_show(demo.window);
    g_timeout_add(50, close_reopened, demo.window);
    cui_app_run(app);
    cui_app_destroy(app);
    app = cui_app_create();
    CHECK(app != NULL);
    cui_app_run(app); /* No visible windows: returns immediately. */
    cui_app_destroy(app);
    puts("GTK: Unicode, callbacks, themes, sensitivity, sizing and lifecycle passed");
    return 0;
}
