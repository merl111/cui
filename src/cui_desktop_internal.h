#ifndef CUI_DESKTOP_INTERNAL_H
#define CUI_DESKTOP_INTERNAL_H
#include "cui_internal.h"
#include "cui_desktop.h"
typedef struct cui_file_type {char *name;char **extensions;size_t count;} cui_file_type;
typedef struct cui_file_settings {cui_file_type *filters;size_t count,initial;int multiple;} cui_file_settings;
struct cui_dialog {
    cui_dialog *next; cui_window *parent; cui_dialog_kind kind;
    char *title, *path, *message, *accept;
    cui_dialog_callback callback; void *userdata, *native;
    cui_timer *timer; int finished, cancelled, references;
    cui_file_settings *files;char **paths;size_t path_count,selected_filter;
    cui_dialog_result result; unsigned color; cui_font_value font;
};
struct cui_command {
    cui_command *next; cui_app *app; char *label;
    unsigned id, key, modifiers; int enabled, checked, checkable;
    cui_task action; void *userdata, *native;
};
typedef struct cui_menu_item { cui_command *command; cui_menu *submenu; char *label; } cui_menu_item;
struct cui_menu {
    cui_menu *next; cui_app *app; cui_menu_item *items; size_t count;
    void *native; int attached;
};
cui_dialog *cui__dialog_create(cui_window *parent,cui_dialog_kind kind,const char *title,const char *path,const char *message,const char *accept,cui_dialog_callback callback,void *data);
void cui__file_open_native(cui_dialog *dialog);
void cui__file_settings_free(cui_file_settings *settings);
void cui__file_finish(cui_dialog *dialog,cui_dialog_result result,const char *const *paths,size_t count,size_t filter);
int cui__desktop_key(cui_window *window,unsigned key,unsigned modifiers);
int cui__desktop_command_id(cui_window *window,unsigned id);
void cui__dialog_retain(cui_dialog *dialog);
void cui__dialog_release(cui_dialog *dialog);
void cui__desktop_free(cui_app *app);
void cui__desktop_destroy(cui_app *app);
void cui__desktop_window(cui_window *window);
void cui__desktop_finish(cui_dialog *dialog, cui_dialog_result result, const char *path);
void cui__desktop_open(cui_dialog *dialog);
void cui__desktop_cancel(cui_dialog *dialog);
void cui__desktop_command(cui_command *command);
void cui__desktop_dispose(cui_app *app);
void cui__desktop_menu(cui_window *window);
int cui__desktop_popup_at(cui_menu *menu, cui_widget *anchor,
                          double x, double y, double width, double height);
char *cui__desktop_copy(const char *text);
int cui__font_valid(const cui_font_value *font);
void cui__picker_open(cui_dialog *dialog);
void cui__picker_cancel(cui_dialog *dialog);
#endif
