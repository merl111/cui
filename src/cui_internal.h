#ifndef CUI_INTERNAL_H
#define CUI_INTERNAL_H

#include "cui.h"

typedef enum cui_kind {
    CUI_BOX, CUI_LABEL, CUI_BUTTON, CUI_ENTRY, CUI_CHECKBOX,
    CUI_TOGGLE, CUI_SWITCH, CUI_RADIO, CUI_PASSWORD, CUI_SEARCH,
    CUI_TEXTAREA, CUI_CODE, CUI_SELECT, CUI_LIST, CUI_TABLE,
    CUI_SLIDER, CUI_PROGRESS, CUI_SPINNER, CUI_SEPARATOR, CUI_BADGE, CUI_CHART, CUI_IMAGE, CUI_GRID, CUI_WRAP, CUI_SPLIT, CUI_TREE, CUI_NUMBER, CUI_DATE, CUI_TIME_INPUT, CUI_ICON, CUI_CANVAS, CUI_STACK
} cui_kind;
typedef enum cui_component { CUI_COMPONENT_NONE, CUI_COMPONENT_TABS, CUI_COMPONENT_DISCLOSURE, CUI_COMPONENT_FIELD, CUI_COMPONENT_BREADCRUMBS } cui_component;

typedef struct cui_size { float width, height; } cui_size;
typedef struct cui_rect { float x, y, width, height; } cui_rect;

struct cui_widget {
    cui_kind kind;
    cui_axis axis;
    cui_role role;
    cui_widget_style style;
    int styled;
    cui_icon_asset *icon;
    int icon_only, icon_size, icon_trailing;
    cui_window *window;
    cui_widget *parent, *first, *last, *next;
    void *native, *aux, *font_native;
    char *font_family, *placeholder;
    double font_points;
    int font_weight;
    int font_style; /* 0 inherits, 1 upright, 2 italic. */
    char **items, **headers;
    size_t item_count, columns;
    int selected, hidden, min_width, min_height, index;
    double value, opacity;
    cui_component component;
    struct cui_command *command;
    cui_widget *content;
    void *payload;
    void (*destroy_payload)(void *);
    double *series;
    size_t series_count;
    unsigned char *pixels;
    int image_width, image_height;
    cui_callback callback;
    void *userdata;
    cui_key_callback key_callback;
    void *key_userdata;
    int composing;
    int gap, padding, expand, updating, enabled, read_only;
    int layer_alignment, layer_width, layer_height, layer_margin;
    unsigned grid_columns, grid_row, grid_column, grid_row_span, grid_column_span;
    cui_size minimum;
    cui_rect frame;
};

struct cui_window {
    cui_app *app;
    cui_window *next;
    cui_widget *root;
    void *native, *content, *font;
    int width, height, visible, laying_out;
    float scale;
    int decorated, resizable, popup;
    cui_window *anchor_parent;
    int anchor_x, anchor_y, anchor_width, anchor_height;
    void *attached_native;
    double corner_radius;
    void *pointer_event; /* Borrowed only during a native press callback. */
    int scrollable;
    float scroll_x, scroll_y, document_width, document_height;
    struct cui_menu *menu;
    void *menu_native;
};

struct cui_app {
    cui_window *windows;
    void *native;
    const char *error;
    int running, in_run, destroying;
    cui_theme theme;
    double text_scale;
    cui_timer *timers;
    struct cui_dialog *dialogs;
    struct cui_command *commands;
    struct cui_menu *menus;
    void *desktop_native;
    unsigned next_command;
};
struct cui_timer {
    cui_app *app;
    cui_timer *next;
    unsigned interval;
    cui_task task;
    void *userdata, *native;
    int active;
};

