#include "cui_internal.h"
#include "cui_tables_internal.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

int cui__is_checkable(const cui_widget *w)
{ return w && (w->kind == CUI_CHECKBOX || w->kind == CUI_TOGGLE || w->kind == CUI_SWITCH || w->kind == CUI_RADIO); }

int cui_activate(cui_widget *w)
{
    cui_widget *ancestor;
    if (!w || (w->kind != CUI_BUTTON && !cui__is_checkable(w))) return 0;
    for (ancestor = w; ancestor; ancestor = ancestor->parent)
        if (ancestor->hidden || !ancestor->enabled) return 0;
    if (cui__is_checkable(w)) cui_set_checked(w, w->kind == CUI_RADIO || !cui_get_checked(w));
    cui__emit(w);
    return 1;
}

int cui__has_text(const cui_widget *w)
{
    return w->kind == CUI_LABEL || w->kind == CUI_BUTTON || w->kind == CUI_ENTRY ||
           cui__is_checkable(w) || w->kind == CUI_PASSWORD || w->kind == CUI_SEARCH ||
           w->kind == CUI_TEXTAREA || w->kind == CUI_CODE || w->kind == CUI_BADGE;
}

static cui_widget *check_control(cui_widget *parent, cui_kind kind, const char *text, int checked)
{
    cui_widget *w = cui__append(parent, kind, text, CUI_VERTICAL, 0);
    cui_set_checked(w, checked);
    return w;
}
cui_widget *cui_checkbox(cui_widget *p, const char *t, int v) { return check_control(p, CUI_CHECKBOX, t, v); }
cui_widget *cui_toggle(cui_widget *p, const char *t, int v) { return check_control(p, CUI_TOGGLE, t, v); }
cui_widget *cui_switch(cui_widget *p, const char *t, int v) { return check_control(p, CUI_SWITCH, t, v); }
cui_widget *cui_radio(cui_widget *p, const char *t, int v) { return check_control(p, CUI_RADIO, t, v); }
cui_widget *cui_password(cui_widget *p, const char *t) { return cui__append(p, CUI_PASSWORD, t, CUI_VERTICAL, 0); }
cui_widget *cui_textarea(cui_widget *p, const char *t) { return cui__append(p, CUI_TEXTAREA, t, CUI_VERTICAL, 0); }
cui_widget *cui_code(cui_widget *p, const char *t) { return cui__append(p, CUI_CODE, t, CUI_VERTICAL, 0); }
cui_widget *cui_spinner(cui_widget *p) { return cui__append(p, CUI_SPINNER, "", CUI_VERTICAL, 0); }
cui_widget *cui_separator(cui_widget *p) { return cui__append(p, CUI_SEPARATOR, "", CUI_VERTICAL, 0); }
cui_widget *cui_search(cui_widget *p, const char *hint)
{
    cui_widget *w = cui__append(p, CUI_SEARCH, "", CUI_VERTICAL, 0);
    cui_set_placeholder(w, hint);
    return w;
}
cui_widget *cui_badge(cui_widget *p, const char *text, cui_role tone)
{
    cui_widget *w = cui__append(p, CUI_BADGE, text, CUI_VERTICAL, 0);
    cui_set_role(w, tone);
    return w;
}

void cui__free_strings(char **strings, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i) free(strings[i]);
    free(strings);
}

static char **copy_strings(const char *const *items, size_t count)
{
    char **copy;
    size_t i;
    if (!items || count > INT_MAX) return NULL;
    copy = (char **)calloc(count, sizeof(*copy));
    if (!copy) return NULL;
    for (i = 0; i < count; ++i) {
        const char *text = items[i] ? items[i] : "";
        copy[i] = (char *)malloc(strlen(text) + 1);
        if (!copy[i]) { cui__free_strings(copy, i); return NULL; }
        strcpy(copy[i], text);
    }
    return copy;
}

static int replace_items(cui_widget *w, const char *const *items, size_t count)
{
    char **copy = count ? copy_strings(items, count) : NULL;
    char **previous = w->items;
    size_t previous_count = w->item_count;
    if (count && !copy) { w->window->app->error = "Invalid items or out of memory"; return 0; }
    if (w->kind == CUI_TABLE && !cui__table_resize(w, count / w->columns)) { cui__free_strings(copy, count); return 0; }
    ++w->updating;
    w->items = copy; w->item_count = count; w->selected = -1;
    cui__backend_items(w);
    if (w->kind == CUI_TABLE) cui__backend_table_selection(w);
    else cui__backend_set_selected(w, -1);
    --w->updating;
    cui__free_strings(previous, previous_count);
    cui__backend_refresh(w->window);
    return 1;
}

