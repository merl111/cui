/* Public ABI smoke/contract coverage. Runs on a Windows desktop with App SDK 1.8. */
#include "cui.h"
#include "cui_desktop.h"
#include "cui_draw.h"
#include "cui_inputs.h"
#include "cui_navigation.h"
#include "cui_tables.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static cui_app *app;
static cui_widget *entry, *button;
static int actions, ticks, dialogs;
static cui_dialog *active_dialog;
static void action(cui_widget *w, void *data) {
    (void)w;
    (void)data;
    ++actions;
}
static void dialog_done(cui_dialog *d, cui_dialog_result result, const char *path, void *data) {
    (void)d;
    (void)path;
    (void)data;
    assert(result == CUI_DIALOG_CANCELLED);
    ++dialogs;
}
static void tick(void *data) {
    cui_window *window = data;
    if (++ticks == 1) {
        assert(cui_focus(entry));
        assert(cui_has_focus(entry));
        assert(cui_activate(button));
        assert(actions == 1);
        cui_app_set_theme(app, CUI_THEME_DARK);
    } else if (ticks == 2) {
        cui_app_set_theme(app, CUI_THEME_LIGHT);
        cui_set_read_only(entry, 1);
        cui_set_text(entry, "read-only setters still work");
    } else if (ticks == 3) {
        assert(dialogs == 1);
        active_dialog =
            cui_alert(window, "WinUI dialog", "Cancel after opening", "OK", dialog_done, NULL);
        assert(active_dialog);
    } else if (ticks == 5) {
        cui_dialog_cancel(active_dialog);
    } else if (ticks >= 7 && dialogs == 2) {
        assert(!*cui_app_error(app));
        cui_window_close(window);
    }
    assert(ticks < 40);
}
int main(void) {
    app = cui_app_create();
    if (!app) {
        fputs("WinUI initialization failed: install the Windows App SDK 1.8 runtime for this "
              "architecture.\n",
              stderr);
        return 1;
    }
    cui_window *window = cui_window_create(app, "CUI WinUI contracts", 800, 700);
    assert(window);
    cui_widget *root = cui_window_root(window);
    cui_window_set_scrollable(window, 1);
    button = cui_button(root, "Fluent action");
    assert(button);
    cui_set_role(button, CUI_ROLE_PRIMARY);
    cui_on_action(button, action, NULL);
    entry = cui_entry(root, "Hello");
    assert(entry);
    cui_on_action(entry, action, NULL);
    cui_set_text(entry, "Unicode: 世界\nsecond line");
    char text[128];
    cui_get_text(entry, text, sizeof(text));
    assert(strstr(text, "世界"));
    assert(!actions);
    cui_widget *check = cui_checkbox(root, "Check", 0), *toggle = cui_switch(root, "Switch", 0),
               *password = cui_password(root, "secret");
    assert(check && toggle && password);
    cui_on_action(check, action, NULL);
    cui_on_action(toggle, action, NULL);
    cui_set_checked(check, 1);
    cui_set_checked(toggle, 1);
    assert(cui_get_checked(check) && cui_get_checked(toggle) && !actions);
    cui_get_text(password, text, sizeof(text));
    assert(!strcmp(text, "secret"));
    const char *items[] = {"Same", "Same", "Other"};
    cui_widget *list = cui_list(root, items, 3), *select = cui_select(root, items, 3);
    assert(list && select);
    cui_on_action(list, action, NULL);
    cui_on_action(select, action, NULL);
    cui_set_selected(list, 1);
    cui_set_selected(select, 2);
    assert(cui_get_selected(list) == 1 && cui_get_selected(select) == 2 && !actions);
    const char *headers[] = {"Name", "Value"}, *cells[] = {"Beta", "2", "Alpha", "1"};
    cui_widget *table = cui_table(root, headers, 2);
    assert(table && cui_table_set_rows(table, cells, 2));
    cui_on_action(table, action, NULL);
    cui_table_set_multiple(table, 1);
    assert(cui_table_select_row(table, 0, 1) && cui_table_select_row(table, 1, 1));
    assert(cui_table_selected_rows(table, NULL, 0) == 2);
    assert(cui_table_set_editable(table, 0, 1));
    assert(cui_table_sort(table, 0, 0, 0));
    cui_table_get_cell(table, 0, 0, text, sizeof(text));
    assert(!strcmp(text, "Alpha") && !actions);
    cui_widget *number = cui_number(root, 2, 0, 10, .5, 1);
    assert(number);
    cui_on_action(number, action, NULL);
    assert(cui_number_set(number, 3.5));
    assert(fabs(cui_number_get(number) - 3.5) < .01 && !actions);
    cui_widget *date = cui_date(root, (cui_date_value){2026, 10, 2}),
               *time = cui_time_input(root, (cui_time_value){13, 24, 37});
    assert(date && time);
    assert(cui_date_get(date).day == 2 && cui_time_get(time).second == 37);
    cui_tree_item nodes[] = {{1, 0, "Root", 1}, {2, 1, "Child", 0}};
    cui_widget *tree = cui_tree(root, nodes, 2);
    assert(tree);
    assert(cui_tree_select(tree, 2));
    assert(cui_tree_selected(tree) == 2);
    cui_widget *canvas = cui_canvas(root);
    cui_surface *surface = cui_surface_create(64, 64, 1);
    assert(canvas && surface);
    cui_draw_command draw = {0};
    draw.op = CUI_DRAW_CLEAR;
    draw.color = 0x123456ff;
    assert(cui_surface_render(surface, &draw, 1));
    assert(cui_canvas_set_surface(canvas, surface));
    cui_canvas_region region = {1, 0, 0, 32, 32, "Canvas action", 1};
    assert(cui_canvas_set_regions(canvas, &region, 1));
    cui_surface_release(surface);
    cui_dialog *dialog =
        cui_alert(window, "Cancel test", "This should never be shown", "OK", dialog_done, NULL);
    assert(dialog);
    cui_dialog_cancel(dialog);
    assert(!actions);
    assert(!*cui_app_error(app));
    assert(cui_every(app, 150, tick, window));
    cui_window_show(window);
    cui_app_run(app);
    assert(ticks >= 7 && actions == 1 && dialogs == 2);
    cui_app_destroy(app);
    puts("WinUI C ABI, controls, models, canvas, theme, focus and teardown passed");
    return 0;
}
