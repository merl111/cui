#include "cui_desktop_internal.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

static cui_app *active_app;

static void free_widgets(cui_widget *widget)
{
    while (widget) {
        cui_widget *next = widget->next;
        free_widgets(widget->first);
        cui__free_strings(widget->items, widget->item_count);
        cui__free_strings(widget->headers, widget->columns);
        if (widget->destroy_payload) widget->destroy_payload(widget->payload);
        free(widget->series); free(widget->pixels); cui_icon_release(widget->icon);
        cui__backend_font_free(widget); free(widget->font_family); free(widget->placeholder);
        free(widget);
        widget = next;
    }
}

static int has_visible_windows(const cui_app *app)
{
    const cui_window *window;
    for (window = app->windows; window; window = window->next)
        if (window->visible) return 1;
    return 0;
}

cui_app *cui_app_create(void)
{
    cui_app *app;
    if (active_app) return NULL;
    app = (cui_app *)calloc(1, sizeof(*app));
    if (!app) return NULL;
    app->error = ""; app->text_scale = 1;
    if (!cui__backend_init(app)) { free(app); return NULL; }
    active_app = app;
    return app;
}

void cui_app_run(cui_app *app)
{
    if (!app || app->in_run || !has_visible_windows(app)) return;
    app->in_run = 1;
    app->running = 1;
    cui__backend_run(app);
    app->running = 0;
    app->in_run = 0;
}

void cui_app_quit(cui_app *app)
{
    if (app && app->running) {
        app->running = 0;
        cui__backend_quit(app);
    }
}

void cui_app_destroy(cui_app *app)
{
    cui_window *window;
    if (!app) return;
    if (app->in_run) { app->error = "Destroy the app after cui_app_run returns"; return; }
    app->destroying = 1;
    while (app->timers) {
        cui_timer *timer = app->timers;
        cui_timer_stop(timer); app->timers = timer->next; free(timer);
    }
    cui__desktop_destroy(app);
    cui__desktop_dispose(app);
    for(window=app->windows;window;window=window->next) {
        if(window->anchor_parent) cui__backend_window_hide(window);
    }
    window = app->windows;
    while (window) {
        cui_window *next = window->next;
        app->windows = next;
        cui__backend_window_destroy(window);
        free_widgets(window->root);
        free(window);
        window = next;
    }
    cui__desktop_free(app);
    cui__backend_shutdown(app);
    active_app = NULL;
    free(app);
}

const char *cui_app_error(const cui_app *app)
{
    return app ? app->error : "No application";
}

cui_theme cui_app_resolved_theme(cui_app *app)
{
    if (!app) return CUI_THEME_LIGHT;
    if (app->theme != CUI_THEME_SYSTEM) return app->theme;
    return cui__backend_resolved_theme(app);
}

void cui_app_set_theme(cui_app *app, cui_theme theme)
{
    if (!app || theme < CUI_THEME_SYSTEM || theme > CUI_THEME_DARK) return;
    app->theme = theme;
    cui__backend_theme(app);
}

void cui_app_set_background(cui_app *app, int enabled)
{
    if (app) app->background = !!enabled;
}

void cui_app_set_focus_indicators(cui_app *app, int visible)
{
    if (!app) return;
    app->hide_focus = !visible;
    cui__backend_theme(app);
    for (cui_window *w = app->windows; w; w = w->next) cui__backend_refresh(w);
}

static void update_enabled(cui_widget *widget)
{
    cui_widget *parent, *child;
    int enabled = widget->enabled;
    for (parent = widget->parent; parent; parent = parent->parent)
        enabled = enabled && parent->enabled;
    cui__backend_set_enabled(widget, enabled);
    for (child = widget->first; child; child = child->next) update_enabled(child);
}

static cui_widget *create_widget(cui_window *window, cui_widget *parent,
                                 cui_kind kind, const char *text,
                                 cui_axis axis, int gap)
{
    cui_widget *widget = (cui_widget *)calloc(1, sizeof(*widget));
    if (!widget) { window->app->error = "Out of memory"; return NULL; }
    widget->window = window;
    widget->parent = parent;
    widget->kind = kind;
    widget->axis = axis;
    widget->gap = gap;
    widget->enabled = 1; widget->opacity = 1;
    widget->selected = -1;
    widget->grid_row_span = widget->grid_column_span = 1;
    if (parent && parent->kind == CUI_GRID && !cui__grid_default_slot(parent, widget)) {
        window->app->error = "Grid has no free cells";
        free(widget); return NULL;
    }
    if (!cui__backend_widget_create(widget, text ? text : "")) {
        window->app->error = "Could not create native control";
        free(widget);
        return NULL;
    }
    if (parent) {
        if (parent->last) parent->last->next = widget;
        else parent->first = widget;
        parent->last = widget;
    }
    cui__backend_font(widget);
    update_enabled(widget);
    if (parent) {
        cui_widget *ancestor;
        for (ancestor = parent; ancestor; ancestor = ancestor->parent)
            if (ancestor->hidden) { cui__backend_visible(widget, 0); break; }
    }
    return widget;
}

