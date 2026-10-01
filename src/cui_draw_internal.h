#ifndef CUI_DRAW_INTERNAL_H
#define CUI_DRAW_INTERNAL_H
#include "cui_draw.h"
#include "cui_internal.h"
#include <stdint.h>
typedef struct cui_pixel_buffer {
  size_t refs;
  uint32_t data[];
} cui_pixel_buffer;
typedef struct cui_blur_cache {
  struct cui_blur_cache *next;
  int x, y, w, h, radius, shadow;
  double shape[5];
  unsigned tint;
  size_t bytes;
  uint32_t *source, *result;
} cui_blur_cache;
typedef struct cui_damage {
  int x0, y0, x1, y1;
} cui_damage;
struct cui_surface {
  size_t refs;
  int width, height;
  double scale;
  uint32_t *pixels;
  cui_pixel_buffer *buffer;
  cui_blur_cache *cache;
  size_t cache_bytes;
};
typedef struct cui_canvas_state {
  cui_widget *widget;
  cui_surface *surface;
  cui_canvas_callback callback;
  void *userdata;
  cui_canvas_region *regions;
  size_t count;
  unsigned focus, pressed;
  double opacity;
} cui_canvas_state;
cui_canvas_state *cui__canvas_state(const cui_widget *w);
void cui__canvas_event(cui_widget *w, cui_canvas_event_kind kind, double x,
                       double y, double dx, double dy, unsigned modifiers);
int cui__canvas_key(cui_widget *w, int backwards, int activate);
void cui__canvas_focus(cui_widget *w, unsigned id);
int cui__canvas_attach(cui_widget *w);
void cui__canvas_regions(cui_widget *w);
cui_pixel_buffer *cui__pixels_create(size_t bytes);
int cui__pixels_retain(cui_pixel_buffer *buffer);
void cui__pixels_release(void *buffer);
void cui__blur_cache_free(cui_blur_cache *cache);
/* Returns 1 on native immutable-buffer presentation, 0 to use RGBA fallback. */
int cui__canvas_present(cui_widget *widget, cui_surface *surface,
                        double opacity);
int cui__native_opacity(cui_widget *w, double value);
/* Native text rasterization returns an 8-bit coverage mask, owned by caller. */
unsigned char *cui__draw_text(const char *text, const char *family, double size,
                              int weight, int max_width, int *width,
                              int *height);
/* Premultiplied ARGB including native color glyphs; NULL uses mask fallback. */
uint32_t *cui__draw_text_color(const char *text,const char *family,double size,int weight,int max_width,int *width,int *height,unsigned color);
uint32_t *cui__draw_asset(const cui_icon_asset *asset, int width, int height,
                          unsigned color);
int cui__render(cui_surface *surface, const cui_draw_command *commands,
                size_t count, uint32_t *pixels, const cui_damage *damage);
#endif
