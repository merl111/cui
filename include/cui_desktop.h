#ifndef CUI_DESKTOP_H
#define CUI_DESKTOP_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Desktop APIs use the same app ownership and main-thread contract as cui.h. */
int cui_focus(cui_widget *widget);
int cui_has_focus(const cui_widget *widget);
void cui_accessibility(cui_widget *widget, const char *label, const char *description);
void cui_set_read_only(cui_widget *widget, int read_only);
void cui_undo(cui_widget *widget);
void cui_redo(cui_widget *widget);

typedef struct cui_dialog cui_dialog;
typedef enum cui_dialog_kind { CUI_DIALOG_OPEN, CUI_DIALOG_SAVE, CUI_DIALOG_FOLDER, CUI_DIALOG_ALERT, CUI_DIALOG_COLOR, CUI_DIALOG_FONT } cui_dialog_kind;
typedef enum cui_dialog_result { CUI_DIALOG_CANCELLED, CUI_DIALOG_ACCEPTED, CUI_DIALOG_FAILED } cui_dialog_result;
/* File dialogs pass the first UTF-8 local path to the callback; accepted paths
 * can also be read until app destruction. Alerts return an empty path. Opening is deferred to the
 * UI loop; cancelling an open dialog may invoke its callback before returning.
 * Cancel is safe; destruction cancels without callbacks. */
typedef void (*cui_dialog_callback)(cui_dialog *, cui_dialog_result, const char *path, void *userdata);
cui_dialog *cui_file_dialog(cui_window *parent, cui_dialog_kind kind, const char *title,
    const char *initial_path, cui_dialog_callback callback, void *userdata);
/* Portable filters use literal filename extensions without a leading dot/glob,
 * e.g. "txt", "md", "tar.gz". Empty extension lists mean all files.
 * At most 64 named filters, 32 extensions each; names <=255 bytes, extensions
 * <=64 ASCII letters/digits/dots/hyphens/underscores. Filters guide the native
 * chooser; applications must validate selected file content themselves. */
typedef struct cui_file_filter { const char *name; const char *const *extensions; size_t extension_count; } cui_file_filter;
typedef struct cui_file_options {
    const char *initial_path;
    const cui_file_filter *filters; size_t filter_count,initial_filter; /* Zero-based. */
    int multiple; /* OPEN/FOLDER only. FOLDER rejects file filters. */
} cui_file_options;
/* Options and strings are copied before return. NULL options uses defaults.
 * Initial path can be a file or directory; SAVE also accepts a suggested name.
 * No file is read/written. SAVE uses native overwrite confirmation; extension
 * handling follows each OS. Multiple results preserve the chooser's order
 * (not necessarily click order), with a maximum of 65536 local paths. */
cui_dialog *cui_file_dialog_ex(cui_window *parent,cui_dialog_kind kind,const char *title,
    const cui_file_options *options,cui_dialog_callback callback,void *userdata);
size_t cui_dialog_path_count(const cui_dialog *dialog); /* 0 before acceptance/cancel/failure. */
const char *cui_dialog_path(const cui_dialog *dialog,size_t index); /* Borrowed until app destruction; NULL for invalid index. */
int cui_dialog_filter(const cui_dialog *dialog,size_t *index); /* Accepted named filter, unchanged on failure. */
cui_dialog *cui_alert(cui_window *parent, const char *title, const char *message,
    const char *accept_label, cui_dialog_callback callback, void *userdata);
void cui_dialog_cancel(cui_dialog *dialog);
/* RGB colors are 0xRRGGBB (opaque sRGB). Font families are UTF-8, at most
 * 128 bytes; points are 6..200, weight 100..900, italic is 0 or 1.
 * Initial values are copied. Picker callbacks use an empty path; getters
 * succeed only after acceptance, including inside the callback, until app
 * destruction. On cancellation/failure they leave output values unchanged.
 * macOS shares the system font/color panels: opening another picker of the
 * same kind cancels the preceding CUI picker. */
typedef struct cui_font_value { char family[129]; double points; int weight, italic; } cui_font_value;
cui_dialog *cui_color_dialog(cui_window *parent, const char *title, unsigned rgb,
    cui_dialog_callback callback, void *userdata);
cui_dialog *cui_font_dialog(cui_window *parent, const char *title, const cui_font_value *font,
    cui_dialog_callback callback, void *userdata);
int cui_dialog_color(const cui_dialog *dialog, unsigned *rgb);
int cui_dialog_font(const cui_dialog *dialog, cui_font_value *font);
/* Apply all selected font properties, inherited by descendants. */
int cui_font_apply(cui_widget *widget, const cui_font_value *font);

typedef struct cui_command cui_command;
typedef struct cui_menu cui_menu;
typedef enum cui_modifiers { CUI_MOD_SHIFT = 1, CUI_MOD_ALT = 2, CUI_MOD_CONTROL = 4, CUI_MOD_PRIMARY = 8 } cui_modifiers;
/* Key is an ASCII letter/digit, CUI_KEY_ESCAPE, CUI_KEY_ENTER,
   CUI_KEY_TAB, or 0 for no shortcut. PRIMARY means Command
 * on macOS and Control elsewhere. Shortcuts are scoped to CUI windows. */
cui_command *cui_command_create(cui_app *app, const char *label, unsigned key, unsigned modifiers, cui_task action, void *userdata);
void cui_command_set_enabled(cui_command *command, int enabled);
void cui_command_set_checked(cui_command *command, int checked); /* Makes it checkable. */
int cui_command_invoke(cui_command *command);
cui_menu *cui_menu_create(cui_app *app);
int cui_menu_add(cui_menu *menu, cui_command *command);
int cui_menu_add_submenu(cui_menu *menu, const char *label, cui_menu *submenu);
int cui_menu_add_separator(cui_menu *menu);
/* A menu bar contains named submenus only. NULL removes it. Once attached as
 * a bar, only more submenus can be added. macOS retains standard application
 * and text-editing actions alongside the application-provided menus. */
void cui_window_set_menu(cui_window *window, cui_menu *menu);
void cui_menu_popup(cui_menu *menu, cui_widget *anchor);
/* Show an undecorated window as a transient, focusable popup anchored to a
 * widget-local rectangle or enabled canvas hit region. Outside clicks, Escape
 * and parent closure dismiss it. is_visible observes dismissal; show again via
 * this API. Native placement flips/clamps at screen edges. The app owns content.
 * Invalid, hidden or disabled anchors return 0 without showing the popup. */
int cui_window_popup_at(cui_window *panel, cui_widget *anchor,
    double x, double y, double width, double height);
int cui_window_popup_region(cui_window *panel, cui_widget *canvas, unsigned region);
/* Anchor a native context menu to a rectangle in widget-local logical units.
   Returns zero for invalid geometry, different applications or hidden/disabled
   anchors. Native menus handle edge placement, keyboard navigation and dismissal.
   Coordinates must be finite, within +/-1e6; sizes must be 0 < size <= 1e6. */
int cui_menu_popup_at(cui_menu *menu, cui_widget *anchor,
                     double x, double y, double width, double height);
/* Use a current enabled canvas hit region, including a chat action region.
   A stale/missing/disabled region returns zero and never opens a fallback menu. */
int cui_menu_popup_region(cui_menu *menu, cui_widget *canvas, unsigned region);
cui_widget *cui_toolbar(cui_widget *parent, cui_command *const *commands, size_t count);
#ifdef __cplusplus
}
#endif
#endif
