#include "cui_gtk.h"
#include <string.h>
#include <math.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/extensions/shape.h>
#endif

typedef struct gtk_app_state {
    GMainLoop *loop;
    GtkCssProvider *css;
    GDBusProxy *portal;
    gulong settings_handler;
    guint color_scheme;
} gtk_app_state;

static void load_css(GtkCssProvider *provider, const char *css)
{
#if GTK_CHECK_VERSION(4, 12, 0)
    gtk_css_provider_load_from_string(provider, css);
#else
    gtk_css_provider_load_from_data(provider, css, -1);
#endif
}

/* Scoped to our windows: never replace the desktop's global theme. Native
 * controls still implement selection, IME, focus, keyboard and accessibility.
 * User styles (priority 800) can override these application styles (600). */
static const char style[] =
    "window.cui-custom-frame, window.cui-custom-frame:backdrop { background: transparent; background-image: none; border: none; padding: 0; margin: 0; box-shadow: none; outline: none; }"
    "window.cui-custom-frame decoration { background: transparent; border: none; box-shadow: none; outline: none; }"
    "popover.cui-attached-panel, popover.cui-attached-panel.cui-window, popover.cui-attached-panel.cui-window.cui-dark, .cui-attached-panel > contents { background: transparent; background-image: none; border: none; padding: 0; margin: 0; box-shadow: none; }"
    ".cui-canvas, .cui-canvas:focus, .cui-canvas:focus-visible { outline: none; border: none; box-shadow: none; }"

    ".cui-window .cui-icon-view:disabled { opacity: 0.45; }"
    ".cui-window .cui-ambient { background-color: #f3f3fa; background-image: radial-gradient(ellipse at 0% 15%, alpha(#afa3f5,0.65), alpha(#afa3f5,0) 65%), radial-gradient(ellipse at 95% 85%, alpha(#65cfc5,0.55), alpha(#65cfc5,0) 65%), linear-gradient(135deg,#f6e9f3,#edf5fa); }"
    ".cui-window.cui-dark .cui-ambient { background-color: #161b29; background-image: radial-gradient(ellipse at 0% 15%, alpha(#7559b5,0.5), alpha(#7559b5,0) 65%), radial-gradient(ellipse at 95% 85%, alpha(#26776f,0.5), alpha(#26776f,0) 65%), linear-gradient(135deg,#242039,#111d28); }"
    ".cui-window .cui-ambient .cui-card { box-shadow: 0 18px 60px alpha(#19233e,0.14); }"
    ".cui-window .cui-panel { background: white; }"
    ".cui-window .cui-chat-background { background: #e5eced; }"
    ".cui-window .cui-message { background: white; border-radius: 12px; }"
    ".cui-window .cui-outgoing { background: #d8f3df; border-radius: 12px; }"
    ".cui-window.cui-dark .cui-panel { background: #202a35; }"
    ".cui-window.cui-dark .cui-chat-background { background: #14212b; }"
    ".cui-window.cui-dark .cui-message { background: #283644; }"
    ".cui-window.cui-dark .cui-outgoing { background: #294f56; }"
    ".cui-window { background: #f6f7f9; color: #20242c; }"
    ".cui-window.cui-dark { background: #17191e; color: #edf0f5; }"
    ".cui-content label { color: inherit; }"
    ".cui-window .cui-title { font-size: 1.85em; font-weight: 700; letter-spacing: -0.7px; }"
    ".cui-window .cui-heading { font-size: 1.05em; font-weight: 600; }"
    ".cui-window .cui-caption { color: #686f7d; font-size: 0.9em; }"
    ".cui-window.cui-dark .cui-caption { color: #a2aaba; }"
    ".cui-window .cui-card { background: #ffffff; border: 1px solid #e3e6eb; border-radius: 14px; }"
    ".cui-window.cui-dark .cui-card { background: #22252c; border-color: #343942; }"
    ".cui-window .cui-content button { background-image: none; background-color: #ffffff; color: #303641;"
    " border: 1px solid #d9dde5; border-radius: 8px; box-shadow: 0 1px 2px alpha(black,0.04);"
    " padding: 8px 18px; min-height: 20px; font-weight: 500; text-shadow: none; }"
    ".cui-window .cui-content button:hover { background-color: #eef1f6; }"
    ".cui-window .cui-content button:active { background-color: #e3e7ef; }"
    ".cui-window.cui-dark .cui-content button { background-color: #2c3039; color: #edf0f5; border-color: #424854; }"
    ".cui-window.cui-dark .cui-content button:hover { background-color: #373d48; }"
    ".cui-window.cui-dark .cui-content button:active { background-color: #424956; }"
    ".cui-window .cui-content button.cui-primary { background-color: #4967da; color: white; border-color: #4967da; }"
    ".cui-window .cui-content button.cui-primary:hover { background-color: #3d59c5; border-color: #3d59c5; }"
    ".cui-window .cui-content button.cui-primary:active { background-color: #334dab; border-color: #334dab; }"
    ".cui-window entry { background: #fcfcfd; color: #20242c; border: 1px solid #d9dde5;"
    " border-radius: 8px; padding: 9px 12px; min-height: 20px; box-shadow: none; }"
    ".cui-window.cui-dark entry { background: #191c22; color: #edf0f5; border-color: #424854; }"
    ".cui-window entry:focus-within { border-color: #6680e6; box-shadow: 0 0 0 2px alpha(#6680e6,0.2); }"
    ".cui-window entry.error { border-color: #c53f50; }"
    ".cui-window spinbutton { border-radius: 8px; background: #fcfcfd; color: #20242c; border: 1px solid #d9dde5; }"
    ".cui-window.cui-dark spinbutton { background: #191c22; color: #edf0f5; border-color: #424854; }"
    ".cui-window spinbutton text { padding: 9px 12px; }"
    ".cui-window .cui-content spinbutton button { border-radius: 0; border: none; padding: 6px 10px; box-shadow: none; }"
    ".cui-window entry selection { background: #4967da; color: white; }"
    ".cui-window checkbutton { color: inherit; padding: 4px 0; }"
    ".cui-window checkbutton check { background: #fcfcfd; color: white; border: 1px solid #bcc3cf;"
    " border-radius: 5px; min-width: 18px; min-height: 18px; margin-right: 8px; box-shadow: none; }"
    ".cui-window.cui-dark checkbutton check { background: #191c22; border-color: #616c80; }"
    ".cui-window checkbutton check:checked { background: #4967da; border-color: #4967da; }"
    ".cui-window .cui-content button:focus-visible, .cui-window checkbutton:focus-visible {"
    " outline: 2px solid #6680e6; outline-offset: 3px; }"
    ".cui-window .cui-content button.cui-flat { background: transparent; border-color: transparent; box-shadow: none; }"
    ".cui-window .cui-content button.cui-flat:hover { background: alpha(#8095aa,0.16); }"
    ".cui-window .cui-content button.cui-flat:checked { background: alpha(#4967da,0.22); color: #4967da; }"
    ".cui-window .cui-content button.cui-icon-button { padding: 8px; border-radius: 999px; }"
    ".cui-window .cui-content button:disabled, .cui-window entry:disabled, .cui-window checkbutton:disabled { opacity: 0.45; }";

