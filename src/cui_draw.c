#include "cui_desktop.h"
#include "cui_draw_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
cui_pixel_buffer *cui__pixels_create(size_t bytes) {
  cui_pixel_buffer *b = malloc(sizeof(*b) + bytes);
  if (b)
    b->refs = 1;
  return b;
}
int cui__pixels_retain(cui_pixel_buffer *b) {
#if defined(__GNUC__) || defined(__clang__)
  size_t old = __atomic_load_n(&b->refs, __ATOMIC_RELAXED);
  while (old < SIZE_MAX) {
    if (__atomic_compare_exchange_n(&b->refs, &old, old + 1, 0,
                                    __ATOMIC_RELAXED, __ATOMIC_RELAXED))
      return 1;
  }
  return 0;
#else
  /* Non-GNU backends currently use the main-thread RGBA fallback. */
  if (b->refs == SIZE_MAX)
    return 0;
  ++b->refs;
  return 1;
#endif
}
void cui__pixels_release(void *buffer) {
  cui_pixel_buffer *b = buffer;
  if (!b)
    return;
#if defined(__GNUC__) || defined(__clang__)
  if (__atomic_sub_fetch(&b->refs, 1, __ATOMIC_ACQ_REL) == 0)
    free(b);
#else
  if (!--b->refs)
    free(b);
#endif
}
void cui__blur_cache_free(cui_blur_cache *cache) {
  while (cache) {
    cui_blur_cache *next = cache->next;
    free(cache->source);
    free(cache->result);
    free(cache);
    cache = next;
  }
}
unsigned cui_draw_capabilities(void) {
  return CUI_DRAW_ALPHA | CUI_DRAW_GROUPS | CUI_DRAW_BLUR |
         CUI_DRAW_TEXT_SHAPING | CUI_DRAW_REGIONS;
}
int cui_text_measure(const char *text, const char *family, double size,
                     int weight, double *width, double *height) {
  if (!text || !width || !height || !isfinite(size) || size < 1 || size > 512 ||
      weight < 100 || weight > 900 || strlen(text) > 65536)
    return 0;
  int w = 0, h = 0;
  unsigned char *mask = cui__draw_text(text, family, size, weight, 4096, &w, &h);
  if (!mask) return 0;
  free(mask);
  *width = *text ? w : 0;
  *height = h;
  return 1;
}
cui_surface *cui_surface_create(int width, int height, double scale) {
  if (width < 1 || height < 1 || !isfinite(scale) || scale < 0.25 ||
      scale > 8 || width * scale > 4096 || height * scale > 4096)
    return NULL;
  int pw = (int)ceil(width * scale), ph = (int)ceil(height * scale);
  if ((size_t)pw * ph > 16000000)
    return NULL;
  cui_surface *s = calloc(1, sizeof(*s));
  if (!s)
    return NULL;
  s->buffer = cui__pixels_create((size_t)pw * ph * 4);
  if (!s->buffer) {
    free(s);
    return NULL;
  }
  s->pixels = s->buffer->data;
  memset(s->pixels, 0, (size_t)pw * ph * 4);
  s->refs = 1;
  s->width = pw;
  s->height = ph;
  s->scale = scale;
  return s;
}
cui_surface *cui_surface_retain(cui_surface *s) {
  if (s && s->refs < SIZE_MAX) {
    ++s->refs;
    return s;
  }
  return NULL;
}
void cui_surface_release(cui_surface *s) {
  if (s && !--s->refs) {
    cui__pixels_release(s->buffer);
    cui__blur_cache_free(s->cache);
    free(s);
  }
}
static int render_surface(cui_surface *s, const cui_draw_command *cmd,
                          size_t count, const cui_damage *damage) {
  if (!s || (!cmd && count) || count > 8192)
    return 0;
  size_t bytes = (size_t)s->width * s->height * 4;
  cui_pixel_buffer *next = cui__pixels_create(bytes);
  if (!next)
    return 0;
  /* A full CLEAR overwrites every pixel. Partial updates need the old exterior.
   */
  if (damage || !count || cmd[0].op != CUI_DRAW_CLEAR)
    memcpy(next->data, s->pixels, bytes);
  if (!cui__render(s, cmd, count, next->data, damage)) {
    cui__pixels_release(next);
    return 0;
  }
  cui__pixels_release(s->buffer);
  s->buffer = next;
  s->pixels = next->data;
  return 1;
}
int cui_surface_render(cui_surface *s, const cui_draw_command *cmd,
                       size_t count) {
  return render_surface(s, cmd, count, NULL);
}
int cui_surface_render_region(cui_surface *s, const cui_draw_command *cmd,
                              size_t count, double x, double y, double width,
                              double height) {
  if (!s || !cmd || !count || count > 8192 || cmd[0].op != CUI_DRAW_CLEAR ||
      !isfinite(x) || !isfinite(y) || !isfinite(width) || !isfinite(height) ||
      x < 0 || y < 0 || width <= 0 || height <= 0 ||
      x + width > s->width / s->scale || y + height > s->height / s->scale)
    return 0;
  cui_damage d = {(int)floor(x * s->scale), (int)floor(y * s->scale),
                  (int)ceil((x + width) * s->scale),
                  (int)ceil((y + height) * s->scale)};
  return render_surface(s, cmd, count, &d);
}
size_t cui_surface_read(const cui_surface *s, unsigned char *rgba,
                        size_t capacity, int *width, int *height) {
  if (!s)
    return 0;
  if (width)
    *width = s->width;
  if (height)
    *height = s->height;
  size_t count = (size_t)s->width * s->height, bytes = count * 4;
  if (rgba && capacity >= bytes)
    for (size_t i = 0; i < count; ++i) {
      uint32_t v = s->pixels[i];
      unsigned a = v >> 24;
      if (a == 255) {
        rgba[i * 4] = (unsigned char)(v >> 16);
        rgba[i * 4 + 1] = (unsigned char)(v >> 8);
        rgba[i * 4 + 2] = (unsigned char)v;
      } else {
        rgba[i * 4] = (unsigned char)(a ? ((v >> 16) & 255) * 255 / a : 0);
        rgba[i * 4 + 1] = (unsigned char)(a ? ((v >> 8) & 255) * 255 / a : 0);
        rgba[i * 4 + 2] = (unsigned char)(a ? (v & 255) * 255 / a : 0);
      }
      rgba[i * 4 + 3] = (unsigned char)a;
    }
  return bytes;
}
static void clear_regions(cui_canvas_state *s) {
  for (size_t i = 0; i < s->count; ++i)
    free((void *)s->regions[i].label);
  free(s->regions);
  s->regions = NULL;
  s->count = 0;
}
static void dispose(void *data) {
  cui_canvas_state *s = data;
  clear_regions(s);
  cui_surface_release(s->surface);
  free(s);
}
cui_canvas_state *cui__canvas_state(const cui_widget *w) {
  return w && w->destroy_payload == dispose ? w->payload : NULL;
}
cui_widget *cui_canvas(cui_widget *parent) {
  if (!parent)
    return NULL;
  cui_canvas_state *s = calloc(1, sizeof(*s));
  if (!s)
    return NULL;
  cui_widget *w = cui__append(parent, CUI_CANVAS, "", CUI_VERTICAL, 0);
  if (!w) {
    free(s);
    return NULL;
  }
  s->widget = w;
  s->opacity = 1;
  w->payload = s;
  w->destroy_payload = dispose;
  if (!cui__canvas_attach(w))
    return NULL;
  return w;
}
int cui_canvas_set_surface(cui_widget *w, cui_surface *surface) {
  cui_canvas_state *s = cui__canvas_state(w);
  if (!s || !surface)
    return 0;
  if (!cui_surface_retain(surface))
    return 0;
  if (!cui__canvas_present(w, surface, s->opacity)) {
    size_t bytes = cui_surface_read(surface, NULL, 0, NULL, NULL);
    unsigned char *rgba = malloc(bytes);
    if (!rgba) {
      cui_surface_release(surface);
      return 0;
    }
    cui_surface_read(surface, rgba, bytes, NULL, NULL);
    if (s->opacity < 1)
      for (size_t i = 3; i < bytes; i += 4)
        rgba[i] = (unsigned char)(rgba[i] * s->opacity + 0.5);
    /* Adopt our own RGBA allocation instead of making the public API copy. */
    free(w->pixels);
    w->pixels = rgba;
    w->image_width = surface->width;
    w->image_height = surface->height;
    cui__backend_media(w);
  }
  cui_surface_release(s->surface);
  s->surface = surface;
  return 1;
}
int cui_canvas_set_regions(cui_widget *w, const cui_canvas_region *regions,
                           size_t count) {
  cui_canvas_state *s = cui__canvas_state(w);
  if (!s || count > 256 || (!regions && count))
    return 0;
  int same = count == s->count;
  for (size_t i = 0; same && i < count; ++i) {
    const cui_canvas_region *a = regions + i, *b = s->regions + i;
    same = a->id == b->id && a->x == b->x && a->y == b->y &&
           a->width == b->width && a->height == b->height &&
           a->enabled == b->enabled && a->label && !strcmp(a->label, b->label);
  }
  if (same)
    return 1;
  cui_canvas_state next = {0};
  next.regions = count ? calloc(count, sizeof(*regions)) : NULL;
  if (count && !next.regions)
    return 0;
  for (size_t i = 0; i < count; ++i) {
    const cui_canvas_region *r = regions + i;
    if (!r->id || !r->label || !r->label[0] || strlen(r->label) > 4096 ||
        !isfinite(r->x) || !isfinite(r->y) || !isfinite(r->width) ||
        !isfinite(r->height) || r->width <= 0 || r->height <= 0)
      goto fail;
    for (size_t j = 0; j < i; ++j)
      if (regions[j].id == r->id)
        goto fail;
    next.regions[i] = *r;
    next.regions[i].label = NULL;
    next.count = i + 1;
    char *label = malloc(strlen(r->label) + 1);
    if (!label)
      goto fail;
    strcpy(label, r->label);
    next.regions[i].label = label;
  }
  clear_regions(s);
  s->regions = next.regions;
  s->count = count;
  int focus_ok = 0, press_ok = 0;
  for (size_t i = 0; i < count; ++i)
    if (s->regions[i].enabled) {
      focus_ok |= s->regions[i].id == s->focus;
      press_ok |= s->regions[i].id == s->pressed;
    }
  if (!focus_ok)
    s->focus = 0;
  if (!press_ok)
    s->pressed = 0;
  cui__canvas_regions(w);
  return 1;
fail:
  clear_regions(&next);
  return 0;
}
void cui_canvas_on_event(cui_widget *w, cui_canvas_callback callback,
                         void *data) {
  cui_canvas_state *s = cui__canvas_state(w);
  if (s) {
    s->callback = callback;
    s->userdata = data;
  }
}
static unsigned hit(cui_canvas_state *s, double x, double y) {
  for (size_t i = s->count; i; --i) {
    cui_canvas_region *r = s->regions + i - 1;
    if (r->enabled && x >= r->x && y >= r->y && x < r->x + r->width &&
        y < r->y + r->height)
      return r->id;
  }
  return 0;
}
static int enabled(cui_widget *w) {
  for (; w; w = w->parent)
    if (!w->enabled || w->hidden)
      return 0;
  return 1;
}
static void dispatch(cui_canvas_state *s, cui_canvas_event *event) {
  if (enabled(s->widget) && s->callback)
    s->callback(s->widget, event, s->userdata);
}
void cui__canvas_focus(cui_widget *w, unsigned id) {
  cui_canvas_state *s = cui__canvas_state(w);
  if (!s || s->focus == id)
    return;
  s->focus = id;
  cui_canvas_event e = {CUI_CANVAS_FOCUS, id, 0, 0, 0, 0, 0};
  dispatch(s, &e);
}
int cui_canvas_activate_region(cui_widget *w, unsigned id) {
  cui_canvas_state *s = cui__canvas_state(w);
  if (!s || !enabled(w))
    return 0;
  for (size_t i = 0; i < s->count; ++i)
    if (s->regions[i].id == id && s->regions[i].enabled) {
      cui_canvas_event e = {CUI_CANVAS_ACTIVATE, id, 0, 0, 0, 0, 0};
      dispatch(s, &e);
      return 1;
    }
  return 0;
}
int cui_canvas_focus_region(cui_widget *w, unsigned id) {
  cui_canvas_state *s = cui__canvas_state(w);
  if (!s || !enabled(w))
    return 0;
  for (size_t i = 0; i < s->count; ++i)
    if (s->regions[i].id == id && s->regions[i].enabled) {
      cui__canvas_focus(w, id);
      cui_focus(w);
      return 1;
    }
  return 0;
}
void cui__canvas_event(cui_widget *w, cui_canvas_event_kind kind, double x,
                       double y, double dx, double dy, unsigned mods) {
  cui_canvas_state *s = cui__canvas_state(w);
  if (!s || !s->surface || !enabled(w) || !isfinite(x) || !isfinite(y) ||
      !isfinite(dx) || !isfinite(dy))
    return;
  unsigned id = hit(s, x, y);
  cui_canvas_event e = {kind, id, x, y, dx, dy, mods};
  if (kind == CUI_CANVAS_PRESS) {
    s->pressed = id;
    cui__canvas_focus(w, id);
  }
  unsigned pressed = s->pressed;
  if (kind == CUI_CANVAS_RELEASE)
    s->pressed = 0;
  dispatch(s, &e);
  if (kind == CUI_CANVAS_RELEASE && id && id == pressed && hit(s, x, y) == id) {
    e.kind = CUI_CANVAS_ACTIVATE;
    dispatch(s, &e);
  }
}
int cui__canvas_key(cui_widget *w, int backwards, int activate) {
  cui_canvas_state *s = cui__canvas_state(w);
  if (!s || !s->count)
    return 0;
  if (activate)
    return cui_canvas_activate_region(w, s->focus);
  int at = backwards ? 0 : -1;
  for (size_t i = 0; i < s->count; ++i)
    if (s->regions[i].id == s->focus)
      at = (int)i;
  for (size_t i = 0; i < s->count; ++i) {
    at = (at + (backwards ? -1 : 1) + (int)s->count) % (int)s->count;
    if (s->regions[at].enabled) {
      cui__canvas_focus(w, s->regions[at].id);
      return 1;
    }
  }
  return 0;
}
int cui_set_opacity(cui_widget *w, double value) {
  if (!w || !isfinite(value) || value < 0 || value > 1)
    return 0;
  cui_canvas_state *s = cui__canvas_state(w);
  if (s) {
    double old = s->opacity;
    s->opacity = value;
    if (s->surface && !cui_canvas_set_surface(w, s->surface)) {
      s->opacity = old;
      return 0;
    }
    return 1;
  }
  if (!cui__native_opacity(w, value))
    return 0;
  w->opacity = value;
  return 1;
}
double cui_get_opacity(const cui_widget *w) {
  cui_canvas_state *s = cui__canvas_state(w);
  return s ? s->opacity : w ? w->opacity : 1;
}
