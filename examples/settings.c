#include "settings_ui.h"
#include <stdio.h>
#include <string.h>

static int run_settings(int argc, char **argv)
{
    settings_demo demo;
    cui_app *app = cui_app_create();
    if (!app) { fputs("Cannot initialize the native GUI. Is a desktop session available?\n", stderr); return 1; }
    if (!settings_demo_create(&demo, app)) {
        fprintf(stderr, "Cannot create settings: %s\n", cui_app_error(app));
        cui_app_destroy(app);
        return 1;
    }
    if (argc > 1 && !strcmp(argv[1], "--dark")) cui_app_set_theme(app, CUI_THEME_DARK);
    if (argc > 1 && !strcmp(argv[1], "--light")) cui_app_set_theme(app, CUI_THEME_LIGHT);
    cui_window_show(demo.window);
    cui_app_run(app);
    cui_app_destroy(app);
    return 0;
}

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    (void)instance; (void)previous; (void)command; (void)show;
    return run_settings(0, NULL);
}
#else
int main(int argc, char **argv) { return run_settings(argc, argv); }
#endif