static int system_is_dark(cui_app *app)
{
    gtk_app_state *state = (gtk_app_state *)app->native;
    GtkSettings *settings = gtk_settings_get_default();
    gboolean prefer_dark = FALSE;
    char *name = NULL;
    int dark;
    if (state->color_scheme) return state->color_scheme == 1;
    g_object_get(settings, "gtk-application-prefer-dark-theme", &prefer_dark,
                 "gtk-theme-name", &name, NULL);
    dark = prefer_dark || (name && (strstr(name, "dark") || strstr(name, "Dark")));
    g_free(name);
    return dark;
}

void cui__backend_theme(cui_app *app)
{
    cui_window *window;
    char *theme_name = NULL;
    const char *override = g_getenv("GTK_THEME");
    int contrast;
    g_object_get(gtk_settings_get_default(), "gtk-theme-name", &theme_name, NULL);
    contrast = (theme_name && strstr(theme_name, "HighContrast")) || (override && strstr(override, "HighContrast"));
    g_free(theme_name);
    int dark = app->theme == CUI_THEME_DARK ||
               (app->theme == CUI_THEME_SYSTEM && system_is_dark(app));
    for (window = app->windows; window; window = window->next) {
        if (contrast) gtk_widget_remove_css_class(GTK_WIDGET(window->native), "cui-window");
        else gtk_widget_add_css_class(GTK_WIDGET(window->native), "cui-window");
        if (dark) gtk_widget_add_css_class(GTK_WIDGET(window->native), "cui-dark");
        else gtk_widget_remove_css_class(GTK_WIDGET(window->native), "cui-dark");
        if (window->attached_native) {
            GtkWidget *panel = window->attached_native;
            if (contrast) gtk_widget_remove_css_class(panel, "cui-window");
            else gtk_widget_add_css_class(panel, "cui-window");
            if (dark) gtk_widget_add_css_class(panel, "cui-dark");
            else gtk_widget_remove_css_class(panel, "cui-dark");
        }
    }
}

static void settings_changed(GObject *object, GParamSpec *spec, gpointer data)
{
    (void)object;
    if (!strcmp(spec->name, "gtk-font-name")) {
        cui_app *app = (cui_app *)data;
        for (cui_window *w = app->windows; w; w = w->next) { cui__font_tree(w->root); cui__backend_refresh(w); }
    }
    if (g_str_has_prefix(spec->name, "gtk-")) cui__backend_theme((cui_app *)data);
}

static void portal_changed(GDBusProxy *proxy, gchar *sender, gchar *signal,
                           GVariant *parameters, gpointer data)
{
    cui_app *app = (cui_app *)data;
    gtk_app_state *state = (gtk_app_state *)app->native;
    const char *space, *key;
    GVariant *value;
    (void)proxy; (void)sender;
    if (strcmp(signal, "SettingChanged") || !g_variant_is_of_type(parameters, G_VARIANT_TYPE("(ssv)"))) return;
    g_variant_get(parameters, "(&s&sv)", &space, &key, &value);
    if (!strcmp(space, "org.freedesktop.appearance") && !strcmp(key, "color-scheme") &&
        g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32)) {
        state->color_scheme = g_variant_get_uint32(value);
        cui__backend_theme(app);
    }
    g_variant_unref(value);
}

