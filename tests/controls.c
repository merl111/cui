#include "cui_internal.h"
#include "../examples/gallery_ui.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static int callbacks;
static gallery_demo demo;
static void count(cui_widget *w, void *p) { (void)w; (void)p; ++callbacks; }
static void snapshot(cui_window *window, const char *path)
{
    GtkWidget *native = GTK_WIDGET(window->native);
    GdkPaintable *paintable = gtk_widget_paintable_new(native);
    GtkSnapshot *snapshot = gtk_snapshot_new();
    double scale = cui_window_scale(window);
    gtk_snapshot_scale(snapshot, (float)scale, (float)scale);
    gdk_paintable_snapshot(paintable, snapshot, gtk_widget_get_width(native), gtk_widget_get_height(native));
    GskRenderNode *node = gtk_snapshot_free_to_node(snapshot); CHECK(node);
    GdkTexture *texture = gsk_renderer_render_texture(gtk_native_get_renderer(GTK_NATIVE(native)), node, NULL);
    CHECK(gdk_texture_save_to_png(texture, path));
    g_object_unref(texture); gsk_render_node_unref(node); g_object_unref(paintable);
}
static gboolean inspect(gpointer data)
{
    const char *path = g_getenv("CUI_SCREENSHOT");
    (void)data;
    if (cui_get_selected(demo.tabs) == 5) {
        int width, height;
        pango_layout_get_pixel_size(gtk_label_get_layout(GTK_LABEL(demo.font_sample->native)), &width, &height);
        CHECK(width <= gtk_widget_get_width(GTK_WIDGET(demo.font_sample->native)));
        CHECK(height <= gtk_widget_get_height(GTK_WIDGET(demo.font_sample->native)));
        CHECK(gtk_widget_get_width(GTK_WIDGET(demo.tabs->native)) <= gtk_widget_get_width(GTK_WIDGET(demo.window->native)));
    }
    if (path) snapshot(demo.window, path);
    CHECK(cui_window_scale(demo.window) == gtk_widget_get_scale_factor(GTK_WIDGET(demo.window->native)));
    CHECK(gtk_widget_get_width(GTK_WIDGET(demo.slider->native)) > 100 || cui_get_selected(demo.tabs) != 0);
    cui_window_close(demo.window);
    return G_SOURCE_REMOVE;
}
int main(void)
{
    cui_app *app = cui_app_create(); CHECK(app);
    CHECK(gallery_demo_create(&demo, app));
    char text[256];
    CHECK(cui_get_selected(demo.tabs) == 0);
    CHECK(!gtk_entry_get_visibility(GTK_ENTRY(demo.password->native)));
    cui_set_text(demo.password, "secret-\303\274");
    cui_get_text(demo.password, text, sizeof(text)); CHECK(!strcmp(text, "secret-\303\274"));
    cui_on_action(demo.toggle, count, NULL); cui_set_checked(demo.toggle, 1); CHECK(!callbacks);
    g_signal_emit_by_name(demo.toggle->native, "clicked"); CHECK(callbacks == 1 && !cui_get_checked(demo.toggle));
    cui_on_action(demo.toggle_switch, count, NULL);
    cui_set_checked(demo.toggle_switch, 0); CHECK(callbacks == 1);
    gtk_switch_set_active(GTK_SWITCH(demo.toggle_switch->aux), TRUE); CHECK(callbacks == 2);
    CHECK(cui_get_checked(demo.radios[1]));
    cui_activate(demo.radios[0]); CHECK(cui_get_checked(demo.radios[0]) && !cui_get_checked(demo.radios[1]));
    cui_activate(demo.radios[0]); CHECK(cui_get_checked(demo.radios[0]));
    cui_set_value(demo.slider, 0.25); CHECK(cui_get_value(demo.progress) == 0.64);
    gtk_range_set_value(GTK_RANGE(demo.slider->native), 0.8); CHECK(fabs(cui_get_value(demo.progress) - 0.8) < 0.001);
    cui_set_value(demo.slider, NAN); CHECK(fabs(cui_get_value(demo.slider) - 0.8) < 0.001);
    cui_set_value(demo.slider, 2); CHECK(cui_get_value(demo.slider) == 1);
    CHECK(gtk_tree_model_iter_n_children(gtk_combo_box_get_model(GTK_COMBO_BOX(demo.select->native)), NULL) == (int)demo.select->item_count);
    cui_set_selected(demo.select, 2); CHECK(cui_get_selected(demo.select) == 2);
    cui_set_selected(demo.select, 99); CHECK(cui_get_selected(demo.select) == 2);
    cui_set_selected(demo.select, -1); CHECK(cui_get_selected(demo.select) == -1);
    cui_set_selected(demo.tabs, 1);
    CHECK(!gtk_widget_get_visible(GTK_WIDGET(demo.slider->native)));
    CHECK(gtk_widget_get_visible(GTK_WIDGET(demo.table->native)));
    cui_set_selected(demo.table, 3); CHECK(cui_get_selected(demo.table) == 3);
    gtk_editable_set_text(GTK_EDITABLE(demo.filter->native), "Completed");
    CHECK(demo.table->item_count == 3 && cui_get_selected(demo.table) == -1);
    gtk_editable_set_text(GTK_EDITABLE(demo.filter->native), "no matches"); CHECK(demo.table->item_count == 0);
    gtk_editable_set_text(GTK_EDITABLE(demo.filter->native), ""); CHECK(demo.table->item_count == 15);
    const char *duplicates[] = {"Same", "Same", "Other"};
    CHECK(cui_set_items(demo.list, duplicates, 3)); cui_set_selected(demo.list, 1); CHECK(cui_get_selected(demo.list) == 1);
    CHECK(cui_set_items(demo.list, NULL, 0)); CHECK(cui_get_selected(demo.list) == -1);
    cui_set_expanded(demo.disclosure, 1); CHECK(cui_get_expanded(demo.disclosure));
    cui_set_expanded(demo.disclosure, 0); CHECK(!cui_get_expanded(demo.disclosure));
    cui_set_selected(demo.tabs, 2); CHECK(cui_activate(demo.approve));
    CHECK(!cui_activate(demo.approve));
    cui_activate(demo.task_run); CHECK(demo.spinner->hidden);
    cui_set_text(demo.composer, "Hello \344\270\226\347\225\214\nSecond line"); cui_activate(demo.send);
    cui_get_text(demo.reply, text, sizeof(text)); CHECK(!strcmp(text, "Hello \344\270\226\347\225\214\nSecond line"));
    CHECK(!cui_get_text(demo.composer, NULL, 0));
    cui_set_selected(demo.tabs, 4);
    for (int i = 0; i < CUI_PATTERN_COUNT; ++i) {
        CHECK(demo.patterns[i] && cui_pattern_name((cui_pattern)i)[0]);
        CHECK(cui_pattern_part(demo.patterns[i], CUI_PART_TITLE));
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(demo.pattern_picker->native), CUI_RECORDS_TABLE);
    cui_widget *records = demo.patterns[CUI_RECORDS_TABLE];
    cui_widget *record_table = cui_pattern_part(records, CUI_PART_BODY);
    CHECK(cui_activate(cui_pattern_part(records, CUI_PART_PRIMARY)));
    CHECK(!strcmp(record_table->items[0], "Release notes"));
    gtk_editable_set_text(GTK_EDITABLE(cui_pattern_part(records, CUI_PART_INPUT)->native), "no match");
    CHECK(record_table->item_count == 0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(demo.pattern_picker->native), CUI_FILTER_TABLE);
    CHECK(cui_activate(cui_pattern_part(demo.patterns[CUI_FILTER_TABLE], CUI_PART_PRIMARY)));
    CHECK(cui_pattern_part(demo.patterns[CUI_FILTER_TABLE], CUI_PART_BODY)->item_count == 6);
    CHECK(cui_stream_append(demo.patterns[CUI_STREAMING], "Unicode: 世界"));
    CHECK(!cui_stream_append(records, "invalid kind"));
    CHECK(!cui_app_set_text_scale(app, NAN) && !cui_app_set_text_scale(app, 0));
    CHECK(!cui_set_font(demo.font_sample, "bad\"family", 12, 400));
    CHECK(cui_set_font(demo.font_sample, NULL, 24, 600));
    cui_widget *image = cui_pattern_part(demo.patterns[CUI_AGENT_SCREEN], CUI_PART_PREVIEW);
    CHECK(image->image_width == 64 && image->image_height == 40);
    CHECK(!cui_image_set_rgba(image, NULL, 10, 10));
    double invalid[] = {NAN};
    CHECK(!cui_chart_set_values(cui_pattern_part(demo.patterns[CUI_INSIGHTS], CUI_PART_CHART), invalid, 1));
    gtk_combo_box_set_active(GTK_COMBO_BOX(demo.pattern_picker->native), 0);
    cui_set_selected(demo.tabs, 0); cui_set_value(demo.slider, 0.64); cui_set_value(demo.progress, 0.64);
    cui_set_selected(demo.select, 0); cui_set_text(demo.password, "correct-horse");
    cui_set_checked(demo.toggle, 0); cui_set_checked(demo.radios[1], 1);
    cui_set_text(demo.meter, "Progress  64%");
    cui_set_text(demo.status, "Explore the controls. Every example runs through the public C API.");
    const char *page = g_getenv("CUI_GALLERY_PAGE");
    const char *pattern = g_getenv("CUI_GALLERY_PATTERN");
    if (pattern) { gtk_combo_box_set_active(GTK_COMBO_BOX(demo.pattern_picker->native), (guint)atoi(pattern)); }
    if (g_getenv("CUI_TEXT_SCALE")) {
        CHECK(cui_app_set_text_scale(app, atof(g_getenv("CUI_TEXT_SCALE"))));
        cui_set_selected(demo.text_scale_select, 2);
    }
    if (page) cui_set_selected(demo.tabs, atoi(page));
    if (g_getenv("CUI_TEST_DARK")) cui_app_set_theme(app, CUI_THEME_DARK);
    cui_window_show(demo.window);
    g_timeout_add(700, inspect, NULL); cui_app_run(app);
    cui_app_destroy(app);
    puts("controls: values, selection, replacement, callbacks, filtering, tabs, disclosure, composition and UTF-8 passed");
    return 0;
}
