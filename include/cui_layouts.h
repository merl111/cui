#ifndef CUI_LAYOUTS_H
#define CUI_LAYOUTS_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Layered layout: children overlap without changing the base content size.
 * Add the base content first (FILL), then transient layers. Children are ordinary
 * boxes. Zero width/height uses content size; FILL ignores dimensions. Native
 * focus, dismissal and modal policy remain with the enclosing composition. */
typedef enum cui_layer_alignment {
    CUI_LAYER_FILL, CUI_LAYER_CENTER, CUI_LAYER_TOP, CUI_LAYER_BOTTOM,
    CUI_LAYER_BOTTOM_RIGHT
} cui_layer_alignment;
cui_widget *cui_stack(cui_widget *parent);
cui_widget *cui_stack_layer(cui_widget *stack, cui_layer_alignment alignment,
    int width, int height, int margin);
/* A full-stack dimming button, inserted after the base and before dialog layers.
 * Emits on_action on activation; the application closes its dialog and hides
 * both layers. Native hit testing prevents clicks inside a later dialog layer
 * from activating the backdrop. label is its accessible dismissal name, never
 * visible text. Customize the dimming color with cui_set_style. */
cui_widget *cui_stack_backdrop(cui_widget *stack, const char *label);
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
