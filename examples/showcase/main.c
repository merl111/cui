#include "cui.h"
#include "cui_draw.h"
#include "cui_chat.h"
#include "cui_desktop.h"
#include "cui_layouts.h"
#include "cui_inputs.h"
#include "cui_navigation.h"
#include "cui_search.h"
#include "cui_tokens.h"
#include "cui_feedback.h"
#include "cui_tables.h"
#include "cui_patterns.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

typedef struct showcase_case { const char *id, *name; void (*create)(cui_widget *); } showcase_case;
static cui_app *app;
static cui_window *window;
static cui_widget *status;
static cui_widget *chat_preview;
static size_t current;
static int smoke, capture, settling;
static volatile sig_atomic_t advance;
static void clicked(cui_widget *sender, void *data)
{ (void)sender; (void)data; cui_set_text(status, "Action received by the application."); }
static void chat_clicked(cui_widget *sender,void *data)
{
    (void)data;cui_chat_event event;if(!cui_chat_event_get(sender,&event))return;
    char value[160];snprintf(value,sizeof(value),"Application action %d · item %llu · option %u",event.action,(unsigned long long)event.id,event.index);
    cui_set_text(status,value);
}
static void command_clicked(void *data)
{ clicked(NULL, data); }
static void pattern_clicked(cui_widget *sender, void *data)
{
    (void)data; char text[80];
    snprintf(text, sizeof(text), "Application received pattern event %d.", (int)cui_pattern_event(sender));
    cui_set_text(status, text);
    if(cui_pattern_event(sender)==CUI_EVENT_SUBMIT&&(uintptr_t)data==CUI_CHAT){
        cui_widget *input=cui_pattern_part(sender,CUI_PART_INPUT);
        char message[2048];cui_get_text(input,message,sizeof(message));
        static cui_item_id next_id=100;
        const cui_pattern_item item={next_id++,"You",message,"Just now","",0,CUI_ROLE_SUBTLE,0};
        if(*message&&cui_pattern_upsert_item(sender,&item))cui_set_text(input,"");
    }
    if(cui_pattern_event(sender)==CUI_EVENT_CANCEL&&cui_pattern_item_event_id(sender))
        cui_pattern_remove_item(sender,cui_pattern_item_event_id(sender));
}
static void dialog_finished(cui_dialog *dialog, cui_dialog_result result, const char *path, void *data)
{
    (void)dialog; (void)data;
    cui_set_text(status, result == CUI_DIALOG_ACCEPTED ? (*path ? path : "Selection accepted.") : "Dialog closed.");
}
static void open_dialog(cui_widget *sender, void *data)
{
    (void)sender;
    cui_dialog_kind kind = (cui_dialog_kind)(uintptr_t)data;
    if (kind == CUI_DIALOG_ALERT) cui_alert(window, "Continue?", "This demonstration does not write files.", "Continue", dialog_finished, NULL);
    else if (kind == CUI_DIALOG_COLOR) cui_color_dialog(window, "Choose a color", 0x398576, dialog_finished, NULL);
    else if (kind == CUI_DIALOG_FONT) {
        const cui_font_value font = {"", 14, 400, 0};
        cui_font_dialog(window, "Choose typography", &font, dialog_finished, NULL);
    } else {
        const char *extensions[] = {"txt", "md"};
        const cui_file_filter filters[] = {{"Documents", extensions, 2}, {"All files", NULL, 0}};
        const cui_file_options options = {NULL, filters, 2, 0, 1};
        cui_file_dialog_ex(window, kind, "Choose documents", &options, dialog_finished, NULL);
    }
}
static void popup_menu(cui_widget *sender, void *data)
{ cui_menu_popup((cui_menu *)data, sender); }
#include "cases.inc"

static void show_case(void)
{
    cui_window *previous = window;
    chat_preview = NULL;
    int height = !strncmp(cases[current].id, "pattern-", 8) ? 800 : 540;
    window = cui_window_create(app, "CUI Showcase", 680, height);
    if (!window) { fputs("Could not create showcase window\n", stderr); exit(1); }
    cui_window_set_scrollable(window, 1);
    cui_widget *root = cui_window_root(window);
    cui_box_set_padding(root, 28);
    cui_set_role(cui_label(root, "CUI  /  NATIVE COMPONENTS"), CUI_ROLE_CAPTION);
    cui_set_role(cui_label(root, cases[current].name), CUI_ROLE_TITLE);
    cui_widget *surface = cui_box(root, CUI_VERTICAL, 14);
    cui_box_set_padding(surface, 20); cui_set_role(surface, CUI_ROLE_CARD);
    cases[current].create(surface);
    status = cui_label(root, "Native controls · System fonts · Logical layout");
    cui_set_role(status, CUI_ROLE_CAPTION);
    if (*cui_app_error(app)) { fprintf(stderr, "%s\n", cui_app_error(app)); exit(1); }
    cui_window_show(window);
    if (previous) cui_window_close(previous);
    settling = 0;
}
static void next_signal(int signal_number)
{ (void)signal_number; advance = 1; }
static void tick(void *data)
{
    (void)data;
    if (chat_preview) cui_chat_refresh(chat_preview, cui_window_scale(window));
    if (++settling == 6) { printf("READY %s\n", cases[current].id); fflush(stdout); }
    if ((smoke && settling >= 6) || advance) {
        advance = 0;
        if (++current == sizeof(cases) / sizeof(cases[0])) cui_app_quit(app);
        else show_case();
    }
}
static int run(int argc, char **argv)
{
    const char *choice = argc > 1 ? argv[1] : "button";
    if (!strcmp(choice, "--list")) {
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) puts(cases[i].id);
        return 0;
    }
    smoke = !strcmp(choice, "--smoke-test"); capture = !strcmp(choice, "--capture");
    if (!smoke && !capture) {
        for (; current < sizeof(cases) / sizeof(cases[0]); ++current) if (!strcmp(choice, cases[current].id)) break;
        if (current == sizeof(cases) / sizeof(cases[0])) { fprintf(stderr, "Unknown component: %s\n", choice); return 2; }
    }
#ifdef SIGUSR1
    if (capture) signal(SIGUSR1, next_signal);
#else
    if (capture) { fputs("Capture mode requires POSIX signals\n", stderr); return 2; }
#endif
    app = cui_app_create(); if (!app) return 1;
    cui_app_set_theme(app, CUI_THEME_LIGHT);
    show_case();
    if (smoke || capture) cui_every(app, 50, tick, NULL);
    cui_app_run(app); cui_app_destroy(app); return 0;
}
int main(int argc, char **argv) { return run(argc, argv); }