int cui_set_items(cui_widget *w, const char *const *items, size_t count)
{
    if (!w || (w->kind != CUI_SELECT && w->kind != CUI_LIST)) return 0;
    return replace_items(w, items, count);
}
static cui_widget *choice(cui_widget *p, cui_kind kind, const char *const *items, size_t count)
{
    cui_widget *w = cui__append(p, kind, "", CUI_VERTICAL, 0);
    return w && cui_set_items(w, items, count) ? w : NULL;
}
cui_widget *cui_select(cui_widget *p, const char *const *items, size_t n) { return choice(p, CUI_SELECT, items, n); }
cui_widget *cui_list(cui_widget *p, const char *const *items, size_t n) { return choice(p, CUI_LIST, items, n); }
cui_widget *cui_table(cui_widget *p, const char *const *headers, size_t columns)
{
    cui_widget *w;
    char **copy;
    if (!columns || columns > 64) return NULL;
    copy = copy_strings(headers, columns);
    if (!copy) return NULL;
    w = cui__append(p, CUI_TABLE, "", CUI_VERTICAL, 0);
    if (!w) { cui__free_strings(copy, columns); return NULL; }
    w->headers = copy; w->columns = columns;
    if (!cui__table_init(w)) return NULL;
    ++w->updating; cui__backend_items(w); --w->updating;
    return w;
}
int cui_table_set_rows(cui_widget *w, const char *const *cells, size_t rows)
{
    if (!w || w->kind != CUI_TABLE || !w->columns || rows > (size_t)INT_MAX / w->columns) return 0;
    return replace_items(w, cells, rows * w->columns);
}
void cui_set_selected(cui_widget *w, int index)
{
    size_t count;
    if (!w) return;
    if (w->kind == CUI_TABLE) { cui__table_select(w, index); return; }
    if (w->component == CUI_COMPONENT_TABS) { cui__tabs_select(w, index); return; }
    if (w->kind != CUI_SELECT && w->kind != CUI_LIST && w->kind != CUI_TABLE) return;
    count = w->kind == CUI_TABLE ? (w->columns ? w->item_count / w->columns : 0) : w->item_count;
    if (index < -1 || (index >= 0 && (size_t)index >= count)) return;
    ++w->updating;
    cui__backend_set_selected(w, index);
    --w->updating;
}
int cui_get_selected(const cui_widget *w)
{
    if (!w) return -1;
    if (w->component == CUI_COMPONENT_TABS || w->kind == CUI_TABLE) return w->selected;
    if (w->kind != CUI_SELECT && w->kind != CUI_LIST && w->kind != CUI_TABLE) return -1;
    return cui__backend_get_selected(w);
}
void cui_set_value(cui_widget *w, double value)
{
    if (!w || (w->kind != CUI_SLIDER && w->kind != CUI_PROGRESS) || !isfinite(value)) return;
    w->value = value < 0 ? 0 : value > 1 ? 1 : value;
    ++w->updating;
    cui__backend_set_value(w, w->value);
    --w->updating;
}
double cui_get_value(const cui_widget *w)
{ return w && (w->kind == CUI_SLIDER || w->kind == CUI_PROGRESS) ? cui__backend_get_value(w) : 0; }
static cui_widget *meter(cui_widget *p, cui_kind kind, double value)
{
    cui_widget *w = cui__append(p, kind, "", CUI_VERTICAL, 0);
    cui_set_value(w, value);
    return w;
}
cui_widget *cui_slider(cui_widget *p, double value) { return meter(p, CUI_SLIDER, value); }
cui_widget *cui_progress(cui_widget *p, double value) { return meter(p, CUI_PROGRESS, value); }

void cui_set_placeholder(cui_widget *w, const char *text)
{
    if (!w || (w->kind != CUI_ENTRY && w->kind != CUI_PASSWORD && w->kind != CUI_SEARCH)) return;
    cui__backend_placeholder(w, text ? text : "");
}
void cui_set_tooltip(cui_widget *w, const char *text)
{ if (w) cui__backend_tooltip(w, text ? text : ""); }
void cui_set_min_size(cui_widget *w, int width, int height)
{
    if (!w || width < 0 || height < 0) return;
    w->min_width = width; w->min_height = height;
    cui__backend_min_size(w);
    cui__backend_refresh(w->window);
}
static void update_visible(cui_widget *w, int ancestor_visible)
{
    cui_widget *child;
    int visible = ancestor_visible && !w->hidden;
    cui__backend_visible(w, visible);
    for (child = w->first; child; child = child->next) update_visible(child, visible);
}
void cui_set_visible(cui_widget *w, int visible)
{
    cui_widget *parent;
    int ancestor_visible = 1;
    if (!w) return;
    w->hidden = !visible;
    for (parent = w->parent; parent; parent = parent->parent) ancestor_visible &= !parent->hidden;
    update_visible(w, ancestor_visible);
    cui__backend_refresh(w->window);
}
void cui_window_set_scrollable(cui_window *w, int scrollable)
{
    if (!w || w->scrollable == !!scrollable) return;
    w->scrollable = !!scrollable;
    w->scroll_x = w->scroll_y = 0;
    cui__backend_scrollable(w);
    cui__backend_refresh(w);
}