static void connect_portal(cui_app *app)
{
    gtk_app_state *state = (gtk_app_state *)app->native;
    GVariant *reply, *value;
    state->portal = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
        G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START | G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
        NULL, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.Settings", NULL, NULL);
    if (!state->portal) return;
    g_signal_connect(state->portal, "g-signal", G_CALLBACK(portal_changed), app);
    reply = g_dbus_proxy_call_sync(state->portal, "Read",
        g_variant_new("(ss)", "org.freedesktop.appearance", "color-scheme"),
        G_DBUS_CALL_FLAGS_NO_AUTO_START, 500, NULL, NULL);
    if (!reply) return;
    g_variant_get(reply, "(v)", &value);
    if (g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32))
        state->color_scheme = g_variant_get_uint32(value);
    g_variant_unref(value);
    g_variant_unref(reply);
}

int cui__backend_init(cui_app *app)
{
    gtk_app_state *state;
    if (!gtk_init_check()) return 0;
    state = g_new0(gtk_app_state, 1);
    app->native = state;
    state->loop = g_main_loop_new(NULL, FALSE);
    state->css = gtk_css_provider_new();
    char *styles = g_strconcat(style, cui__gtk_styles(), NULL);
    load_css(state->css, styles);
    g_free(styles);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
        GTK_STYLE_PROVIDER(state->css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    state->settings_handler = g_signal_connect(gtk_settings_get_default(), "notify",
        G_CALLBACK(settings_changed), app);
    connect_portal(app);
    return 1;
}

void cui__backend_shutdown(cui_app *app)
{
    gtk_app_state *state = (gtk_app_state *)app->native;
    g_signal_handler_disconnect(gtk_settings_get_default(), state->settings_handler);
    gtk_style_context_remove_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(state->css));
    if (state->portal) g_signal_handlers_disconnect_by_data(state->portal, app);
    g_clear_object(&state->portal);
    g_object_unref(state->css);
    g_main_loop_unref(state->loop);
    g_free(state);
}

void cui__backend_run(cui_app *app)
{ g_main_loop_run(((gtk_app_state *)app->native)->loop); }
void cui__backend_quit(cui_app *app)
{ g_main_loop_quit(((gtk_app_state *)app->native)->loop); }

static gboolean window_close(GtkWindow *native, gpointer data)
{
    (void)native;
    cui_window_close((cui_window *)data);
    return TRUE;
}

int cui__backend_window_create(cui_window *window, const char *title)
{
    GtkWidget *native = gtk_window_new();
    window->native = native;
    gtk_window_set_title(GTK_WINDOW(native), title);
    gtk_window_set_default_size(GTK_WINDOW(native), window->width, window->height);
    gtk_widget_add_css_class(native, "cui-window");
    g_signal_connect(native, "close-request", G_CALLBACK(window_close), window);
    return 1;
}

void cui__backend_window_destroy(cui_window *window)
{
    if(window->attached_native) {
        if(gtk_widget_get_parent(window->attached_native))gtk_widget_unparent(window->attached_native);
        g_object_unref(window->attached_native);window->attached_native=NULL;
    }
    gtk_window_destroy(GTK_WINDOW(window->native));
}
void cui__backend_window_show(cui_window *window)
{
    cui__backend_theme(window->app);
    if(window->attached_native) {
        gtk_popover_popup(GTK_POPOVER(window->attached_native));
#ifdef GDK_WINDOWING_X11
        GdkSurface *surface=gtk_native_get_surface(GTK_NATIVE(window->attached_native));
        if(surface && GDK_IS_X11_SURFACE(surface))
            XStoreName(gdk_x11_display_get_xdisplay(gdk_surface_get_display(surface)),
                       gdk_x11_surface_get_xid(surface),gtk_window_get_title(GTK_WINDOW(window->native)));
#endif
    } else gtk_window_present(GTK_WINDOW(window->native));
}
void cui__backend_window_hide(cui_window *window)
{
    if(window->attached_native) gtk_popover_popdown(GTK_POPOVER(window->attached_native));
    else gtk_widget_set_visible(GTK_WIDGET(window->native), FALSE);
}
void cui__backend_refresh(cui_window *window)
{
    if (window->root) gtk_widget_queue_resize(GTK_WIDGET(window->root->native));
}

void cui__gtk_action(GtkWidget *native, gpointer data)
{ (void)native; cui__emit((cui_widget *)data); }

