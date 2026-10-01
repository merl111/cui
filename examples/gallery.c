#include "gallery_ui.h"
#include <stdio.h>
#include <string.h>

static int gallery_main(int argc, char **argv)
{
    cui_app *app = cui_app_create(); gallery_demo demo;
    if (!app) { fputs("Could not open the desktop session.\n", stderr); return 1; }
    if (!gallery_demo_create(&demo, app)) { fprintf(stderr, "%s\n", cui_app_error(app)); cui_app_destroy(app); return 1; }
    if (argc > 1 && !strcmp(argv[1], "--dark")) cui_app_set_theme(app, CUI_THEME_DARK);
    cui_window_show(demo.window); cui_app_run(app); cui_app_destroy(app);
    return 0;
}
#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{ (void)instance; (void)previous; (void)command; (void)show; return gallery_main(0, NULL); }
#else
int main(int argc, char **argv) { return gallery_main(argc, argv); }
#endif