cui_timer *cui_every(cui_app *app, unsigned milliseconds, cui_task task, void *userdata)
{
    if (!app || !task || milliseconds < 10 || milliseconds > 86400000) return NULL;
    cui_timer *timer = (cui_timer *)calloc(1, sizeof(*timer));
    if (!timer) return NULL;
    timer->app = app; timer->interval = milliseconds; timer->task = task; timer->userdata = userdata; timer->active = 1;
    if (!cui__backend_timer(timer)) { free(timer); return NULL; }
    timer->next = app->timers; app->timers = timer;
    return timer;
}
void cui_timer_stop(cui_timer *timer)
{
    if (!timer || !timer->active) return;
    timer->active = 0; cui__backend_timer_stop(timer);
}
int cui_timer_start(cui_timer *timer)
{
    if(!timer || timer->app->destroying)return 0;
    if(timer->active)return 1;
    if(!cui__backend_timer(timer))return 0;
    timer->active=1;return 1;
}
cui_widget *cui_chart(cui_widget *parent, const double *values, size_t count)
{
    cui_widget *w = cui__append(parent, CUI_CHART, "", CUI_VERTICAL, 0);
    return w && cui_chart_set_values(w, values, count) ? w : NULL;
}
int cui_chart_set_values(cui_widget *w, const double *values, size_t count)
{
    size_t i;
    double *copy = NULL;
    if (!w || w->kind != CUI_CHART || count > 65536 || (count && !values)) return 0;
    for (i = 0; i < count; ++i) if (!isfinite(values[i])) return 0;
    if (count) { copy = (double *)malloc(count * sizeof(*copy)); if (!copy) return 0; memcpy(copy, values, count * sizeof(*copy)); }
    free(w->series); w->series = copy; w->series_count = count;
    cui__backend_media(w); return 1;
}
cui_widget *cui_image(cui_widget *parent) { return cui__append(parent, CUI_IMAGE, "", CUI_VERTICAL, 0); }
int cui_image_set_rgba(cui_widget *w, const unsigned char *pixels, int width, int height)
{
    if (!w || (w->kind != CUI_IMAGE && w->kind != CUI_CANVAS) || !pixels || width <= 0 || height <= 0 || width > 4096 || height > 4096) return 0;
    size_t bytes = (size_t)width * (size_t)height * 4;
    unsigned char *copy = (unsigned char *)malloc(bytes);
    if (!copy) return 0;
    memcpy(copy, pixels, bytes); free(w->pixels); w->pixels = copy;
    w->image_width = width; w->image_height = height;
    cui__backend_media(w); return 1;
}

void cui__font_resolve(cui_widget *w, const char **family, double *points, int *weight)
{
    *family = NULL; *points = 0; *weight = 0;
    for (; w; w = w->parent) {
        if (!*family && w->font_family) *family = w->font_family;
        if (!*points && w->font_points) *points = w->font_points;
        if (!*weight && w->font_weight) *weight = w->font_weight;
    }
}
int cui__font_italic(const cui_widget *w)
{
    for(;w;w=w->parent)if(w->font_style)return w->font_style==2;
    return 0;
}
void cui__font_tree(cui_widget *w)
{
    if (!w) return;
    cui__backend_font(w);
    for (cui_widget *child = w->first; child; child = child->next) cui__font_tree(child);
}
int cui_set_font(cui_widget *w, const char *family, double points, int weight)
{
    char *copy = NULL;
    if (!w || !isfinite(points) || (points != 0 && (points < 6 || points > 200)) ||
        (weight != 0 && (weight < 100 || weight > 900))) return 0;
    if (family && *family) {
        if (strlen(family) > 128) return 0;
        for (const unsigned char *p = (const unsigned char *)family; *p; ++p)
            if (*p < 32 || *p == '\\' || *p == '"') return 0;
        copy = (char *)malloc(strlen(family) + 1); if (!copy) return 0; strcpy(copy, family);
    }
    free(w->font_family); w->font_family = copy; w->font_points = points; w->font_weight = weight;
    cui__font_tree(w); cui__backend_refresh(w->window); return 1;
}
int cui_app_set_text_scale(cui_app *app, double scale)
{
    if (!app || !isfinite(scale) || scale < 0.5 || scale > 4) return 0;
    app->text_scale = scale;
    for (cui_window *w = app->windows; w; w = w->next) { cui__font_tree(w->root); cui__backend_refresh(w); }
    return 1;
}