void cui__gtk_icon_paint(cairo_t *cr,const cui_icon_asset *a,const GdkRGBA *color)
{
    for(size_t i=0;i<a->count;i++){
        const cui_icon_command *c=a->commands+i;const float *v=c->values;
        switch(c->op){
        case CUI_ICON_MOVE:cairo_move_to(cr,v[0],v[1]);break;
        case CUI_ICON_LINE:cairo_line_to(cr,v[0],v[1]);break;
        case CUI_ICON_CUBIC:cairo_curve_to(cr,v[0],v[1],v[2],v[3],v[4],v[5]);break;
        case CUI_ICON_CLOSE:cairo_close_path(cr);break;
        case CUI_ICON_FILL:case CUI_ICON_STROKE:
            if(c->current_color)gdk_cairo_set_source_rgba(cr,color);
            else cairo_set_source_rgba(cr,(c->rgba>>24)/255.,((c->rgba>>16)&255)/255.,((c->rgba>>8)&255)/255.,(c->rgba&255)/255.);
            if(c->op==CUI_ICON_FILL){cairo_set_fill_rule(cr,v[0]?CAIRO_FILL_RULE_EVEN_ODD:CAIRO_FILL_RULE_WINDING);cairo_fill(cr);}
            else {cairo_set_miter_limit(cr,4);cairo_set_line_width(cr,v[0]);cairo_set_line_cap(cr,(cairo_line_cap_t)(int)v[1]);cairo_set_line_join(cr,(cairo_line_join_t)(int)v[2]);cairo_stroke(cr);}break;
        }
    }
}
static cairo_user_data_key_t icon_pixels_key;
static void icon_draw(GtkDrawingArea *area,cairo_t *cr,int width,int height,gpointer data)
{
    cui_widget *w=data;cui_icon_asset *a=w->icon;if(!a)return;
    double size=w->icon_size?w->icon_size:20;
    double scale=MIN(MIN(width,height),size)/MAX(a->width,a->height);
    cairo_translate(cr,(width-a->width*scale)/2,(height-a->height*scale)/2);cairo_scale(cr,scale,scale);
    if(a->pixels){
        int aw=(int)a->width,ah=(int)a->height,stride=cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32,aw);
        unsigned char *pixels=g_malloc((size_t)stride*ah);
        for(int y=0;y<ah;y++)for(int x=0;x<aw;x++){
            const unsigned char *p=a->pixels+((size_t)y*aw+x)*4;
            guint32 value=((guint32)p[3]<<24)|((p[0]*p[3]/255)<<16)|((p[1]*p[3]/255)<<8)|(p[2]*p[3]/255);
            memcpy(pixels+(size_t)y*stride+x*4,&value,4);
        }
        cairo_surface_t *surface=cairo_image_surface_create_for_data(pixels,CAIRO_FORMAT_ARGB32,aw,ah,stride);
        if(cairo_surface_set_user_data(surface,&icon_pixels_key,pixels,g_free)!=CAIRO_STATUS_SUCCESS){cairo_surface_destroy(surface);g_free(pixels);return;}
        cairo_set_source_surface(cr,surface,0,0);cairo_paint(cr);cairo_surface_destroy(surface);
    }else{GdkRGBA color;
#if GTK_CHECK_VERSION(4,10,0)
        gtk_widget_get_color(GTK_WIDGET(area),&color);
#else
        gtk_style_context_get_color(gtk_widget_get_style_context(GTK_WIDGET(area)),&color);
#endif
        cui__gtk_icon_paint(cr,a,&color);
    }
}
static GtkWidget *icon_area(cui_widget *w)
{
    GtkWidget *area=gtk_drawing_area_new();int size=w->icon_size?w->icon_size:20;
    gtk_widget_set_size_request(area,size,size);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area),icon_draw,w,NULL);
    gtk_widget_set_halign(area,GTK_ALIGN_CENTER);gtk_widget_set_valign(area,GTK_ALIGN_CENTER);
    return area;
}
void cui__backend_icon(cui_widget *w)
{
    GtkWidget *native=w->native;if(!native)return;
    if(w->kind==CUI_ICON){int size=w->icon_size?w->icon_size:20;gtk_widget_set_size_request(native,size,size);gtk_widget_queue_draw(native);return;}
    const char *saved=g_object_get_data(G_OBJECT(native),"cui-icon-label");
    char *label=g_strdup(saved?saved:gtk_button_get_label(GTK_BUTTON(native)));
    if(!label)label=g_strdup("");
    g_object_set_data_full(G_OBJECT(native),"cui-icon-label",g_strdup(label),g_free);
    if(!w->icon_only)gtk_widget_remove_css_class(native,"cui-icon-button");
    if(!w->icon&&!w->icon_only)gtk_button_set_label(GTK_BUTTON(native),label);
    else if(w->icon_only){gtk_button_set_child(GTK_BUTTON(native),icon_area(w));gtk_widget_add_css_class(native,"cui-icon-button");}
    else{GtkWidget *box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);gtk_box_append(GTK_BOX(box),icon_area(w));gtk_box_append(GTK_BOX(box),gtk_label_new(label));gtk_button_set_child(GTK_BUTTON(native),box);}
    gtk_accessible_update_property(GTK_ACCESSIBLE(native),GTK_ACCESSIBLE_PROPERTY_LABEL,label,-1);
    if(w->icon_only)gtk_widget_set_tooltip_text(native,label);
    g_free(label);
}

