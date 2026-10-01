#include "cui_inputs_internal.h"
#include "cui_gtk.h"
#include <math.h>

const char *cui__gtk_styles(void)
{
    return
    ".cui-window button:checked { background: #e7ecff; color: #344fbd; border-color: #bac8fc; }"
    ".cui-window.cui-dark button:checked { background: #343c58; color: #c4d0ff; border-color: #536495; }"
    ".cui-window .cui-badge { border-radius: 12px; padding: 4px 10px; background: #e7ecff; color: #344fbd; font-size: 0.85em; font-weight: 600; }"
    ".cui-window .cui-success { background: #e4f4ea; color: #227345; }"
    ".cui-window .cui-warning { background: #fff1d3; color: #855400; }"
    ".cui-window .cui-danger { background: #fce8e9; color: #a7313a; }"
    ".cui-window.cui-dark .cui-badge { background: #343c58; color: #c4d0ff; }"
    ".cui-window.cui-dark .cui-success { background: #253c32; color: #9ce0b6; }"
    ".cui-window.cui-dark .cui-warning { background: #443a25; color: #f3cd85; }"
    ".cui-window.cui-dark .cui-danger { background: #452d34; color: #f3acb2; }"
    ".cui-window .cui-subtle { background: alpha(#888888,0.08); border-radius: 8px; }"
    ".cui-window switch { background: #d6dbe4; border: none; border-radius: 14px; min-width: 44px; min-height: 24px; }"
    ".cui-window switch:checked { background: #4967da; }"
    ".cui-window switch slider { background: white; border: none; border-radius: 50%; min-width: 20px; min-height: 20px; margin: 2px; }"
    ".cui-window scale trough, .cui-window progressbar trough { background: #dce1eb; border: none; min-height: 6px; min-width: 2px; border-radius: 6px; }"
    ".cui-window.cui-dark scale trough, .cui-window.cui-dark progressbar trough { background: #3c4250; }"
    ".cui-window scale highlight, .cui-window progressbar progress { background: #6680e6; border: none; border-radius: 6px; min-height: 6px; min-width: 2px; }"
    ".cui-window scale slider { background: #6680e6; border: 3px solid white; min-width: 14px; min-height: 14px; box-shadow: 0 1px 4px alpha(black,0.15); }"
    ".cui-window textview, .cui-window textview text, .cui-window list, .cui-window listview, .cui-window columnview, .cui-window columnview listview { background: #fcfcfd; color: #20242c; }"
    ".cui-window.cui-dark textview, .cui-window.cui-dark textview text, .cui-window.cui-dark list, .cui-window.cui-dark listview, .cui-window.cui-dark columnview, .cui-window.cui-dark columnview listview { background: #1c1f26; color: #edf0f5; }"
    ".cui-window textview text selection, .cui-window list row:selected, .cui-window listview row:selected, .cui-window columnview row:selected { background: #4967da; color: white; }"
    ".cui-window .cui-input-surface { border: 1px solid #d9dde5; border-radius: 8px; }"
    ".cui-window.cui-dark .cui-input-surface { border-color: #424854; }"
    ".cui-window list row { padding: 9px 12px; border-radius: 5px; }"
    ".cui-window columnview cell { padding: 10px 12px; }"
    ".cui-window columnview header button { border-radius: 0; padding: 9px 12px; font-weight: 600; }"
    ".cui-window .cui-code { font-family: monospace; font-size: 0.92em; }"
    ".cui-window separator { background: alpha(#888888,0.25); min-height: 1px; }"
    ".cui-window checkbutton radio { min-width: 18px; min-height: 18px; margin-right: 8px; }";
}