cui_window *cui_window_create(cui_app *app, const char *title, int width, int height)
{
    cui_window *window;
    if (!app) return NULL;
    if (width <= 0 || height <= 0) { app->error = "Window size must be positive"; return NULL; }
    window = (cui_window *)calloc(1, sizeof(*window));
    if (!window) { app->error = "Out of memory"; return NULL; }
    window->app = app;
    window->width = width;
    window->height = height;
    window->scale = 1;
    window->decorated = window->resizable = 1;
    if (!cui__backend_window_create(window, title ? title : "")) {
        app->error = "Could not create native window";
        free(window);
        return NULL;
    }
    window->root = create_widget(window, NULL, CUI_BOX, "", CUI_VERTICAL, 12);
    if (!window->root) { cui__backend_window_destroy(window); free(window); return NULL; }
    window->root->padding = 24;
    cui__backend_padding(window->root);
    window->next = app->windows;
    app->windows = window;
    cui__desktop_window(window);
    return window;
}

cui_widget *cui_window_root(cui_window *window) { return window ? window->root : NULL; }

void cui_window_show(cui_window *window)
{
    if (!window) return;
    window->visible = 1;
    cui__backend_refresh(window);
    cui__backend_window_show(window);
}

void cui_window_close(cui_window *window)
{
    if (!window || !window->visible) return;
    window->visible = 0;
    for(cui_window *child=window->app->windows;child;child=child->next)
        if(child->anchor_parent==window)cui_window_close(child);
    cui__backend_window_hide(window);
    if (!has_visible_windows(window->app) && !window->app->background) cui_app_quit(window->app);
}

int cui_window_set_frame(cui_window *w, int decorated, int resizable, double radius)
{
    if (!w || !isfinite(radius) || radius < 0 || radius > 2048) return 0;
    w->decorated = !!decorated; w->resizable = !!resizable; w->corner_radius = radius;
    return cui__backend_window_frame(w);
}
int cui_window_set_size(cui_window *w, int width, int height)
{
    if (!w || width < 1 || height < 1 || width > 4096 || height > 4096) return 0;
    w->width = width; w->height = height;
    return cui__backend_window_size(w);
}
int cui_window_set_position(cui_window *w, int x, int y)
{
    if (!w || w->anchor_parent || x < -32768 || y < -32768 || x > 32767 || y > 32767) return 0;
    return cui__backend_window_position(w,x,y);
}
int cui_window_begin_move(cui_window *w)
{ return w && !w->anchor_parent ? cui__backend_window_move(w) : 0; }
int cui_window_is_visible(const cui_window *w)
{ return w ? w->visible : 0; }

cui_widget *cui__append(cui_widget *parent, cui_kind kind,
                                 const char *text, cui_axis axis, int gap)
{
    cui_widget *widget;
    if (!parent || !cui__container(parent)) return NULL;
    if (parent->kind == CUI_SPLIT && parent->first && parent->first->next) return NULL;
    widget = create_widget(parent->window, parent, kind, text, axis, gap);
    if (widget) cui__backend_refresh(parent->window);
    return widget;
}

cui_widget *cui_box(cui_widget *parent, cui_axis axis, int gap)
{
    if ((axis != CUI_HORIZONTAL && axis != CUI_VERTICAL) || gap < 0) return NULL;
    return cui__append(parent, CUI_BOX, "", axis, gap);
}

void cui_box_set_padding(cui_widget *box, int padding)
{
    if (!box || !cui__container(box) || padding < 0) return;
    box->padding = padding;
    cui__backend_padding(box);
    cui__backend_refresh(box->window);
}

cui_widget *cui_label(cui_widget *parent, const char *text)
{ return cui__append(parent, CUI_LABEL, text, CUI_VERTICAL, 0); }
cui_widget *cui_button(cui_widget *parent, const char *text)
{ return cui__append(parent, CUI_BUTTON, text, CUI_VERTICAL, 0); }
cui_widget *cui_entry(cui_widget *parent, const char *text)
{ return cui__append(parent, CUI_ENTRY, text, CUI_VERTICAL, 0); }

void cui_expand(cui_widget *widget, int expand)
{
    if (!widget) return;
    widget->expand = !!expand;
    cui__backend_expand(widget);
    cui__backend_refresh(widget->window);
}

void cui_on_action(cui_widget *widget, cui_callback callback, void *userdata)
{
    if (!widget) return;
    widget->callback = callback;
    widget->userdata = userdata;
}

void cui_set_role(cui_widget *widget, cui_role role)
{
    if (!widget || role < CUI_ROLE_BODY || role > CUI_ROLE_AMBIENT) return;
    if ((role == CUI_ROLE_CARD || role == CUI_ROLE_AMBIENT) && !cui__container(widget)) return;
    if (role == CUI_ROLE_PRIMARY && widget->kind != CUI_BUTTON && widget->kind != CUI_TOGGLE) return;
    if (role >= CUI_ROLE_TITLE && role <= CUI_ROLE_CAPTION && widget->kind != CUI_LABEL) return;
    widget->role = role;
    cui__backend_role(widget);
    cui__font_tree(widget);
    cui__backend_refresh(widget->window);
}

