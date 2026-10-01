#ifndef CUI_INPUTS_INTERNAL_H
#define CUI_INPUTS_INTERNAL_H
#include "cui_internal.h"
#include "cui_inputs.h"
typedef struct cui_number_state { double minimum, maximum, step; unsigned digits; } cui_number_state;
typedef struct cui_datetime_state { cui_date_value date; cui_time_value time; } cui_datetime_state;
void cui__backend_number(cui_widget *w);
void cui__backend_datetime(cui_widget *w);
void cui__date_user(cui_widget *w, cui_date_value value);
void cui__time_user(cui_widget *w, cui_time_value value);
void cui__number_user(cui_widget *w, double value);
void cui__number_step(cui_widget *w, int direction);
void cui__backend_invalid(cui_widget *w, int invalid);
#endif