GtkWidget *cui__gtk_canvas_new(void);
int cui__backend_widget_create(cui_widget *widget, const char *text)
{
    GtkWidget *native;
    const char *signal = NULL;
    switch (widget->kind) {
    case CUI_CANVAS: native=cui__gtk_canvas_new(); widget->aux=gtk_overlay_get_child(GTK_OVERLAY(native)); break;
    case CUI_ICON: native=icon_area(widget);gtk_widget_add_css_class(native,"cui-icon-view");break;
    case CUI_GRID: case CUI_WRAP: case CUI_SPLIT:
        native = cui__gtk_container(widget); break;
    case CUI_BOX:
        native = gtk_box_new(widget->axis == CUI_HORIZONTAL ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL, widget->gap);
        break;
    case CUI_LABEL:
        native = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(native), 0);
        break;
    case CUI_BUTTON:
        native = gtk_button_new_with_label(text);
        signal = "clicked";
        break;
    case CUI_ENTRY:
        native = gtk_entry_new();
        gtk_editable_set_text(GTK_EDITABLE(native), text);
        if (widget->parent && widget->parent->last && widget->parent->last->kind == CUI_LABEL)
            gtk_accessible_update_property(GTK_ACCESSIBLE(native), GTK_ACCESSIBLE_PROPERTY_LABEL,
                gtk_label_get_text(GTK_LABEL(widget->parent->last->native)), -1);
        signal = "changed";
        break;
    case CUI_CHECKBOX:
        native = gtk_check_button_new_with_label(text);
        signal = "toggled";
        break;
    default:
        native = cui__gtk_control(widget, text);
        if (!native) return 0;
        break;
    }
    widget->native = native;
    if (widget->parent) cui__gtk_append(widget->parent, widget);
    else {
        gtk_widget_add_css_class(native, "cui-content");
        gtk_window_set_child(GTK_WINDOW(widget->window->native), native);
    }
    cui__backend_expand(widget);
    if (signal) g_signal_connect(native, signal, G_CALLBACK(cui__gtk_action), widget);
    return 1;
}

void cui__backend_expand(cui_widget *widget)
{
    int horizontal = widget->parent && widget->parent->axis == CUI_HORIZONTAL;
    /* Explicitly set both to prevent a descendant's expansion propagating
     * through containers and unexpectedly stretching sibling rows. */
    gtk_widget_set_hexpand(GTK_WIDGET(widget->native), horizontal && widget->expand);
    gtk_widget_set_vexpand(GTK_WIDGET(widget->native), !horizontal && widget->expand);
}

static void free_padding_provider(gpointer data)
{
    gtk_style_context_remove_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(data));
    g_object_unref(data);
}

void cui__backend_padding(cui_widget *widget)
{
    /* Inner padding belongs in CSS; margins would move the card's background. */
    GtkWidget *native = GTK_WIDGET(widget->native);
    int padding = widget->padding;
    /* GTK boxes have no padding API. A per-box provider gives nested cards
     * true inner padding without inserting synthetic public widgets. */
    GtkCssProvider *provider = g_object_get_data(G_OBJECT(native), "cui-padding");
    char css[128], name[64];
    if (!provider) {
        provider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_display(gdk_display_get_default(),
            GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
        g_object_set_data_full(G_OBJECT(native), "cui-padding", provider, free_padding_provider);
    }
    g_snprintf(name, sizeof(name), "cui-box-%p", (void *)widget);
    gtk_widget_set_name(native, name);
    g_snprintf(css, sizeof(css), "#%s { padding: %dpx; }", name, padding);
    load_css(provider, css);
}

void cui__backend_role(cui_widget *widget)
{
    static const char *roles[] = {"cui-body", "cui-title", "cui-heading", "cui-caption", "cui-primary", "cui-card",
        "cui-success", "cui-warning", "cui-danger", "cui-subtle", "cui-panel", "cui-chat-background", "cui-message", "cui-outgoing", "cui-flat", "cui-ambient"};
    size_t i;
    for (i = 0; i < G_N_ELEMENTS(roles); ++i) gtk_widget_remove_css_class(GTK_WIDGET(widget->native), roles[i]);
    gtk_widget_add_css_class(GTK_WIDGET(widget->native), roles[widget->role]);
}

void cui__backend_set_text(cui_widget *widget, const char *text)
{
    switch (widget->kind) {
    case CUI_LABEL: case CUI_BADGE: gtk_label_set_text(GTK_LABEL(widget->native), text); break;
    case CUI_BUTTON: case CUI_TOGGLE:
        if(widget->icon_only && !*text)break;
        g_object_set_data_full(G_OBJECT(widget->native),"cui-icon-label",g_strdup(text),g_free);
        cui__backend_icon(widget);break;
    case CUI_ENTRY: case CUI_PASSWORD: case CUI_SEARCH: gtk_editable_set_text(GTK_EDITABLE(widget->native), text); break;
    case CUI_RADIO:
    case CUI_CHECKBOX: gtk_check_button_set_label(GTK_CHECK_BUTTON(widget->native), text); break;
    case CUI_SWITCH: gtk_label_set_text(GTK_LABEL(gtk_widget_get_first_child(GTK_WIDGET(widget->native))), text); break;
    case CUI_TEXTAREA: case CUI_CODE:
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget->aux)), text, -1); break;
    default: break;
    }
}

