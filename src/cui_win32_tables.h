#ifndef CUI_WIN32_TABLES_H
#define CUI_WIN32_TABLES_H
#include "cui_tables_internal.h"
#include <windows.h>
#include <commctrl.h>
wchar_t *cui__win32_wide(const char *text);
void cui__win32_table_init(cui_widget *w);
void cui__win32_table_items(cui_widget *w);
LRESULT cui__win32_table_notify(cui_widget *w,NMHDR *header);
#endif
