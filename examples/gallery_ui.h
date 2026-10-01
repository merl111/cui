#ifndef CUI_GALLERY_UI_H
#define CUI_GALLERY_UI_H
#include "cui.h"
#include "cui_patterns.h"
typedef struct gallery_demo {
    cui_app *app;
    cui_window *window;
    cui_widget *tabs, *status, *slider, *progress, *meter, *list, *table, *filter;
    cui_widget *password, *toggle, *toggle_switch, *radios[3], *select, *disclosure;
    cui_widget *approve, *approval_status, *task_run, *task_badge, *spinner;
    cui_widget *patterns[CUI_PATTERN_COUNT], *pattern_picker, *font_sample, *text_scale_select;
    cui_timer *stream_timer; int stream_step;
    cui_widget *composer, *send, *reply, *code;
} gallery_demo;
int gallery_demo_create(gallery_demo *demo, cui_app *app);
#endif
