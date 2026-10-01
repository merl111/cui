#ifndef CUI_H
#define CUI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cui_app cui_app;
typedef struct cui_window cui_window;
typedef struct cui_widget cui_widget;
typedef struct cui_timer cui_timer;
typedef void (*cui_callback)(cui_widget *sender, void *userdata);
typedef void (*cui_task)(void *userdata);
typedef enum cui_key {
    CUI_KEY_BACKSPACE=8, CUI_KEY_TAB=9, CUI_KEY_ENTER=13, CUI_KEY_ESCAPE=27,
    CUI_KEY_UP=256, CUI_KEY_DOWN, CUI_KEY_HOME, CUI_KEY_END
} cui_key;
/* Navigation keys on entry/search/password and list/table controls. Return 1 to consume.
 * Active IME composition retains the keys it needs. Modifiers use CUI_MOD_*
 * from cui_desktop.h; Control also sets PRIMARY on Windows/Linux. NULL removes
 * the handler; text edits still use on_action. */
typedef int (*cui_key_callback)(cui_widget *, cui_key, unsigned modifiers, void *userdata);
int cui_on_key(cui_widget *entry, cui_key_callback callback, void *userdata);

typedef enum cui_axis { CUI_HORIZONTAL, CUI_VERTICAL } cui_axis;
typedef enum cui_theme { CUI_THEME_SYSTEM, CUI_THEME_LIGHT, CUI_THEME_DARK } cui_theme;
typedef enum cui_role {
    CUI_ROLE_BODY, CUI_ROLE_TITLE, CUI_ROLE_HEADING,
    CUI_ROLE_CAPTION, CUI_ROLE_PRIMARY, CUI_ROLE_CARD,
    CUI_ROLE_SUCCESS, CUI_ROLE_WARNING, CUI_ROLE_DANGER, CUI_ROLE_SUBTLE,
    CUI_ROLE_PANEL, CUI_ROLE_CHAT_BACKGROUND, CUI_ROLE_MESSAGE, CUI_ROLE_OUTGOING, CUI_ROLE_FLAT
} cui_role;

/* Original built-in vector symbols. No icon fonts or external assets. */
typedef enum cui_symbol {
    CUI_SYMBOL_NONE, CUI_SYMBOL_PLAY, CUI_SYMBOL_PAUSE, CUI_SYMBOL_PREVIOUS, CUI_SYMBOL_NEXT, CUI_SYMBOL_VOLUME, CUI_SYMBOL_MUTED, CUI_SYMBOL_SHUFFLE, CUI_SYMBOL_REPEAT, CUI_SYMBOL_SEARCH, CUI_SYMBOL_MENU, CUI_SYMBOL_MORE, CUI_SYMBOL_ATTACH, CUI_SYMBOL_SEND, CUI_SYMBOL_HEART, CUI_SYMBOL_HEART_FILLED, CUI_SYMBOL_REPLY, CUI_SYMBOL_INFO, CUI_SYMBOL_CLOSE, CUI_SYMBOL_PLUS, CUI_SYMBOL_CHECK, CUI_SYMBOL_UP, CUI_SYMBOL_DOWN, CUI_SYMBOL_PIN, CUI_SYMBOL_ARCHIVE, CUI_SYMBOL_MAIL, CUI_SYMBOL_EDIT, CUI_SYMBOL_COUNT
} cui_symbol;
/* Immutable, reference-counted icon assets. Creation copies the input. Assets
 * are independent of apps; widgets retain them. Release your reference after
 * attaching. All operations, including retain/release, use the UI thread.
 * Coordinates use the asset view box; sizes on widgets are logical units.
 * Commands: MOVE/LINE use values[0..1], CUBIC uses all six, CLOSE none.
 * FILL uses values[0] for even-odd (0 = nonzero); STROKE uses width, cap
 * (0 butt, 1 round, 2 square), join (0 miter, 1 round, 2 bevel).
 * Paint rgba is 0xRRGGBBAA; current_color substitutes the native foreground.
 * FILL/STROKE consume a path. Repeat geometry for both fill and stroke.
 * Invalid input returns NULL without modifying existing widgets. */
