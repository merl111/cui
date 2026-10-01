#include "cui_inputs_internal.h"
#include "cui_desktop.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static double rounded(double value, unsigned digits)
{
    char text[96];
    snprintf(text, sizeof(text), "%.*f", (int)digits, value);
    return strtod(text, NULL);
}
static int valid_range(double minimum, double maximum, double step, unsigned digits)
{
    return digits <= 9 && isfinite(minimum) && isfinite(maximum) && isfinite(step) &&
        minimum >= -1e9 && maximum <= 1e9 && minimum <= maximum && step > 0 && step <= 2e9 &&
        rounded(minimum, digits) == minimum && rounded(maximum, digits) == maximum && rounded(step, digits) == step;
}
int cui_number_set(cui_widget *w, double value)
{
    if (!w || w->kind != CUI_NUMBER || !isfinite(value)) return 0;
    cui_number_state *s = w->payload;
    value = value < s->minimum ? s->minimum : value > s->maximum ? s->maximum : value;
    w->value = rounded(value, s->digits);
    ++w->updating; cui__backend_number(w); --w->updating;
    return 1;
}
double cui_number_get(const cui_widget *w)
{ return w && w->kind == CUI_NUMBER ? w->value : 0; }
int cui_number_configure(cui_widget *w, double minimum, double maximum, double step, unsigned digits)
{
    if (!w || w->kind != CUI_NUMBER || !valid_range(minimum, maximum, step, digits)) return 0;
    *(cui_number_state *)w->payload = (cui_number_state){minimum, maximum, step, digits};
    return cui_number_set(w, w->value);
}
cui_widget *cui_number(cui_widget *parent, double value, double minimum, double maximum, double step, unsigned digits)
{
    if (!valid_range(minimum, maximum, step, digits) || !isfinite(value)) return NULL;
    cui_number_state *s = malloc(sizeof(*s));
    if (!s) return NULL;
    cui_widget *w = cui__append(parent, CUI_NUMBER, "", CUI_VERTICAL, 0);
    if (!w) { free(s); return NULL; }
    *s = (cui_number_state){minimum, maximum, step, digits};
    w->payload = s; w->destroy_payload = free;
    cui_number_set(w, value); return w;
}
void cui__number_user(cui_widget *w, double value)
{
    if (w->updating || !w->payload || w->read_only) return;
    double previous = w->value;
    if (cui_number_set(w, value) && w->value != previous) cui__emit(w);
}
void cui__number_step(cui_widget *w, int direction)
{
    if (!w->payload) return;
    cui_number_state *s = w->payload;
    cui__number_user(w, w->value + direction * s->step);
}
static void field_changed(cui_widget *entry, void *data)
{ (void)entry; cui__emit(data); }
cui_widget *cui_field(cui_widget *parent, const char *label, const char *value, const char *help)
{
    cui_widget *field = cui_box(parent, CUI_VERTICAL, 6);
    if (!field) return NULL;
    field->component = CUI_COMPONENT_FIELD;
    cui_widget *title = cui_label(field, label);
    cui_widget *entry = cui_entry(field, value);
    cui_widget *hint = cui_label(field, help);
    cui_widget *error = cui_label(field, "");
    if (!title || !entry || !hint || !error) return NULL;
    field->content = entry;
    cui_set_role(title, CUI_ROLE_HEADING); cui_set_role(hint, CUI_ROLE_CAPTION);
    cui_set_visible(hint, help && *help); cui_set_role(error, CUI_ROLE_DANGER); cui_set_visible(error, 0);
    cui_accessibility(entry, label, help); cui_on_action(entry, field_changed, field);
    return field;
}
cui_widget *cui_field_entry(cui_widget *field)
{ return field && field->component == CUI_COMPONENT_FIELD ? field->content : NULL; }
void cui_field_set_error(cui_widget *field, const char *message)
{
    cui_widget *entry = cui_field_entry(field);
    if (!entry) return;
    int invalid = message && *message;
    cui_set_text(field->last, message); cui_set_visible(field->last, invalid);
    cui__backend_invalid(entry, invalid);
    /* Keep the error available to assistive technology on the focused input. */
    size_t length = cui_get_text(field->first, NULL, 0);
    char *label = malloc(length + 1);
    if (label) {
        cui_get_text(field->first, label, length + 1);
        size_t help_length = cui_get_text(entry->next, NULL, 0);
        char *help = malloc(help_length + 1);
        if (help) { cui_get_text(entry->next, help, help_length + 1); cui_accessibility(entry, label, invalid ? message : help); free(help); }
        free(label);
    }
}
int cui_field_is_valid(const cui_widget *field)
{ return field && field->component == CUI_COMPONENT_FIELD && field->last->hidden; }
