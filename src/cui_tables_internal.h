#ifndef CUI_TABLES_INTERNAL_H
#define CUI_TABLES_INTERNAL_H
#include "cui_internal.h"
#include "cui_tables.h"
typedef struct cui_table_state {
    unsigned char *selected;
    size_t rows, *source_rows;
    int multiple, editable[64], sort_column, descending, numeric;
    cui_table_event event;
    int event_row, event_column;
    void *editor;
} cui_table_state;
int cui__table_init(cui_widget *w);
int cui__table_resize(cui_widget *w, size_t rows);
void cui__table_select(cui_widget *w, int row);
void cui__table_selection_changed(cui_widget *w);
void cui__table_edit(cui_widget *w, size_t row, size_t column, const char *text);
void cui__table_sort(cui_widget *w, size_t column, int descending);
void cui__backend_table_selection(cui_widget *w);
void cui__backend_table_reveal(cui_widget *w,size_t row);
void cui__backend_table_cell(cui_widget *w, size_t row, size_t column);
#endif
