#ifndef CUI_SETTINGS_UI_H
#define CUI_SETTINGS_UI_H
#include "cui.h"

typedef struct settings_demo {
    cui_app *app;
    cui_window *window;
    cui_widget *name, *workspace, *notifications, *status;
    cui_widget *system, *light, *dark, *save;
} settings_demo;

int settings_demo_create(settings_demo *demo, cui_app *app);
#endif
