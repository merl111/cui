#ifndef CUI_DRAW_H
#define CUI_DRAW_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Independent, main-thread-only RGBA surface. Logical dimensions and pixel
 * scale are fixed; resize by replacing the surface. Maximum 4096 pixels/axis,
 * 16 million pixels. Widgets retain surfaces. All input arrays/text are
 * borrowed only for the duration of the call; render is transactional on
 * invalid/OOM input. */
typedef struct cui_surface cui_surface;
typedef enum cui_draw_op {
  CUI_DRAW_CLEAR,
  CUI_DRAW_SAVE,
  CUI_DRAW_RESTORE,
  CUI_DRAW_TRANSLATE,
  CUI_DRAW_SCALE,
  CUI_DRAW_CLIP,
  CUI_DRAW_LAYER,
  CUI_DRAW_END_LAYER,
  CUI_DRAW_RECT,
  CUI_DRAW_ELLIPSE,
  CUI_DRAW_LINE,
  CUI_DRAW_ICON,
  CUI_DRAW_TEXT,
  CUI_DRAW_GRADIENT,
  CUI_DRAW_SHADOW,
  CUI_DRAW_MATERIAL
} cui_draw_op;
/* Colors are straight 0xRRGGBBAA, source-over. Coordinates are logical pixels.
 * p: rect/clip/gradient/material/shadow/icon = x,y,width,height,radius,amount.
 * amount: material/shadow blur radius (0..32 logical pixels).
 * line = x1,y1,x2,y2,width; text =
 * x,y,max_width,size,weight(100..900),alignment,box_height. TEXT alignment is
 * 0=left, 1=center, 2=right; positive box_height centers visible glyphs
 * vertically and clips to that height (zero retains top origin). Text uses
 * optional font (NULL = system sans). gradient is vertical color→color2.
 * translate/scale = x,y; layer = opacity (0..1). Group opacity is applied once
 * when END_LAYER composites its children, unlike primitive paint alpha. CLEAR
 * fills the whole target; use before clips/layers. Balanced saves/layers
 * required. Transforms support positive scaling and translation; clipping is
 * rounded-rectangle intersection. At most 8192 commands, 32 saves/clips, 8
 * layers. ICON accepts existing vector or RGBA assets; color tints symbolic
 * paints. Blur samples only the current app-owned surface, never desktop
 * pixels.
 */
typedef enum cui_draw_align {
  CUI_DRAW_ALIGN_LEFT,
  CUI_DRAW_ALIGN_CENTER,
  CUI_DRAW_ALIGN_RIGHT
} cui_draw_align;
typedef struct cui_draw_command {
  cui_draw_op op;
  float p[8];
  unsigned color, color2;
  const char *text, *font;
  const cui_icon_asset *icon;
} cui_draw_command;
typedef enum cui_canvas_event_kind {
  CUI_CANVAS_PRESS,
  CUI_CANVAS_RELEASE,
  CUI_CANVAS_MOVE,
  CUI_CANVAS_SCROLL,
  CUI_CANVAS_ACTIVATE,
  CUI_CANVAS_FOCUS
} cui_canvas_event_kind;
typedef struct cui_canvas_event {
  cui_canvas_event_kind kind;
  unsigned id; /* Topmost enabled region, or zero for background. */
  double x, y, dx,
      dy; /* Logical coordinates; scroll deltas are platform units. */
  unsigned modifiers; /* cui_modifiers from cui_desktop.h. */
} cui_canvas_event;
typedef struct cui_canvas_region {
  unsigned id;
  float x, y, width, height;
  const char *label; /* Required accessible name; copied. */
  int enabled;
} cui_canvas_region;
typedef void (*cui_canvas_callback)(cui_widget *canvas,
                                    const cui_canvas_event *event,
                                    void *userdata);
/* Draw capabilities describe the surface renderer, independent of native child
 * control opacity. Native widget opacity returns zero when unsupported. */
typedef enum cui_draw_capability {
  CUI_DRAW_ALPHA = 1,
  CUI_DRAW_GROUPS = 2,
  CUI_DRAW_BLUR = 4,
  CUI_DRAW_TEXT_SHAPING = 8,
  CUI_DRAW_REGIONS = 16
} cui_draw_capability;
unsigned cui_draw_capabilities(void);
/* Measure the same native single-line text used by DRAW_TEXT. Main thread.
 * Size is logical pixels, 1..512; weight 100..900. Width is capped at 4096.
 * Invalid input leaves outputs unchanged. No application/widget is required
 * after the native toolkit has been initialized. */
int cui_text_measure(const char *text, const char *family, double size,
                     int weight, double *width, double *height);
cui_surface *cui_surface_create(int width, int height, double scale);
cui_surface *cui_surface_retain(cui_surface *surface);
void cui_surface_release(cui_surface *surface);
int cui_surface_render(cui_surface *surface, const cui_draw_command *commands,
                       size_t count);
/* Replay a complete scene beginning with CLEAR, committing only this logical
 * damage rectangle (rounded outward to pixels). Include both old and new bounds
 * of moved content, shadows and any affected backdrop material in damage.
 * Neighboring blur inputs are replayed automatically. Outside pixels and the
 * whole surface on failure remain unchanged. Rectangle must lie in the surface.
 * Cache memory is bounded and released with the surface; no borrowed inputs are
 * retained. */
int cui_surface_render_region(cui_surface *surface,
                              const cui_draw_command *commands, size_t count,
                              double x, double y, double width, double height);
/* Returns required RGBA byte count. Copies only when capacity is sufficient.
 * Output is straight RGBA, top-left origin; dimensions returned in pixels. */
size_t cui_surface_read(const cui_surface *surface, unsigned char *rgba,
                        size_t capacity, int *width, int *height);
cui_widget *cui_canvas(cui_widget *parent);
int cui_canvas_set_surface(cui_widget *canvas, cui_surface *surface);
/* Replace regions atomically; unique nonzero IDs, max 256. Last region is top.
 * Press/release/scroll/move and keyboard activation share the same IDs.
 * Updating/clearing handlers and regions does not synthesize activation. */
int cui_canvas_set_regions(cui_widget *canvas, const cui_canvas_region *regions,
                           size_t count);
void cui_canvas_on_event(cui_widget *canvas, cui_canvas_callback callback,
                         void *userdata);
/* Programmatic/assistive activation validates the current enabled region. */
int cui_canvas_activate_region(cui_widget *canvas, unsigned id);
int cui_canvas_focus_region(cui_widget *canvas, unsigned id);
int cui_set_opacity(cui_widget *widget, double opacity);
double cui_get_opacity(const cui_widget *widget);
#ifdef __cplusplus
}
#endif
#endif
