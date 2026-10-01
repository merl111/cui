#ifndef CUI_INPUTS_H
#define CUI_INPUTS_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Native numeric entry with stepper arrows. Values are rounded to the given
 * decimal precision and clamped. Bounds and step must be representable at that
 * precision (0..9 digits); bounds are limited to +/-1e9. Invalid/nonfinite
 * configuration is rejected. Programmatic updates never emit on_action.
 * User typing commits on Enter/focus loss; arrows commit immediately.
 * Invalid typed text is restored to the last committed value. */
cui_widget *cui_number(cui_widget *parent, double value, double minimum,
                       double maximum, double step, unsigned digits);
int cui_number_configure(cui_widget *number, double minimum, double maximum,
                         double step, unsigned digits);
int cui_number_set(cui_widget *number, double value);
double cui_number_get(const cui_widget *number);
typedef struct cui_date_value { int year, month, day; } cui_date_value;
typedef struct cui_time_value { int hour, minute, second; } cui_time_value;
/* Gregorian civil dates (1601..9999) and wall-clock times (24-hour values).
 * These are timezone-free values, not Unix timestamps. Native controls format
 * them for the platform. Invalid dates/times are rejected without mutation. */
cui_widget *cui_date(cui_widget *parent, cui_date_value value);
int cui_date_set(cui_widget *date, cui_date_value value);
cui_date_value cui_date_get(const cui_widget *date);
cui_widget *cui_time_input(cui_widget *parent, cui_time_value value);
int cui_time_set(cui_widget *time, cui_time_value value);
cui_time_value cui_time_get(const cui_widget *time);
/* Labeled field with persistent help and an inline error. The application
 * validates business rules in the field's on_action callback. An empty error
 * clears invalid state. The entry can be configured with ordinary CUI APIs.
 * The root forwards user edits; do not replace its entry's internal callback. */
cui_widget *cui_field(cui_widget *parent, const char *label, const char *value, const char *help);
cui_widget *cui_field_entry(cui_widget *field);
void cui_field_set_error(cui_widget *field, const char *message);
int cui_field_is_valid(const cui_widget *field);
#ifdef __cplusplus
}
#endif
#endif
