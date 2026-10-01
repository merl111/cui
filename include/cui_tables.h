#ifndef CUI_TABLES_H
#define CUI_TABLES_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum cui_table_event { CUI_TABLE_NONE, CUI_TABLE_SELECTION, CUI_TABLE_EDIT, CUI_TABLE_SORT } cui_table_event;
/* Tables start read-only and single-select. Row/column indices are zero based.
 * Native controls render visible rows on demand; the copied data is in memory.
 * Setters suppress callbacks; native selection/edit/sort actions emit them. */
void cui_table_set_multiple(cui_widget *table, int multiple);
int cui_table_select_row(cui_widget *table, size_t row, int selected);
size_t cui_table_selected_rows(const cui_widget *table, size_t *rows, size_t capacity);
int cui_table_set_editable(cui_widget *table, size_t column, int editable);
int cui_table_set_cell(cui_widget *table, size_t row, size_t column, const char *text);
size_t cui_table_get_cell(const cui_widget *table, size_t row, size_t column, char *buffer, size_t capacity);
/* Stable sorting by one column. numeric=0 compares UTF-8 bytes; numeric=1
 * compares finite numbers and puts non-numbers last. Sort/replacement clears
 * selection. Indices subsequently refer to the new order. Native header clicks
 * sort text; numeric sorting can be requested by the application. */
/* Original row index from the last set_rows call, retained across sorting.
 * Returns (size_t)-1 for an invalid row. Useful for updating application data. */
size_t cui_table_source_row(const cui_widget *table, size_t row);
int cui_table_sort(cui_widget *table, size_t column, int descending, int numeric);
/* Selection reports row=-1,column=-1; editing reports both; sorting reports
 * column and row=-1. This is the last user event, not programmatic changes. */
cui_table_event cui_table_last_event(const cui_widget *table, int *row, int *column);
#ifdef __cplusplus
}
#endif
#endif
