#include "cui_inputs_internal.h"
#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"inputs:%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static cui_app *app;
static cui_widget *number, *field, *date, *time_input;
static cui_timer *timer;
static int changes, edits;
static void changed(cui_widget *sender, void *data)
{ (void)data; CHECK(sender == number); ++changes; }
static void edited(cui_widget *sender, void *data)
{
    (void)data; CHECK(sender == field); ++edits;
    cui_field_set_error(field, cui_get_text(cui_field_entry(field), NULL, 0) ? "" : "Name is required.");
}
static void verify(void *data)
{
    (void)data; cui_timer_stop(timer);
    CHECK(GTK_IS_SPIN_BUTTON(number->native));
    CHECK(cui_date_get(date).day == 29);
    CHECK(!cui_date_set(date, (cui_date_value){1900,2,29}));
    CHECK(cui_date_set(date, (cui_date_value){2000,2,29}));
    CHECK(!cui_date_set(date, (cui_date_value){2026,4,31}));
    GDateTime *day = g_date_time_new_utc(2026,9,30,12,0,0);
#if GTK_CHECK_VERSION(4, 20, 0)
    gtk_calendar_set_date(GTK_CALENDAR(date->aux),day);
#else
    gtk_calendar_select_day(GTK_CALENDAR(date->aux),day);
#endif
    g_date_time_unref(day);
    CHECK(cui_date_get(date).year == 2026 && cui_date_get(date).day == 30);
    CHECK(!cui_time_set(time_input, (cui_time_value){24,0,0}));
    CHECK(!cui_time_set(time_input, (cui_time_value){12,60,0}));
    GtkWidget *hour = gtk_widget_get_first_child(GTK_WIDGET(time_input->native));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(hour),23);
    CHECK(cui_time_get(time_input).hour == 23);

    CHECK(cui_number_get(number) == 1.25 && changes == 0);
    CHECK(cui_number_set(number, 1.237) && cui_number_get(number) == 1.24);
    CHECK(cui_number_set(number, 999) && cui_number_get(number) == 10);
    CHECK(!cui_number_set(number, NAN));
    CHECK(!cui_number_configure(number, 0, 10, 0.005, 2));
    CHECK(!cui_number_configure(number, 10, 0, 1, 2));
    CHECK(!cui_number_configure(number, 0, 10, 1, 10));
    CHECK(cui_number_configure(number, -5, 5, 0.25, 2) && cui_number_get(number) == 5);
    CHECK(changes == 0);
    gtk_spin_button_spin(GTK_SPIN_BUTTON(number->native), GTK_SPIN_STEP_BACKWARD, 0);
    CHECK(changes == 1 && cui_number_get(number) == 4.75);
    gtk_editable_set_text(GTK_EDITABLE(number->native), "2.50");
    gtk_spin_button_update(GTK_SPIN_BUTTON(number->native));
    CHECK(changes == 2 && cui_number_get(number) == 2.5);
    gtk_editable_set_text(GTK_EDITABLE(number->native), "invalid");
    gtk_spin_button_update(GTK_SPIN_BUTTON(number->native));
    CHECK(cui_number_get(number) == 2.5 && changes == 2);
    CHECK(cui_field_is_valid(field));
    GtkEditable *entry = GTK_EDITABLE(cui_field_entry(field)->native);
    gtk_editable_set_text(entry, "");
    CHECK(edits == 1 && !cui_field_is_valid(field));
    CHECK(gtk_widget_has_css_class(GTK_WIDGET(entry), "error"));
    gtk_editable_set_text(entry, "Valid name");
    CHECK(edits >= 2 && cui_field_is_valid(field));
    CHECK(!gtk_widget_has_css_class(GTK_WIDGET(entry), "error"));
    cui_app_quit(app);
}
int main(void)
{
    app = cui_app_create(); CHECK(app);
    if(getenv("CUI_TEXT_SCALE"))CHECK(cui_app_set_text_scale(app,1.5));
    cui_window *window = cui_window_create(app, "Inputs", 640, 480); CHECK(window);
    cui_widget *root = cui_window_root(window);
    CHECK(!cui_number(root, 1, 0, 1, 0, 2));
    CHECK(!cui_date(root, (cui_date_value){2025,2,29}));
    date = cui_date(root, (cui_date_value){2024,2,29}); CHECK(date);
    time_input = cui_time_input(root, (cui_time_value){12,30,45}); CHECK(time_input);
    number = cui_number(root, 1.25, -10, 10, 0.25, 2); CHECK(number);
    field = cui_field(root, "Name", "Initial", "A name is required."); CHECK(field);
    cui_on_action(number, changed, NULL); cui_on_action(field, edited, NULL);
    timer = cui_every(app, 150, verify, NULL);
    cui_window_show(window); cui_app_run(app); cui_app_destroy(app);
    puts("inputs: numeric bounds, precision, native stepping/editing and inline validation passed");
    return 0;
}