size_t cui__backend_get_text(const cui_widget *widget, char *buffer, size_t capacity)
{
    const char *text = "";
    switch (widget->kind) {
    case CUI_LABEL: case CUI_BADGE: text = gtk_label_get_text(GTK_LABEL(widget->native)); break;
    case CUI_BUTTON: case CUI_TOGGLE:
        text=g_object_get_data(G_OBJECT(widget->native),"cui-icon-label");
        if(!text)text=gtk_button_get_label(GTK_BUTTON(widget->native));
        break;
    case CUI_ENTRY: case CUI_PASSWORD: case CUI_SEARCH: text = gtk_editable_get_text(GTK_EDITABLE(widget->native)); break;
    case CUI_RADIO:
    case CUI_CHECKBOX: text = gtk_check_button_get_label(GTK_CHECK_BUTTON(widget->native)); break;
    case CUI_SWITCH: text = gtk_label_get_text(GTK_LABEL(gtk_widget_get_first_child(GTK_WIDGET(widget->native)))); break;
    case CUI_TEXTAREA: case CUI_CODE: {
        GtkTextIter start, end;
        GtkTextBuffer *native = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget->aux));
        gtk_text_buffer_get_bounds(native, &start, &end);
        char *copy = gtk_text_buffer_get_text(native, &start, &end, TRUE);
        size_t length = cui__copy_text(copy, buffer, capacity);
        g_free(copy);
        return length;
    }
    default: break;
    }
    return cui__copy_text(text ? text : "", buffer, capacity);
}

void cui__backend_set_checked(cui_widget *widget, int checked)
{
    if (widget->kind == CUI_SWITCH) gtk_switch_set_active(GTK_SWITCH(widget->aux), checked);
    else if (widget->kind == CUI_TOGGLE) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget->native), checked);
    else gtk_check_button_set_active(GTK_CHECK_BUTTON(widget->native), checked);
}
int cui__backend_get_checked(const cui_widget *widget)
{
    if (widget->kind == CUI_SWITCH) return gtk_switch_get_active(GTK_SWITCH(widget->aux));
    if (widget->kind == CUI_TOGGLE) return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget->native));
    return gtk_check_button_get_active(GTK_CHECK_BUTTON(widget->native));
}
void cui__backend_set_enabled(cui_widget *widget, int enabled)
{ gtk_widget_set_sensitive(GTK_WIDGET(widget->native), enabled); }

/* GTK uses its native layout manager, including RTL placement. The shared
 * layout engine is used by the Windows and AppKit backends. */
cui_size cui__backend_measure(cui_widget *widget)
{
    int width, height;
    gtk_widget_measure(GTK_WIDGET(widget->native), GTK_ORIENTATION_HORIZONTAL, -1, &width, NULL, NULL, NULL);
    gtk_widget_measure(GTK_WIDGET(widget->native), GTK_ORIENTATION_VERTICAL, width, &height, NULL, NULL, NULL);
    return (cui_size){(float)width, (float)height};
}
void cui__backend_place(cui_widget *widget) { (void)widget; }

void cui__backend_font(cui_widget *w)
{
    if (cui__container(w)) return;
    const char *family; double points; int weight;
    cui__font_resolve(w, &family, &points, &weight);
    char *system_name = NULL;
    g_object_get(gtk_settings_get_default(), "gtk-font-name", &system_name, NULL);
    PangoFontDescription *font = pango_font_description_from_string(system_name ? system_name : "Sans 11");
    if (!points) {
        points = (double)pango_font_description_get_size(font)/PANGO_SCALE;
        if (points <= 0) points = 11;
        if (w->role == CUI_ROLE_TITLE) points *= 1.85;
        else if (w->role == CUI_ROLE_HEADING) points *= 1.05;
        else if (w->role == CUI_ROLE_CAPTION || w->kind == CUI_CODE || w->kind == CUI_BADGE) points *= 0.9;
    }
    if (!family) family = w->kind == CUI_CODE ? "monospace" : pango_font_description_get_family(font);
    if (!weight) weight = w->role == CUI_ROLE_TITLE ? 700 : w->role == CUI_ROLE_HEADING ? 600 : 400;
    GtkCssProvider *provider = g_object_get_data(G_OBJECT(w->native), "cui-font");
    char name[64]; g_snprintf(name, sizeof(name), "cui-font-%p", (void *)w);
    if (!provider) {
        provider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION+2);
        gtk_widget_add_css_class(GTK_WIDGET(w->native), name);
        g_object_set_data_full(G_OBJECT(w->native), "cui-font", provider, free_padding_provider);
    }
    char number[G_ASCII_DTOSTR_BUF_SIZE]; g_ascii_dtostr(number, sizeof(number), points*w->window->app->text_scale);
    char *escaped = g_strdup(family ? family : "Sans");
    g_strdelimit(escaped, "\"\\", ' ');
    char *css = g_strdup_printf(".%s, .%s * { font-family: \"%s\"; font-size: %spt; font-weight: %d; font-style: %s; }", name, name, escaped, number, weight, cui__font_italic(w)?"italic":"normal");
    load_css(provider, css); g_free(css); g_free(escaped); pango_font_description_free(font); g_free(system_name);
}
void cui__backend_font_free(cui_widget *w) { (void)w; }
double cui_window_scale(const cui_window *w) { return w ? gtk_widget_get_scale_factor(GTK_WIDGET(w->native)) : 1; }

