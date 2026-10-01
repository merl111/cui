/* Native popup mapping must survive a compositor round trip. A show/hide test
 * in one callback misses Wayland's asynchronous popup dismissal. */
#include "cui_gtk.h"
#include "cui_draw.h"
#include <assert.h>
#include <stdio.h>

static cui_app *app;
static cui_window *phone, *panel;
static int phase;

static void check_mapped(void) {
    GtkWidget *popover = panel->attached_native;
    assert(cui_window_is_visible(panel));
    assert(gtk_widget_get_mapped(popover));
    assert(gtk_widget_get_width(popover) == 382);
    assert(gtk_widget_get_height(popover) == 696);
    assert(gdk_surface_get_mapped(gtk_native_get_surface(GTK_NATIVE(popover))));
}

static void advance(void *unused) {
    (void)unused;
    switch (++phase) {
    case 1:
        assert(!cui_window_is_visible(panel));
        cui_window_show(panel);
        break;
    case 2:
        check_mapped();
        cui_window_close(panel);
        break;
    case 3:
        cui_window_show(panel);
        break;
    case 4:
        check_mapped();
        /* The anchor can be updated before the new native size is allocated. */
        assert(cui_window_set_anchor(panel, phone, 12, 88, 874, 500));
        assert(cui_window_set_size(phone, 900, 600));
        break;
    case 5:
        assert(cui_window_set_anchor(panel, phone, 12, 88, 874, 500));
        break;
    case 6:
        check_mapped();
        assert(cui_window_set_size(phone, 347, 774));
        assert(cui_window_set_anchor(panel, phone, 12, 88, 323, 674));
        break;
    case 7:
        check_mapped();
        cui_window_close(panel);
        break;
    case 8:
        cui_window_show(panel);
        break;
    case 9:
        check_mapped();
        cui_app_quit(app);
        break;
    }
}

static void fill_window(cui_window *window, int width, int height) {
    cui_box_set_padding(cui_window_root(window), 0);
    cui_widget *canvas = cui_canvas(cui_window_root(window));
    assert(canvas);
    cui_expand(canvas, 1);
    cui_set_min_size(canvas, 260, 420);
    cui_surface *surface = cui_surface_create(width, height, 1);
    assert(surface);
    cui_draw_command clear = {.op=CUI_DRAW_CLEAR, .color=0x555555ff};
    assert(cui_surface_render(surface, &clear, 1));
    assert(cui_canvas_set_surface(canvas, surface));
    cui_surface_release(surface);
}

int main(void) {
    app = cui_app_create();
    assert(app);
    phone = cui_window_create(app, "Panel test phone", 347, 774);
    panel = cui_window_create(app, "Panel test inspector", 382, 696);
    assert(phone && panel);
    assert(cui_window_set_frame(phone, 0, 1, 0));
    assert(cui_window_set_frame(panel, 0, 0, 23));
    fill_window(phone, 347, 774);
    fill_window(panel, 382, 696);
    assert(cui_window_set_anchor(panel, phone, 12, 88, 323, 674));
    cui_window_show(phone);
    assert(cui_every(app, 500, advance, NULL));
    cui_app_run(app);
    cui_app_destroy(app);
    puts("Attached panel: delayed mapping, close/reopen and resize placement passed");
}