typedef struct cui_icon_asset cui_icon_asset;
typedef enum cui_icon_op { CUI_ICON_MOVE, CUI_ICON_LINE, CUI_ICON_CUBIC,
    CUI_ICON_CLOSE, CUI_ICON_FILL, CUI_ICON_STROKE } cui_icon_op;
typedef struct cui_icon_command {
    cui_icon_op op;
    float values[6];
    unsigned rgba;
    int current_color;
} cui_icon_command;
cui_icon_asset *cui_icon_vector(float width, float height, const cui_icon_command *commands, size_t count);
cui_icon_asset *cui_icon_rgba(const unsigned char *pixels, int width, int height);
cui_icon_asset *cui_icon_symbol(cui_symbol symbol);
/* Portable compiled SVG assets, produced by tools/compile_icons.py. File and
 * memory loaders validate sizes, commands and finite coordinates. No network,
 * external entities, scripts, fonts or native parser dependencies. */
cui_icon_asset *cui_icon_load(const char *path);
/* Decode a raster image file with the operating system image loader (PNG/JPEG).
 * RGBA pixels are copied; vectors should use compiled SVG to retain scaling. */
cui_icon_asset *cui_icon_load_image(const char *path);
cui_icon_asset *cui_icon_decode(const void *bytes, size_t length);
cui_icon_asset *cui_icon_retain(cui_icon_asset *asset);
void cui_icon_release(cui_icon_asset *asset);
/* NULL removes an icon. Icon-only buttons require a nonempty accessible label,
 * retained by get/set_text. Normal buttons/toggles support a leading icon.
 * Sizes 1..512 logical units (default 20); vectors render at backing scale.
 * get_icon returns a borrowed reference. Setters are silent. */
cui_widget *cui_icon(cui_widget *parent, cui_icon_asset *asset);
cui_widget *cui_icon_button(cui_widget *parent, cui_icon_asset *asset, const char *accessible_label);
int cui_set_icon(cui_widget *widget, cui_icon_asset *asset);
cui_icon_asset *cui_get_icon(const cui_widget *widget);
int cui_set_icon_size(cui_widget *widget, int logical_size);
/* Toggle between icon-only and icon-with-label on buttons/toggles. */
int cui_set_icon_only(cui_widget *widget, int icon_only);

/* All calls belong on the main thread. One app may exist at a time.
 * Strings are UTF-8; dimensions are logical units (96 DPI on Windows,
 * points on macOS, GTK logical pixels on Linux).
 * The app owns every window and widget. Closing a window hides it;
 * handles stay valid until app destruction. No per-widget destruction yet.
 */
cui_app *cui_app_create(void); /* NULL when initialization fails. */
void cui_app_run(cui_app *app); /* Returns when quit or last window closes. */
void cui_app_quit(cui_app *app);
void cui_app_destroy(cui_app *app); /* Only after run returns; never in callbacks. */
const char *cui_app_error(const cui_app *app); /* Last error, or empty string. */
void cui_app_set_theme(cui_app *app, cui_theme theme);
/* Extra text scale, independent of display DPI, 0.5..4.0 (default 1).
 * Font sizes are typographic points. NULL/empty family, size 0, weight 0
 * inherit the parent/platform defaults. Weight is 100..900. Installed fonts
 * and platform fallback/shaping are used; no fonts are bundled. */
