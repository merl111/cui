/* Reproduce Archaic's 50 ms idle layout polling without an account/network. */
#include "cui.h"
#include "cui_chat.h"
#include "cui_draw.h"
#include "cui_layouts.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

static cui_app *app;
static cui_widget *root, *login, *settings, *buttons[5], *account, *entry, *split;
static cui_widget *composers[2];
static int ticks, actions, verify;
static cui_widget *header;
static double total, maximum;

static double now(void) {
#ifdef _WIN32
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
#endif
}
static void changed(cui_widget *w, void *data) {
    (void)w; (void)data;
    ++actions;
}
static void refresh(void) {
    cui_set_visible(login, 1);
    cui_set_min_size(login, 400, 1);
    cui_box_set_padding(login, 16);
    cui_expand(login, 1);
    cui_set_visible(settings, 0);
    cui_set_min_size(settings, 184, 1);
    assert(cui_set_icon_only(account, 0));
    cui_set_text(entry, "unchanged draft");
    cui_split_set_position(split, .3);
    for (int i = 0; i < 5; ++i) {
        assert(cui_set_icon_only(buttons[i], 0));
        cui_set_min_size(buttons[i], 176, 42);
        cui_set_visible(buttons[i], 1);
    }
    for (int i = 0; i < 2; ++i)
        assert(cui_chat_refresh(composers[i], 1));
}
static void tick(void *data) {
    (void)data;
    if (ticks == 5) {
        int width = 0, height = 0;
        assert(cui_widget_get_size(header, &width, &height));
        printf("canvas header_height=%d expected=42\n", height);
        if (verify) assert(height == 42);
    }
    double start = now();
    refresh();
    double elapsed = (now() - start) * 1000;
    if (++ticks > 5) {
        total += elapsed;
        if (elapsed > maximum) maximum = elapsed;
    }
    if (ticks == 35) {
        printf("idle_layout mean_ms=%.6f max_ms=%.6f samples=30\n", total / 30, maximum);
        fflush(stdout);
        assert(!actions);
        /* Real changes still apply after repeated no-op updates. */
        cui_set_visible(settings, 1);
        cui_set_visible(login, 0);
        cui_set_min_size(settings, 200, 200);
        cui_box_set_padding(settings, 24);
        cui_expand(settings, 1);
        cui_split_set_position(split, .6);
        assert(cui_split_get_position(split) == .6);
        cui_set_text(entry, "changed draft");
        char text[64];
        cui_get_text(entry, text, sizeof(text));
        assert(!strcmp(text, "changed draft"));
        assert(!actions);
        assert(!*cui_app_error(app));
        cui_app_quit(app);
    }
}
int main(int argc, char **argv) {
    verify = argc > 1 && !strcmp(argv[1], "--verify-layout");
    app = cui_app_create();
    assert(app);
    cui_window *window = cui_window_create(app, "Archaic idle layout benchmark", 1440, 900);
    assert(window);
    root = cui_window_root(window);
#ifdef __APPLE__
    int initial_width, initial_height, hidden_width, hidden_height;
    assert(cui_window_get_size(window, &initial_width, &initial_height));
    cui_widget *temporary_panel = cui_label(root, "Panel hidden before showing the window");
    assert(temporary_panel);
    cui_set_min_size(temporary_panel, 2000, 1200);
    cui_set_visible(temporary_panel, 0);
    assert(cui_window_get_size(window, &hidden_width, &hidden_height));
    assert(initial_width == hidden_width && initial_height == hidden_height);
#endif
    header = cui_canvas(root);
    assert(header);
    cui_set_min_size(header, 110, 42);
    login = cui_box(root, CUI_VERTICAL, 12);
    assert(login);
    assert(cui_label(login, "Welcome to Archaic"));
    assert(cui_entry(login, "https://matrix.org"));
    entry = cui_entry(login, "unchanged draft");
    assert(entry);
    cui_on_action(entry, changed, NULL);
    assert(cui_password(login, ""));
    assert(cui_button(login, "Sign in"));
    account = cui_button(login, "Account");
    settings = cui_box(root, CUI_VERTICAL, 12);
    assert(account && settings);
    cui_icon_asset *icon = cui_icon_symbol(CUI_SYMBOL_SETTINGS);
    assert(icon && cui_set_icon(account, icon));
    for (int i = 0; i < 5; ++i) {
        buttons[i] = cui_button(settings, "Settings section");
        assert(buttons[i] && cui_set_icon(buttons[i], icon));
    }
    cui_icon_release(icon);
    split = cui_split(settings, CUI_HORIZONTAL, .3);
    assert(split);
    for (int i = 0; i < 2; ++i) {
        composers[i] = cui_chat_create(cui_split_pane(split, i), CUI_CHAT_COMPOSER, CUI_CHAT_DAYLIGHT);
        assert(composers[i]);
    }
    refresh();
    assert(cui_every(app, 50, tick, NULL));
    cui_window_show(window);
    cui_app_run(app);
    assert(ticks == 35);
    cui_app_destroy(app);
    return 0;
}