static void notify_action(GObject *object, GParamSpec *spec, gpointer data)
{ (void)object; (void)spec; cui__emit((cui_widget *)data); }
static void list_action(GtkListBox *list, GtkListBoxRow *row, gpointer data)
{ (void)list; (void)row; cui__emit((cui_widget *)data); }
static GtkWidget *scrolled(cui_widget *widget, GtkWidget *child, int height)
{
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), child);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, 240, height);
    gtk_widget_add_css_class(scroll, "cui-input-surface");
    widget->aux = child;
    return scroll;
}
static GtkWidget *text_view(cui_widget *widget, const char *text)
{
    GtkWidget *view = gtk_text_view_new();
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    gtk_text_buffer_set_text(buffer, text, -1);
    gtk_text_buffer_set_enable_undo(buffer, TRUE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), widget->kind != CUI_CODE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), widget->kind == CUI_CODE ? GTK_WRAP_NONE : GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 12);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 12);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(view), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(view), 10);
    if (widget->kind == CUI_CODE) gtk_widget_add_css_class(view, "cui-code");
    g_signal_connect(buffer, "changed", G_CALLBACK(cui__gtk_action), widget);
    return scrolled(widget, view, 120);
}
static GtkWidget *switch_control(cui_widget *widget, const char *text)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    GtkWidget *label = gtk_label_new(text), *toggle = gtk_switch_new();
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_widget_set_valign(toggle, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(row), label);
    gtk_box_append(GTK_BOX(row), toggle);
    gtk_accessible_update_property(GTK_ACCESSIBLE(toggle), GTK_ACCESSIBLE_PROPERTY_LABEL, text, -1);
    widget->aux = toggle;
    g_signal_connect(toggle, "notify::active", G_CALLBACK(notify_action), widget);
    return row;
}
static void draw_chart(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data)
{
    cui_widget *w = (cui_widget *)data;
    double low = 0, high = 1;
    size_t i;
    (void)area;
    if (!w->series_count) return;
    low = high = w->series[0];
    for (i = 1; i < w->series_count; ++i) { if (w->series[i] < low) low = w->series[i]; if (w->series[i] > high) high = w->series[i]; }
    if (high == low) high = low + 1;
    cairo_set_source_rgba(cr, 0.5, 0.55, 0.65, 0.18); cairo_set_line_width(cr, 1);
    for (i = 0; i < 4; ++i) { double y = 12 + (height - 24) * (double)i / 3; cairo_move_to(cr, 12, y); cairo_line_to(cr, width - 12, y); }
    cairo_stroke(cr); cairo_set_source_rgb(cr, 0.40, 0.50, 0.90); cairo_set_line_width(cr, 2.5);
    for (i = 0; i < w->series_count; ++i) {
        double x = 12 + (width - 24) * (double)i / (double)(w->series_count > 1 ? w->series_count - 1 : 1);
        double y = height - 12 - (height - 24) * (w->series[i] - low) / (high - low);
        if (!i) cairo_move_to(cr, x, y); else cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
}
static void date_changed(GtkCalendar *calendar, gpointer data)
{
    GDateTime *date = gtk_calendar_get_date(calendar);
    cui_date_value value = {g_date_time_get_year(date),g_date_time_get_month(date),g_date_time_get_day_of_month(date)};
    g_date_time_unref(date); cui__date_user(data,value);
}
static void time_changed(GtkSpinButton *spin, gpointer data)
{
    (void)spin; cui_widget *w=data;
    GtkWidget *hour=gtk_widget_get_first_child(GTK_WIDGET(w->native));
    GtkWidget *minute=gtk_widget_get_next_sibling(hour), *second=gtk_widget_get_next_sibling(minute);
    cui__time_user(w,(cui_time_value){gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(hour)),
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(minute)),gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(second))});
}
static GtkWidget *date_picker(cui_widget *w)
{
    GtkWidget *button=gtk_menu_button_new(), *popover=gtk_popover_new(), *calendar=gtk_calendar_new();
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(button),popover);
    gtk_popover_set_child(GTK_POPOVER(popover),calendar);
    w->aux=calendar; g_signal_connect(calendar,"day-selected",G_CALLBACK(date_changed),w);
    return button;
}
static GtkWidget *time_picker(cui_widget *w)
{
    GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    const char *names[]={"Hours","Minutes","Seconds"};
    for(int i=0;i<3;++i){
        GtkWidget *spin=gtk_spin_button_new_with_range(0,i?59:23,1);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin),TRUE);
        gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin),GTK_UPDATE_IF_VALID);
        gtk_accessible_update_property(GTK_ACCESSIBLE(spin),GTK_ACCESSIBLE_PROPERTY_LABEL,names[i],-1);
        gtk_box_append(GTK_BOX(row),spin);g_signal_connect(spin,"value-changed",G_CALLBACK(time_changed),w);
    }
    return row;
}
static int number_input(GtkSpinButton *spin, double *result, gpointer data)
{
    cui_widget *w=data;
    if(!w->payload)return FALSE;
    const char *text=gtk_editable_get_text(GTK_EDITABLE(spin));
    char *end;double value=g_strtod(text,&end);
    if(end==text)return GTK_INPUT_ERROR;
    while(g_ascii_isspace(*end))++end;
    cui_number_state *s=w->payload;
    if(*end || !isfinite(value) || value<s->minimum || value>s->maximum)return GTK_INPUT_ERROR;
    *result=value;return TRUE;
}
static void number_changed(GtkSpinButton *spin, gpointer data)
{ cui__number_user(data, gtk_spin_button_get_value(spin)); }
GtkWidget *cui__gtk_control(cui_widget *widget, const char *text)
{
    GtkWidget *native;
    switch (widget->kind) {
    case CUI_DATE: return date_picker(widget);
    case CUI_TIME_INPUT: return time_picker(widget);
    case CUI_NUMBER:
        native = gtk_spin_button_new_with_range(0, 100, 1);
        gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(native), TRUE);
        gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(native), GTK_UPDATE_IF_VALID);
        g_signal_connect(native, "value-changed", G_CALLBACK(number_changed), widget);
        g_signal_connect(native, "input", G_CALLBACK(number_input), widget);
        return native;
    case CUI_TREE: return cui__gtk_tree(widget);
    case CUI_CHART:
        native = gtk_drawing_area_new(); gtk_widget_set_size_request(native, 260, 140);
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(native), draw_chart, widget, NULL); return native;
    case CUI_IMAGE:
        native = gtk_picture_new(); gtk_widget_set_size_request(native, 260, 160);
        gtk_picture_set_can_shrink(GTK_PICTURE(native), TRUE); return native;
    case CUI_TOGGLE:
        native = gtk_toggle_button_new_with_label(text);
        g_signal_connect(native, "toggled", G_CALLBACK(cui__gtk_action), widget);
        return native;
    case CUI_SWITCH: return switch_control(widget, text);
    case CUI_RADIO: {
        cui_widget *sibling;
        native = gtk_check_button_new_with_label(text);
        for (sibling = widget->parent->first; sibling; sibling = sibling->next)
            if (sibling->kind == CUI_RADIO) { gtk_check_button_set_group(GTK_CHECK_BUTTON(native), GTK_CHECK_BUTTON(sibling->native)); break; }
        /* 'activate' observes only the user's chosen button, not deselections. */
        g_signal_connect_after(native, "activate", G_CALLBACK(cui__gtk_action), widget);
        return native;
    }
    case CUI_PASSWORD: case CUI_SEARCH:
        native = widget->kind == CUI_SEARCH ? gtk_search_entry_new() : gtk_entry_new();
        if (widget->kind == CUI_PASSWORD) gtk_entry_set_visibility(GTK_ENTRY(native), FALSE);
        gtk_editable_set_text(GTK_EDITABLE(native), text);
        g_signal_connect(native, "changed", G_CALLBACK(cui__gtk_action), widget);
        return native;
    case CUI_TEXTAREA: case CUI_CODE: return text_view(widget, text);
    case CUI_SELECT:
        native = gtk_drop_down_new(NULL, NULL);
        g_signal_connect(native, "notify::selected", G_CALLBACK(notify_action), widget);
        return native;
    case CUI_LIST:
        native = gtk_list_box_new();
        gtk_list_box_set_selection_mode(GTK_LIST_BOX(native), GTK_SELECTION_SINGLE);
        g_signal_connect(native, "row-selected", G_CALLBACK(list_action), widget);
        return scrolled(widget, native, 160);
    case CUI_TABLE:
        native = gtk_column_view_new(NULL);
        gtk_column_view_set_show_row_separators(GTK_COLUMN_VIEW(native), TRUE);
        return scrolled(widget, native, 180);
    case CUI_SLIDER:
        native = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.01);
        gtk_scale_set_draw_value(GTK_SCALE(native), FALSE);
        gtk_widget_set_size_request(native, 160, -1);
        g_signal_connect(native, "value-changed", G_CALLBACK(cui__gtk_action), widget);
        return native;
    case CUI_PROGRESS: return gtk_progress_bar_new();
    case CUI_SPINNER:
        native = gtk_spinner_new(); gtk_spinner_start(GTK_SPINNER(native));
        gtk_widget_set_size_request(native, 22, 22);
        gtk_widget_set_halign(native, GTK_ALIGN_START);
        return native;
    case CUI_SEPARATOR: return gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    case CUI_BADGE:
        native = gtk_label_new(text); gtk_widget_add_css_class(native, "cui-badge");
        gtk_widget_set_halign(native, GTK_ALIGN_START);
        gtk_widget_set_valign(native, GTK_ALIGN_CENTER);
        return native;
    default: return NULL;
    }
}