static inline int cui__container(const cui_widget *w)
{ return w && (w->kind == CUI_BOX || w->kind == CUI_GRID || w->kind == CUI_WRAP || w->kind == CUI_SPLIT || w->kind == CUI_STACK); }
static inline int cui__in_layer(const cui_widget *w)
{
    for(const cui_widget *c=w;c&&c->parent;c=c->parent)
        if(c->parent->kind==CUI_STACK && c->parent->first && c!=c->parent->first)return 1;
    return 0;
}
int cui__grid_default_slot(const cui_widget *grid, cui_widget *child);
void cui__backend_container(cui_widget *widget);
void cui__backend_grid_cell(cui_widget *widget);
void cui__backend_split_position(cui_widget *widget);
cui_size cui__layout_measure(cui_widget *widget);
void cui__layout_arrange(cui_widget *widget, cui_rect rect);
int cui__backend_init(cui_app *app);
int cui__backend_keys(cui_widget *widget);
int cui__key(cui_widget *widget,cui_key key,unsigned modifiers);
int cui__backend_hover(const cui_widget *widget);
char *cui__search_key(const char *text);
void cui__backend_announce(cui_widget *widget,const char *text,int urgent);
void cui__backend_shutdown(cui_app *app);
void cui__backend_run(cui_app *app);
void cui__backend_quit(cui_app *app);
void cui__backend_theme(cui_app *app);
cui_theme cui__backend_resolved_theme(cui_app *app);
int cui__backend_window_create(cui_window *window, const char *title);
int cui__backend_window_frame(cui_window *window);
int cui__backend_window_size(cui_window *window);
int cui__backend_window_position(cui_window *window, int x, int y);
int cui__backend_window_move(cui_window *window);
int cui__backend_window_get_size(cui_window *window, int *width, int *height);
int cui__backend_window_resize(cui_window *window, int corner);
int cui__backend_window_anchor(cui_window *window);
int cui__backend_popup_anchor(cui_window *panel,cui_widget *anchor,double x,double y,double width,double height);
void cui__backend_window_destroy(cui_window *window);
void cui__backend_window_show(cui_window *window);
void cui__backend_window_hide(cui_window *window);
void cui__backend_refresh(cui_window *window);
int cui__backend_widget_create(cui_widget *widget, const char *text);
void cui__backend_expand(cui_widget *widget);
void cui__backend_padding(cui_widget *widget);
void cui__backend_role(cui_widget *widget);
void cui__backend_style(cui_widget *widget);
void cui__backend_set_text(cui_widget *widget, const char *text);
size_t cui__backend_get_text(const cui_widget *widget, char *buffer, size_t capacity);
void cui__backend_set_checked(cui_widget *widget, int checked);
int cui__backend_get_checked(const cui_widget *widget);
void cui__backend_set_enabled(cui_widget *widget, int enabled);
cui_size cui__backend_measure(cui_widget *widget);
void cui__backend_place(cui_widget *widget);
void cui__backend_scrollable(cui_window *window);
void cui__backend_visible(cui_widget *widget, int visible);
void cui__backend_min_size(cui_widget *widget);
void cui__backend_items(cui_widget *widget);
void cui__backend_set_selected(cui_widget *widget, int index);
int cui__backend_get_selected(const cui_widget *widget);
void cui__backend_set_value(cui_widget *widget, double value);
double cui__backend_get_value(const cui_widget *widget);
void cui__backend_placeholder(cui_widget *widget, const char *text);
void cui__backend_tooltip(cui_widget *widget, const char *text);
void cui__backend_font(cui_widget *widget);
void cui__backend_font_free(cui_widget *widget);
void cui__font_tree(cui_widget *widget);
int cui__font_italic(const cui_widget *widget);
void cui__font_resolve(cui_widget *widget, const char **family, double *points, int *weight);
int cui__backend_timer(cui_timer *timer);
void cui__backend_timer_stop(cui_timer *timer);
void cui__backend_media(cui_widget *widget);
void cui__backend_icon(cui_widget *widget);
struct cui_icon_asset {
    size_t refs, count;
    float width, height;
    cui_icon_command *commands;
    unsigned char *pixels;
};
typedef void (*cui_icon_path)(void *context, const float *xy, size_t count, int fill);
void cui__draw_icon(cui_symbol symbol, cui_icon_path path, void *context);

cui_widget *cui__append(cui_widget *parent, cui_kind kind, const char *text, cui_axis axis, int gap);
void cui__free_strings(char **strings, size_t count);
int cui__is_checkable(const cui_widget *widget);
int cui__has_text(const cui_widget *widget);
void cui__tabs_select(cui_widget *tabs, int index);

int cui__contains_focus(cui_widget *widget);
void cui__emit(cui_widget *widget);
size_t cui__copy_text(const char *text, char *buffer, size_t capacity);
cui_size cui__measure(cui_widget *widget);
void cui__arrange(cui_widget *widget, cui_rect rect);
void cui__layout(cui_window *window, float width, float height);

#endif