void cui__emit(cui_widget *widget)
{
    if (widget->updating || widget->window->app->destroying) return;
    if (widget->kind == CUI_RADIO) {
        /* A selected radio cannot be toggled off by activating it again. */
        cui_set_checked(widget, 1);
    }
    if (!widget->updating && widget->callback)
        widget->callback(widget, widget->userdata);
}

void cui_set_text(cui_widget *widget, const char *text)
{
    if(widget && widget->icon_only && (!text||!*text))return;
    if (!widget || !cui__has_text(widget)) return;
    ++widget->updating;
    cui__backend_set_text(widget, text ? text : "");
    if(widget->icon_only){cui_set_tooltip(widget,text);cui_accessibility(widget,text,text);}
    --widget->updating;
    cui__backend_refresh(widget->window);
}

int cui_insert_text(cui_widget *w, const char *text)
{
    if (!w || w->read_only || !(w->kind == CUI_ENTRY || w->kind == CUI_SEARCH ||
        w->kind == CUI_TEXTAREA || w->kind == CUI_CODE)) return 0;
    ++w->updating;
    int result = cui__backend_insert_text(w, text ? text : "");
    --w->updating;
    return result;
}

size_t cui_get_text(const cui_widget *widget, char *buffer, size_t capacity)
{
    if (!widget || !cui__has_text(widget)) return cui__copy_text("", buffer, capacity);
    return cui__backend_get_text(widget, buffer, capacity);
}

size_t cui__copy_text(const char *text, char *buffer, size_t capacity)
{
    size_t length = strlen(text);
    if (buffer && capacity) {
        size_t count = length < capacity - 1 ? length : capacity - 1;
        memcpy(buffer, text, count);
        buffer[count] = '\0';
    }
    return length;
}

void cui_set_checked(cui_widget *widget, int checked)
{
    cui_widget *sibling;
    if (!cui__is_checkable(widget)) return;
    if (widget->kind == CUI_RADIO && checked)
        for (sibling = widget->parent->first; sibling; sibling = sibling->next)
            if (sibling != widget && sibling->kind == CUI_RADIO) cui_set_checked(sibling, 0);
    ++widget->updating;
    cui__backend_set_checked(widget, !!checked);
    --widget->updating;
}

int cui_get_checked(const cui_widget *widget)
{
    return cui__is_checkable(widget) ? cui__backend_get_checked(widget) : 0;
}

void cui_set_enabled(cui_widget *widget, int enabled)
{
    if (!widget) return;
    widget->enabled = !!enabled;
    update_enabled(widget);
}

int cui_window_get_size(cui_window *w, int *width, int *height)
{
    if (!w || !width || !height) return 0;
    if (!w->visible) { *width=w->width; *height=w->height; return 1; }
    return cui__backend_window_get_size(w,width,height);
}
int cui_window_begin_resize(cui_window *w, int corner)
{
    if (!w || w->anchor_parent || !w->visible || !w->resizable || corner<0 || corner>3) return 0;
    return cui__backend_window_resize(w,corner);
}

int cui_window_set_anchor(cui_window *panel,cui_window *parent,int x,int y,int width,int height)
{
    if(!panel || !parent || panel->app!=parent->app || panel->decorated ||
       x<0 || y<0 || width<1 || height<1 || x>4096 || y>4096 || width>4096 || height>4096)return 0;
    for(cui_window *p=parent;p;p=p->anchor_parent)if(p==panel)return 0;
    panel->popup=0;
    panel->anchor_parent=parent;panel->anchor_x=x;panel->anchor_y=y;
    panel->anchor_width=width;panel->anchor_height=height;
    return cui__backend_window_anchor(panel);
}

int cui_textarea_set_height(cui_widget *w, int height)
{
    if (!w || w->kind != CUI_TEXTAREA || height < 24 || height > 2048) return 0;
    if (w->min_height != height) cui_set_min_size(w, w->min_width, height);
    return 1;
}

int cui_set_style(cui_widget *w,const cui_widget_style *style)
{
    if(!w)return 0;
    if(style&&(!isfinite(style->radius)||!isfinite(style->border_width)||style->radius<0||style->radius>128||style->border_width<0||style->border_width>128||style->padding<0||style->padding>128))return 0;
    if(w->styled&&style&&w->style.background==style->background&&w->style.foreground==style->foreground&&w->style.border==style->border&&w->style.radius==style->radius&&w->style.border_width==style->border_width&&w->style.padding==style->padding)return 1;
    w->styled=style!=NULL;if(style)w->style=*style;
    cui__backend_style(w);cui__backend_refresh(w->window);return 1;
}