void cui__backend_items(cui_widget *w)
{
    size_t i;
    if (w->kind == CUI_TABLE) { cui__gtk_table_items(w); return; }
    if (w->kind == CUI_SELECT) {
        GtkStringList *strings = gtk_string_list_new(NULL);
        gtk_string_list_append(strings, ""); /* Explicit empty selection, even on GTK versions that autoselect. */
        for (i = 0; i < w->item_count; ++i) gtk_string_list_append(strings, w->items[i]);
        gtk_drop_down_set_model(GTK_DROP_DOWN(w->native), G_LIST_MODEL(strings));
        g_object_unref(strings);
    } else {
        GtkWidget *child;
        while ((child = gtk_widget_get_first_child(GTK_WIDGET(w->aux)))) gtk_list_box_remove(GTK_LIST_BOX(w->aux), child);
        for (i = 0; i < w->item_count; ++i) {
            GtkWidget *label = gtk_label_new(w->items[i]);
            gtk_label_set_xalign(GTK_LABEL(label), 0);
            gtk_list_box_append(GTK_LIST_BOX(w->aux), label);
        }
    }
}
void cui__backend_set_selected(cui_widget *w, int index)
{
    guint selected = index < 0 ? GTK_INVALID_LIST_POSITION : (guint)index;
    if (w->kind == CUI_SELECT) gtk_drop_down_set_selected(GTK_DROP_DOWN(w->native), (guint)(index + 1));
    else if (w->kind == CUI_TABLE) gtk_single_selection_set_selected(GTK_SINGLE_SELECTION(gtk_column_view_get_model(GTK_COLUMN_VIEW(w->aux))), selected);
    else gtk_list_box_select_row(GTK_LIST_BOX(w->aux), index < 0 ? NULL : gtk_list_box_get_row_at_index(GTK_LIST_BOX(w->aux), index));
}
int cui__backend_get_selected(const cui_widget *w)
{
    guint selected;
    if (w->kind == CUI_LIST) {
        GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(w->aux));
        return row ? gtk_list_box_row_get_index(row) : -1;
    }
    if (w->kind == CUI_SELECT) return (int)gtk_drop_down_get_selected(GTK_DROP_DOWN(w->native)) - 1;
    else selected = gtk_single_selection_get_selected(GTK_SINGLE_SELECTION(gtk_column_view_get_model(GTK_COLUMN_VIEW(w->aux))));
    return selected == GTK_INVALID_LIST_POSITION ? -1 : (int)selected;
}
void cui__backend_set_value(cui_widget *w, double value)
{
    if (w->kind == CUI_SLIDER) gtk_range_set_value(GTK_RANGE(w->native), value);
    else gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w->native), value);
}
double cui__backend_get_value(const cui_widget *w)
{ return w->kind == CUI_SLIDER ? gtk_range_get_value(GTK_RANGE(w->native)) : gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(w->native)); }
void cui__backend_placeholder(cui_widget *w, const char *text)
{ g_object_set(w->native, "placeholder-text", text, NULL); }
void cui__backend_tooltip(cui_widget *w, const char *text)
{ gtk_widget_set_tooltip_text(GTK_WIDGET(w->native), text); }
void cui__backend_visible(cui_widget *w, int visible)
{ gtk_widget_set_visible(GTK_WIDGET(w->native), visible); }
void cui__backend_min_size(cui_widget *w)
{ gtk_widget_set_size_request(GTK_WIDGET(w->native), w->min_width ? w->min_width : -1, w->min_height ? w->min_height : -1); }
void cui__backend_scrollable(cui_window *window)
{
    GtkWidget *root = GTK_WIDGET(window->root->native);
    g_object_ref(root);
    if (window->content) gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(window->content), NULL);
    else gtk_window_set_child(GTK_WINDOW(window->native), NULL);
    if (window->scrollable) {
        GtkWidget *scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), root);
        gtk_window_set_child(GTK_WINDOW(window->native), scroll);
        window->content = scroll;
    } else {
        gtk_window_set_child(GTK_WINDOW(window->native), root);
        window->content = NULL;
    }
    g_object_unref(root);
}
static gboolean timer_tick(gpointer data)
{
    cui_timer *timer = (cui_timer *)data;
    if (timer->active) timer->task(timer->userdata);
    return timer->active ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}