cui_icon_asset *cui_icon_load_image(const char *path)
{
    if(!path)return NULL;
    int file_width=0,file_height=0;
    if(!gdk_pixbuf_get_file_info(path,&file_width,&file_height)||file_width<1||file_height<1||file_width>4096||file_height>4096)return NULL;
    GError *error=NULL;GdkPixbuf *image=gdk_pixbuf_new_from_file(path,&error);
    if(!image){g_clear_error(&error);return NULL;}
    int width=gdk_pixbuf_get_width(image),height=gdk_pixbuf_get_height(image);
    cui_icon_asset *asset=NULL;
    if(width>0&&height>0&&width<=4096&&height<=4096){
        unsigned char *rgba=g_malloc((size_t)width*height*4);
        const unsigned char *pixels=gdk_pixbuf_read_pixels(image);
        int stride=gdk_pixbuf_get_rowstride(image),channels=gdk_pixbuf_get_n_channels(image);
        for(int y=0;y<height;y++)for(int x=0;x<width;x++){
            const unsigned char *p=pixels+(size_t)y*stride+x*channels;
            unsigned char *q=rgba+((size_t)y*width+x)*4;
            q[0]=p[0];q[1]=p[1];q[2]=p[2];q[3]=channels==4?p[3]:255;
        }
        asset=cui_icon_rgba(rgba,width,height);g_free(rgba);
    }
    g_object_unref(image);return asset;
}

/* GTK owns translucent surfaces; X11 additionally receives a bounding shape so
 * rounded custom frames work even without a compositing window manager. */
static void window_shape(cui_window *w)
{
#ifdef GDK_WINDOWING_X11
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(w->native));
    if (!surface || !GDK_IS_X11_SURFACE(surface)) return;
    Display *display = gdk_x11_display_get_xdisplay(gdk_surface_get_display(surface));
    Window xid = gdk_x11_surface_get_xid(surface);
    if (w->decorated || w->corner_radius == 0 || gdk_display_is_composited(gdk_surface_get_display(surface))) {
        XShapeCombineMask(display,xid,ShapeBounding,0,0,None,ShapeSet); return;
    }
    int scale = gdk_surface_get_scale_factor(surface);
    /* XShape coordinates are signed 16-bit, even on a 64-bit host. */
    int width = (int)fmin((double)w->width*scale,32767.);
    int height = (int)fmin((double)w->height*scale,32767.);
    double radius = fmin(w->corner_radius*scale,fmin(width,height)/2.);
    Region region = XCreateRegion();
    if (!region) return;
    for (int y=0;y<height;++y) {
        double dy = y < radius ? radius-y-.5 : y>=height-radius ? y+.5-(height-radius) : 0;
        int inset = dy ? (int)ceil(radius-sqrt(fmax(0,radius*radius-dy*dy))) : 0;
        XRectangle row={(short)inset,(short)y,(unsigned short)(width-2*inset),1};
        XUnionRectWithRegion(&row,region,region);
    }
    XShapeCombineRegion(display,xid,ShapeBounding,0,0,region,ShapeSet);
    XDestroyRegion(region);
#else
    (void)w;
#endif
}
int cui__backend_window_frame(cui_window *w)
{
    GtkWindow *native = GTK_WINDOW(w->native);
    if (w->decorated) gtk_widget_remove_css_class(GTK_WIDGET(native),"cui-custom-frame");
    else gtk_widget_add_css_class(GTK_WIDGET(native),"cui-custom-frame");
    gtk_window_set_decorated(native,w->decorated);
    gtk_window_set_resizable(native,w->resizable);
    GtkCssProvider *css = g_object_get_data(G_OBJECT(native),"cui-frame-css");
    if (!css) {
        css=gtk_css_provider_new();
        gtk_style_context_add_provider(gtk_widget_get_style_context(GTK_WIDGET(native)),
            GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION+1);
        g_object_set_data_full(G_OBJECT(native),"cui-frame-css",css,g_object_unref);
    }
    char style[256];
    if (w->decorated) style[0]=0;
    else g_snprintf(style,sizeof(style),"window.cui-window { background: transparent; background-image: none; border: none; padding: 0; margin: 0; outline: none; box-shadow: none; border-radius: %.3fpx; }",w->corner_radius);
    load_css(css,style);
    gtk_widget_realize(GTK_WIDGET(native));
    window_shape(w);
    return 1;
}
int cui__backend_window_size(cui_window *w)
{
    if(w->attached_native) {
        GtkWidget *viewport=gtk_popover_get_child(GTK_POPOVER(w->attached_native));
        gtk_widget_set_size_request(viewport,w->width,w->height);
        return 1;
    }
    gtk_window_set_default_size(GTK_WINDOW(w->native),w->width,w->height);
#ifdef GDK_WINDOWING_X11
    /* Honor an explicit custom-frame size after GDK's interactive X11 resize;
     * the configure event brings GTK's allocation/default size into sync. */
    GdkSurface *surface=gtk_native_get_surface(GTK_NATIVE(w->native));
    if(w->visible && !w->decorated && surface && GDK_IS_X11_SURFACE(surface)) {
        int scale=gdk_surface_get_scale_factor(surface);
        XResizeWindow(gdk_x11_display_get_xdisplay(gdk_surface_get_display(surface)),
                      gdk_x11_surface_get_xid(surface),w->width*scale,w->height*scale);
    }
#endif
    window_shape(w);
    return 1;
}
int cui__backend_window_position(cui_window *w, int x, int y)
{
#ifdef GDK_WINDOWING_X11
    gtk_widget_realize(GTK_WIDGET(w->native));
    GdkSurface *surface=gtk_native_get_surface(GTK_NATIVE(w->native));
    if (surface && GDK_IS_X11_SURFACE(surface)) {
        int scale=gdk_surface_get_scale_factor(surface);
        XMoveWindow(gdk_x11_display_get_xdisplay(gdk_surface_get_display(surface)),
                    gdk_x11_surface_get_xid(surface),x*scale,y*scale);
        return 1;
    }
#else
    (void)w; (void)x; (void)y;
#endif
    return 0;
}
int cui__backend_window_move(cui_window *w)
{
    GdkEvent *event=w->pointer_event;
    if (!event || !w->visible) return 0;
    GdkSurface *surface=gtk_native_get_surface(GTK_NATIVE(w->native));
    double x=0,y=0;
    if (!surface || !gdk_event_get_position(event,&x,&y)) return 0;
    gdk_toplevel_begin_move(GDK_TOPLEVEL(surface),gdk_event_get_device(event),
                            1,x,y,gdk_event_get_time(event));
    return 1;
}