int cui_app_set_text_scale(cui_app *app, double scale);
int cui_set_font(cui_widget *widget, const char *family, double points, int weight);
double cui_window_scale(const cui_window *window); /* Backing pixels/logical unit. */
/* Repeating UI-thread timer, owned by the app. Stop is idempotent. */
cui_timer *cui_every(cui_app *app, unsigned milliseconds, cui_task task, void *userdata);
void cui_timer_stop(cui_timer *timer);
int cui_timer_start(cui_timer *timer); /* Restart a stopped timer; reuse its interval. */
double cui_time(void); /* Monotonic seconds, arbitrary epoch. */

cui_window *cui_window_create(cui_app *app, const char *title, int width, int height);
cui_widget *cui_window_root(cui_window *window); /* Vertical box, 24 padding, 12 gap. */
void cui_window_show(cui_window *window);
void cui_window_close(cui_window *window);
/* Custom top-level frames. Call on the UI thread; radius is logical pixels.
 * Undecorated windows have transparent backgrounds where supported; content
 * supplies its own close/drag controls. Radius clips the outer window shape.
 * Configure before showing. Native widget appearance is unchanged. */
int cui_window_set_frame(cui_window *window, int decorated, int resizable,
                         double corner_radius);
/* Set the content size in logical pixels (1..4096 per axis). */
int cui_window_set_size(cui_window *window, int width, int height);
/* Best-effort desktop position, logical coordinates. Returns zero on Wayland,
 * where the compositor chooses placement. Does not show a hidden window. */
int cui_window_set_position(cui_window *window, int x, int y);
/* Begin native window dragging from a pointer-press callback. */
int cui_window_begin_move(cui_window *window);
/* Read the current native content size in logical pixels; nonnull outputs.
 * Before mapping, returns the requested size. */
int cui_window_get_size(cui_window *window, int *width, int *height);
/* Begin native corner resizing during a pointer press. Corner: 0 top-left,
 * 1 top-right, 2 bottom-left, 3 bottom-right. Requires a resizable window. */
int cui_window_begin_resize(cui_window *window, int corner);
/* Attach an undecorated auxiliary window beside a rectangle in its parent.
 * Parent and panel must belong to the same app; cycles are rejected. Rectangle
 * uses parent content logical pixels. Does not show the panel. The backend may
 * flip placement to keep it onscreen. Update the anchor when content resizes. */
int cui_window_set_anchor(cui_window *panel, cui_window *parent,
                           int x, int y, int width, int height);
/* close hides a window; show reopens the same window with its state intact. */
int cui_window_is_visible(const cui_window *window);
/* Scroll the document instead of forcing the window to its contents' size. */
void cui_window_set_scrollable(cui_window *window, int scrollable);

/* A parent must be a box or a layout container. New controls are appended in keyboard/layout order.
 * Constructors return NULL on failure. NULL handles are safe for mutators.
 * Boxes stretch children across their axis. Expanding children share spare
 * space along the axis. Content never shrinks below native minimum sizes.
 */
cui_widget *cui_box(cui_widget *parent, cui_axis axis, int gap);
void cui_box_set_padding(cui_widget *box, int padding);
cui_widget *cui_label(cui_widget *parent, const char *text);
cui_widget *cui_button(cui_widget *parent, const char *text);
cui_widget *cui_entry(cui_widget *parent, const char *text);
cui_widget *cui_checkbox(cui_widget *parent, const char *text, int checked);
cui_widget *cui_toggle(cui_widget *parent, const char *text, int checked);
cui_widget *cui_switch(cui_widget *parent, const char *text, int checked);
/* Radio buttons in the same immediate parent form one group. */
cui_widget *cui_radio(cui_widget *parent, const char *text, int checked);
cui_widget *cui_password(cui_widget *parent, const char *text);
cui_widget *cui_search(cui_widget *parent, const char *placeholder);
cui_widget *cui_textarea(cui_widget *parent, const char *text);
cui_widget *cui_code(cui_widget *parent, const char *text); /* Read-only, monospace. */
cui_widget *cui_select(cui_widget *parent, const char *const *items, size_t count);
cui_widget *cui_list(cui_widget *parent, const char *const *items, size_t count);
/* All item/cell strings are copied. Replacing data clears the selection.
 * Tables default to read-only, single selection, with row-major cell arrays.
 * See cui_tables.h for editing, multiple selection and sorting. */