int cui__backend_timer(cui_timer *timer)
{
    timer->native = GUINT_TO_POINTER(g_timeout_add(timer->interval, timer_tick, timer));
    return timer->native != NULL;
}
void cui__backend_timer_stop(cui_timer *timer)
{ if (timer->native) { g_source_remove(GPOINTER_TO_UINT(timer->native)); timer->native = NULL; } }
double cui_time(void) { return (double)g_get_monotonic_time() / 1000000; }
void cui__backend_media(cui_widget *w)
{
    if (w->kind == CUI_CHART) { gtk_widget_queue_draw(GTK_WIDGET(w->native)); return; }
    GBytes *bytes = g_bytes_new(w->pixels, (size_t)w->image_width * (size_t)w->image_height * 4);
    GdkTexture *texture = gdk_memory_texture_new(w->image_width, w->image_height, GDK_MEMORY_R8G8B8A8, bytes, (size_t)w->image_width * 4);
    gtk_picture_set_paintable(GTK_PICTURE(w->kind==CUI_CANVAS?w->aux:w->native), GDK_PAINTABLE(texture));
    g_object_unref(texture); g_bytes_unref(bytes);
}
void cui_clipboard_set_text(cui_window *window, const char *text)
{
    if (window) gdk_clipboard_set_text(gtk_widget_get_clipboard(GTK_WIDGET(window->native)), text ? text : "");
}

