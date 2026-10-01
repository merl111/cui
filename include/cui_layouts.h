#ifndef CUI_LAYOUTS_H
#define CUI_LAYOUTS_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Grid cells are owned boxes. Coordinates are zero-based, at most 64 columns
 * and 256 rows. Spans must not overlap existing cells. */
cui_widget *cui_grid(cui_widget *parent, unsigned columns, int gap);
cui_widget *cui_grid_cell(cui_widget *grid, unsigned row, unsigned column, unsigned row_span, unsigned column_span);
/* A wrapping row recomputes its rows as available width changes. */
cui_widget *cui_wrap(cui_widget *parent, int gap);
/* Horizontal splits divide left/right; vertical splits divide top/bottom.
 * Each pane is an ordinary owned box. The divider is draggable and keyboard
 * operable. on_action on the split observes user divider changes. */
cui_widget *cui_split(cui_widget *parent, cui_axis axis, double fraction);
cui_widget *cui_split_pane(cui_widget *split, unsigned index);
void cui_split_set_position(cui_widget *split, double fraction);
double cui_split_get_position(const cui_widget *split);
#ifdef __cplusplus
}
#endif
#endif
