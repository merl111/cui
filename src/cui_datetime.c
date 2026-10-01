#include "cui_inputs_internal.h"
#include <stdlib.h>

static int valid_date(cui_date_value v)
{
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (v.year < 1601 || v.year > 9999 || v.month < 1 || v.month > 12 || v.day < 1) return 0;
    int leap = v.year % 4 == 0 && (v.year % 100 != 0 || v.year % 400 == 0);
    return v.day <= days[v.month-1] + (v.month == 2 && leap);
}
static int valid_time(cui_time_value v)
{ return v.hour >= 0 && v.hour < 24 && v.minute >= 0 && v.minute < 60 && v.second >= 0 && v.second < 60; }
static cui_widget *picker(cui_widget *parent, cui_kind kind, cui_date_value date, cui_time_value time)
{
    cui_datetime_state *s = malloc(sizeof(*s));
    if (!s) return NULL;
    cui_widget *w = cui__append(parent, kind, "", CUI_VERTICAL, 0);
    if (!w) { free(s); return NULL; }
    s->date = date; s->time = time; w->payload = s; w->destroy_payload = free;
    ++w->updating; cui__backend_datetime(w); --w->updating;
    return w;
}
cui_widget *cui_date(cui_widget *parent, cui_date_value value)
{ return valid_date(value) ? picker(parent, CUI_DATE, value, (cui_time_value){0,0,0}) : NULL; }
cui_widget *cui_time_input(cui_widget *parent, cui_time_value value)
{ return valid_time(value) ? picker(parent, CUI_TIME_INPUT, (cui_date_value){2000,1,1}, value) : NULL; }
int cui_date_set(cui_widget *w, cui_date_value value)
{
    if (!w || w->kind != CUI_DATE || !valid_date(value)) return 0;
    ((cui_datetime_state *)w->payload)->date = value;
    ++w->updating; cui__backend_datetime(w); --w->updating; return 1;
}
int cui_time_set(cui_widget *w, cui_time_value value)
{
    if (!w || w->kind != CUI_TIME_INPUT || !valid_time(value)) return 0;
    ((cui_datetime_state *)w->payload)->time = value;
    ++w->updating; cui__backend_datetime(w); --w->updating; return 1;
}
cui_date_value cui_date_get(const cui_widget *w)
{ return w && w->kind == CUI_DATE ? ((cui_datetime_state *)w->payload)->date : (cui_date_value){0,0,0}; }
cui_time_value cui_time_get(const cui_widget *w)
{ return w && w->kind == CUI_TIME_INPUT ? ((cui_datetime_state *)w->payload)->time : (cui_time_value){0,0,0}; }
void cui__date_user(cui_widget *w, cui_date_value value)
{
    if (w->updating || !w->payload) return;
    cui_date_value old = cui_date_get(w);
    if (!cui_date_set(w, value)) { ++w->updating; cui__backend_datetime(w); --w->updating; return; }
    if (old.year != value.year || old.month != value.month || old.day != value.day) cui__emit(w);
}
void cui__time_user(cui_widget *w, cui_time_value value)
{
    if (w->updating || !w->payload) return;
    cui_time_value old = cui_time_get(w);
    if (cui_time_set(w, value) && (old.hour != value.hour || old.minute != value.minute || old.second != value.second)) cui__emit(w);
}