size_t cui_get_selected_text(const cui_widget *w, char *buffer, size_t capacity)
{
    char *text = NULL;
    if (w && (w->kind == CUI_TEXTAREA || w->kind == CUI_CODE)) {
        GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w->aux)); GtkTextIter a, z;
        if (gtk_text_buffer_get_selection_bounds(b, &a, &z)) text = gtk_text_buffer_get_text(b, &a, &z, FALSE);
    } else if (w && (w->kind == CUI_ENTRY || w->kind == CUI_SEARCH)) {
        int a, z;
        if (gtk_editable_get_selection_bounds(GTK_EDITABLE(w->native), &a, &z)) text = gtk_editable_get_chars(GTK_EDITABLE(w->native), a, z);
    }
    size_t n = cui__copy_text(text ? text : "", buffer, capacity); g_free(text); return n;
}

void cui__backend_number(cui_widget *w)
{
    cui_number_state *s = w->payload;
    GtkSpinButton *spin = GTK_SPIN_BUTTON(w->native);
    gtk_spin_button_set_range(spin, s->minimum, s->maximum);
    gtk_spin_button_set_increments(spin, s->step, s->step * 10);
    gtk_spin_button_set_digits(spin, s->digits);
    gtk_spin_button_set_value(spin, w->value);
}
void cui__backend_invalid(cui_widget *w, int invalid)
{
    if (invalid) gtk_widget_add_css_class(GTK_WIDGET(w->native), "error");
    else gtk_widget_remove_css_class(GTK_WIDGET(w->native), "error");
    gtk_accessible_update_state(GTK_ACCESSIBLE(w->native), GTK_ACCESSIBLE_STATE_INVALID,
        invalid ? GTK_ACCESSIBLE_INVALID_TRUE : GTK_ACCESSIBLE_INVALID_FALSE, -1);
}

void cui__backend_datetime(cui_widget *w)
{
    cui_datetime_state *s=w->payload;
    if(w->kind==CUI_DATE){
        GDateTime *date=g_date_time_new_utc(s->date.year,s->date.month,s->date.day,12,0,0);
#if GTK_CHECK_VERSION(4, 20, 0)
        gtk_calendar_set_date(GTK_CALENDAR(w->aux),date);
#else
        gtk_calendar_select_day(GTK_CALENDAR(w->aux),date);
#endif
        char *label=g_date_time_format(date,"%x");
        gtk_menu_button_set_label(GTK_MENU_BUTTON(w->native),label);g_free(label);g_date_time_unref(date);
    }else{
        GtkWidget *spin=gtk_widget_get_first_child(GTK_WIDGET(w->native));
        int values[]={s->time.hour,s->time.minute,s->time.second};
        for(int i=0;i<3;++i,spin=gtk_widget_get_next_sibling(spin))gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin),values[i]);
    }
}