int cui__backend_window_get_size(cui_window *w, int *width, int *height)
{
    GtkWidget *native=w->attached_native ? w->attached_native : w->native;
    *width=gtk_widget_get_width(native);
    *height=gtk_widget_get_height(native);
    return *width>0 && *height>0;
}
int cui__backend_window_resize(cui_window *w, int corner)
{
    GdkEvent *event=w->pointer_event;
    if (!event) return 0;
    GdkSurface *surface=gtk_native_get_surface(GTK_NATIVE(w->native));
    double x=0,y=0;
    if (!surface || !gdk_event_get_position(event,&x,&y)) return 0;
    const GdkSurfaceEdge edges[]={GDK_SURFACE_EDGE_NORTH_WEST,GDK_SURFACE_EDGE_NORTH_EAST,
                                 GDK_SURFACE_EDGE_SOUTH_WEST,GDK_SURFACE_EDGE_SOUTH_EAST};
    gdk_toplevel_begin_resize(GDK_TOPLEVEL(surface),edges[corner],gdk_event_get_device(event),
                              1,x,y,gdk_event_get_time(event));
    return 1;
}

static void attached_panel_closed(GtkPopover *popover, gpointer data)
{
    (void)popover;
    cui_window *window=data;
    if(!window->app->destroying)cui_window_close(window);
}

int cui__backend_window_anchor(cui_window *w)
{
    GtkWidget *parent=w->anchor_parent->root->native;
    if(!w->attached_native) {
        GtkWidget *popover=gtk_popover_new();
        g_signal_connect(popover,"closed",G_CALLBACK(attached_panel_closed),w);
        gtk_widget_add_css_class(popover,"cui-window");
        gtk_widget_add_css_class(popover,"cui-attached-panel");
        gtk_popover_set_has_arrow(GTK_POPOVER(popover),FALSE);
        gtk_popover_set_autohide(GTK_POPOVER(popover),FALSE);
        gtk_popover_set_position(GTK_POPOVER(popover),GTK_POS_RIGHT);
        GtkWidget *child=gtk_window_get_child(GTK_WINDOW(w->native));
        gtk_widget_set_visible(w->native,FALSE);
        g_object_ref(child);gtk_window_set_child(GTK_WINDOW(w->native),NULL);
        /* A high-DPI texture's natural size is in physical pixels. Bound the
         * popup in logical pixels, just as a GtkWindow's default size does. */
        GtkWidget *viewport=gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(viewport),GTK_POLICY_EXTERNAL,GTK_POLICY_EXTERNAL);
        gtk_widget_set_size_request(viewport,w->width,w->height);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(viewport),child);g_object_unref(child);
        gtk_popover_set_child(GTK_POPOVER(popover),viewport);
        gtk_widget_set_parent(popover,parent);
        w->attached_native=g_object_ref(popover);
        cui__backend_theme(w->app);
    }
    if(gtk_widget_get_parent(w->attached_native)!=parent) {
        gtk_widget_unparent(w->attached_native);gtk_widget_set_parent(w->attached_native,parent);
    }
    GdkRectangle rect={w->anchor_x,w->anchor_y,w->anchor_width,w->anchor_height};
    int parent_width=gtk_widget_get_width(parent),parent_height=gtk_widget_get_height(parent);
    if(parent_width>0 && parent_height>0) {
        rect.x=MIN(rect.x,parent_width-1);rect.y=MIN(rect.y,parent_height-1);
        rect.width=MIN(rect.width,parent_width-rect.x);
        rect.height=MIN(rect.height,parent_height-rect.y);
    }
    gtk_popover_set_pointing_to(GTK_POPOVER(w->attached_native),&rect);
    /* Wayland compositors may dismiss popups whose buffer is neither adjacent
     * to nor overlapping the parent. Keep the decorative gap inside the
     * parent's transparent padding, including during asynchronous resizing. */
    int gap=MIN(16,MAX(0,parent_width-rect.x-rect.width-1));
    gtk_popover_set_offset(GTK_POPOVER(w->attached_native),gap,0);
    if(w->visible) {
        if(gtk_widget_get_visible(w->attached_native))gtk_popover_present(GTK_POPOVER(w->attached_native));
        else gtk_popover_popup(GTK_POPOVER(w->attached_native));
    }
    return 1;
}