cui_widget *cui_table(cui_widget *parent, const char *const *headers, size_t columns);
int cui_table_set_rows(cui_widget *table, const char *const *cells, size_t rows);
int cui_set_items(cui_widget *widget, const char *const *items, size_t count);
void cui_set_selected(cui_widget *widget, int index); /* -1 clears list/table/select. */
int cui_get_selected(const cui_widget *widget);
cui_widget *cui_slider(cui_widget *parent, double value); /* Normalized 0..1. */
cui_widget *cui_progress(cui_widget *parent, double value);
void cui_set_value(cui_widget *widget, double value); /* Clamp finite values to 0..1. */
double cui_get_value(const cui_widget *widget);
cui_widget *cui_spinner(cui_widget *parent); /* Animate until hidden/destroyed. */
cui_widget *cui_separator(cui_widget *parent);
cui_widget *cui_badge(cui_widget *parent, const char *text, cui_role tone);
cui_widget *cui_chart(cui_widget *parent, const double *values, size_t count);
int cui_chart_set_values(cui_widget *chart, const double *values, size_t count);
cui_widget *cui_image(cui_widget *parent);
/* Copies tightly packed RGBA8 pixels, maximum 4096 x 4096. */
int cui_image_set_rgba(cui_widget *image, const unsigned char *pixels, int width, int height);
void cui_clipboard_set_text(cui_window *window, const char *text);
void cui_set_placeholder(cui_widget *entry, const char *text);
void cui_set_tooltip(cui_widget *widget, const char *text);
void cui_set_min_size(cui_widget *widget, int width, int height);
void cui_set_visible(cui_widget *widget, int visible); /* Hidden items take no space. */

/* Composed controls: portable buttons and boxes with shared state management. */
cui_widget *cui_tabs(cui_widget *parent);
cui_widget *cui_tab_add(cui_widget *tabs, const char *title); /* Returns page box. */
cui_widget *cui_disclosure(cui_widget *parent, const char *title, int expanded);
cui_widget *cui_disclosure_content(cui_widget *disclosure);
void cui_set_expanded(cui_widget *disclosure, int expanded);
int cui_get_expanded(const cui_widget *disclosure);
void cui_expand(cui_widget *widget, int expand);
/* Typography roles apply to labels, PRIMARY to buttons, CARD to boxes.
 * Each backend translates roles to its platform's visual conventions. */
void cui_set_role(cui_widget *widget, cui_role role);

/* Called for user clicks, entry edits, and checkbox toggles. Programmatic
 * setters do not invoke callbacks. Callbacks may update widgets or quit.
 */
void cui_on_action(cui_widget *widget, cui_callback callback, void *userdata);
/* Invoke a button/toggle action synchronously, respecting visibility and
 * enabled ancestors. Useful for application commands and integration tests. */
int cui_activate(cui_widget *widget);
void cui_set_text(cui_widget *widget, const char *text);
/* Returns required bytes excluding NUL. With capacity > 0, always terminates.
 * Short buffers truncate by byte, potentially splitting a UTF-8 character.
 * Use get_text(widget, NULL, 0) + 1 to allocate a complete string.
 */
size_t cui_get_text(const cui_widget *widget, char *buffer, size_t capacity);
/* Selected UTF-8 text; empty when no selection. Same buffer rules as get_text. */
size_t cui_get_selected_text(const cui_widget *widget, char *buffer, size_t capacity);
void cui_set_checked(cui_widget *checkbox, int checked);
int cui_get_checked(const cui_widget *checkbox);
void cui_set_enabled(cui_widget *widget, int enabled);

#ifdef __cplusplus
}
#endif
#endif
