#include "cui_draw_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define LIMIT 32
typedef struct rect {
  double x, y, w, h, r;
} rect;
typedef struct state {
  double sx, sy, tx, ty;
  rect clips[LIMIT];
  int count;
} state;
typedef struct painter {
  cui_surface *s;
  uint32_t *pixels;
  state st, saved[LIMIT];
  int depth, groups;
  uint32_t *layers[8];
  double alpha[8];
  int layer_depth[8];
  size_t bytes;
  cui_damage limit;
  cui_blur_cache *pending;
  size_t pending_bytes;
} painter;
static double clamp(double v, double low, double high) {
  return v < low ? low : v > high ? high : v;
}
static unsigned byte(double v) {
  return (unsigned)clamp(floor(v + 0.5), 0, 255);
}
static uint32_t premul(unsigned c) {
  unsigned a = c & 255;
  return a << 24 | (((c >> 24) * a + 127) / 255) << 16 |
         ((((c >> 16) & 255) * a + 127) / 255) << 8 |
         ((((c >> 8) & 255) * a + 127) / 255);
}
static uint32_t over(uint32_t dst, uint32_t src, double coverage) {
  if (!src || coverage <= 0)
    return dst;
  if (coverage == 1 && (src >> 24) == 255)
    return src;
  unsigned a = byte((src >> 24) * coverage), inv = 255 - a;
  unsigned r = byte(((src >> 16) & 255) * coverage) +
               (((dst >> 16) & 255) * inv + 127) / 255;
  unsigned g = byte(((src >> 8) & 255) * coverage) +
               (((dst >> 8) & 255) * inv + 127) / 255;
  unsigned b = byte((src & 255) * coverage) + ((dst & 255) * inv + 127) / 255;
  return (a + ((dst >> 24) * inv + 127) / 255) << 24 | r << 16 | g << 8 | b;
}
static double coverage(rect r, double x, double y) {
  if (r.w <= 0 || r.h <= 0)
    return 0;
  double radius = clamp(r.r, 0, fmin(r.w, r.h) / 2);
  double qx = fabs(x - r.x - r.w / 2) - (r.w / 2 - radius),
         qy = fabs(y - r.y - r.h / 2) - (r.h / 2 - radius);
  double d =
      (qx <= 0 || qy <= 0) ? fmax(qx, qy) - radius : hypot(qx, qy) - radius;
  return clamp(0.5 - d, 0, 1);
}
static double clip(painter *p, int x, int y) {
  double c = 1;
  for (int i = 0; i < p->st.count && c; i++)
    c = fmin(c, coverage(p->st.clips[i], x + 0.5, y + 0.5));
  return c;
}
static rect box(painter *p, const float *v) {
  return (rect){p->st.tx + v[0] * p->st.sx, p->st.ty + v[1] * p->st.sy,
                v[2] * p->st.sx, v[3] * p->st.sy,
                v[4] * fmin(p->st.sx, p->st.sy)};
}
static void surface_bounds(painter *p, rect r, int *x0, int *y0, int *x1,
                           int *y1) {
  *x0 = (int)clamp(floor(r.x - 1), 0, p->s->width);
  *y0 = (int)clamp(floor(r.y - 1), 0, p->s->height);
  *x1 = (int)clamp(ceil(r.x + r.w + 1), 0, p->s->width);
  *y1 = (int)clamp(ceil(r.y + r.h + 1), 0, p->s->height);
}
static void bounds(painter *p, rect r, int *x0, int *y0, int *x1, int *y1) {
  surface_bounds(p, r, x0, y0, x1, y1);
  if (*x0 < p->limit.x0)
    *x0 = p->limit.x0;
  if (*y0 < p->limit.y0)
    *y0 = p->limit.y0;
  if (*x1 > p->limit.x1)
    *x1 = p->limit.x1;
  if (*y1 > p->limit.y1)
    *y1 = p->limit.y1;
  for (int i = 0; i < p->st.count; ++i) {
    int a, b, c, d;
    surface_bounds(p, p->st.clips[i], &a, &b, &c, &d);
    if (*x0 < a)
      *x0 = a;
    if (*y0 < b)
      *y0 = b;
    if (*x1 > c)
      *x1 = c;
    if (*y1 > d)
      *y1 = d;
  }
}
static void pixel(painter *p, int x, int y, uint32_t color, double amount) {
  if (x < p->limit.x0 || y < p->limit.y0 || x >= p->limit.x1 ||
      y >= p->limit.y1)
    return;
  double a = amount * clip(p, x, y);
  if (a > 0) {
    size_t i = (size_t)y * p->s->width + x;
    p->pixels[i] = over(p->pixels[i], color, a);
  }
}
static uint32_t mix(uint32_t a, uint32_t b, double t) {
  uint32_t out = 0;
  for (int i = 0; i < 32; i += 8)
    out |= byte(((a >> i) & 255) * (1 - t) + ((b >> i) & 255) * t) << i;
  return out;
}
static void shape(painter *p, rect r, unsigned color, unsigned second,
                  int mode) {
  if (mode != 1 && (r.w <= 0 || r.h <= 0))
    return;
  /* Hoist rounded-rectangle geometry out of the pixel loop. Keeping this
   * explicit avoids compiler inlining decisions reintroducing per-pixel
   * radius/half-size calculations as clipping support grows. */
  double radius = clamp(r.r, 0, fmin(r.w, r.h) / 2);
  double half_w = r.w / 2, half_h = r.h / 2;
  double hx = r.w / 2 - radius, hy = r.h / 2 - radius;
  int x0, y0, x1, y1;
  bounds(p, r, &x0, &y0, &x1, &y1);
  uint32_t a = premul(color), b = premul(second);
  for (int y = y0; y < y1; ++y) {
    uint32_t c =
        mode == 2 ? mix(a, b, clamp((y + 0.5 - r.y) / fmax(r.h, 1), 0, 1)) : a;
    for (int x = x0; x < x1; ++x) {
      double v;
      if (mode == 1) {
        double rx = r.w / 2, ry = r.h / 2;
        double d = hypot((x + 0.5 - r.x - rx) / fmax(rx, 0.001),
                         (y + 0.5 - r.y - ry) / fmax(ry, 0.001));
        v = clamp((1 - d) * fmin(rx, ry) + 0.5, 0, 1);
      } else {
        double qx = fabs(x + 0.5 - r.x - half_w) - hx,
               qy = fabs(y + 0.5 - r.y - half_h) - hy;
        double d = (qx <= 0 || qy <= 0) ? (qx > qy ? qx : qy) - radius
                                        : hypot(qx, qy) - radius;
        v = clamp(0.5 - d, 0, 1);
      }
      if (v)
        pixel(p, x, y, c, v);
    }
  }
}
static void line(painter *p, double x, double y, double xx, double yy,
                 double width, unsigned color) {
  x = p->st.tx + x * p->st.sx;
  y = p->st.ty + y * p->st.sy;
  xx = p->st.tx + xx * p->st.sx;
  yy = p->st.ty + yy * p->st.sy;
  double r = width * fmin(p->st.sx, p->st.sy) / 2, dx = xx - x, dy = yy - y,
         den = dx * dx + dy * dy;
  rect b = {fmin(x, xx) - r, fmin(y, yy) - r, fabs(dx) + 2 * r,
            fabs(dy) + 2 * r, 0};
  int x0, y0, x1, y1;
  bounds(p, b, &x0, &y0, &x1, &y1);
  uint32_t c = premul(color);
  for (int j = y0; j < y1; ++j)
    for (int i = x0; i < x1; ++i) {
      double t =
          den ? clamp(((i + 0.5 - x) * dx + (j + 0.5 - y) * dy) / den, 0, 1)
              : 0;
      double d = hypot(i + 0.5 - x - t * dx, j + 0.5 - y - t * dy);
      double v = clamp(r + 0.5 - d, 0, 1);
      if (v)
        pixel(p, i, j, c, v);
    }
}
/* Separable box blur, bounded to the requested app-owned pixel rectangle. */
static int blur(uint32_t *data, int w, int h, int radius) {
  if (radius < 1)
    return 1;
  uint32_t *tmp = malloc((size_t)w * h * 4);
  if (!tmp)
    return 0;
  for (int pass = 0; pass < 2; ++pass) {
    int outer = pass ? w : h, inner = pass ? h : w;
    uint32_t *src = pass ? tmp : data, *dst = pass ? data : tmp;
    for (int row = 0; row < outer; ++row) {
      unsigned sums[4] = {0};
      for (int k = -radius; k <= radius; ++k) {
        int at = (int)clamp(k, 0, inner - 1);
        uint32_t c = src[pass ? (size_t)at * w + row : (size_t)row * w + at];
        for (int b = 0; b < 4; ++b)
          sums[b] += (c >> (b * 8)) & 255;
      }
      for (int j = 0; j < inner; ++j) {
        uint32_t c = 0;
        for (int b = 0; b < 4; ++b)
          c |= (sums[b] / (unsigned)(radius * 2 + 1)) << (b * 8);
        dst[pass ? (size_t)j * w + row : (size_t)row * w + j] = c;
        int remove = (int)clamp(j - radius, 0, inner - 1),
            add = (int)clamp(j + radius + 1, 0, inner - 1);
        uint32_t
            a = src[pass ? (size_t)add * w + row : (size_t)row * w + add],
            d = src[pass ? (size_t)remove * w + row : (size_t)row * w + remove];
        for (int b = 0; b < 4; ++b)
          sums[b] = sums[b] + ((a >> (b * 8)) & 255) - ((d >> (b * 8)) & 255);
      }
    }
  }
  free(tmp);
  return 1;
}
#define BLUR_CACHE_BYTES (32u * 1024u * 1024u)
/* Exact input comparison avoids hash collisions and stale backdrop reuse. */
static cui_blur_cache *cached_blur(painter *p, int x, int y, int w, int h,
                                   int radius, int shadow, rect r,
                                   unsigned tint, const uint32_t *source) {
  double shape[] = {r.x, r.y, r.w, r.h, r.r};
  for (cui_blur_cache *c = p->s->cache; c; c = c->next)
    if (c->x == x && c->y == y && c->w == w && c->h == h &&
        c->radius == radius && c->shadow == shadow &&
        (!shadow ||
         (c->tint == tint && !memcmp(shape, c->shape, sizeof(shape)))) &&
        (shadow || !memcmp(source, c->source, (size_t)w * h * 4)))
      return c;
  return NULL;
}
static void remember_blur(painter *p, int x, int y, int w, int h, int radius,
                          int shadow, rect r, unsigned tint, uint32_t *source,
                          uint32_t *result) {
  size_t bytes = (size_t)w * h * 8 + sizeof(cui_blur_cache);
  cui_blur_cache *c =
      bytes <= BLUR_CACHE_BYTES - p->pending_bytes ? malloc(sizeof(*c)) : NULL;
  if (!c) {
    free(source);
    free(result);
    return;
  }
  *c = (cui_blur_cache){.next = p->pending,
                        .x = x,
                        .y = y,
                        .w = w,
                        .h = h,
                        .radius = radius,
                        .shadow = shadow,
                        .shape = {r.x, r.y, r.w, r.h, r.r},
                        .tint = tint,
                        .bytes = bytes,
                        .source = source,
                        .result = result};
  p->pending = c;
  p->pending_bytes += bytes;
}
static void finish_cache(painter *p, int okay) {
  if (!okay) {
    cui__blur_cache_free(p->pending);
    return;
  }
  if (p->pending_bytes > BLUR_CACHE_BYTES - p->s->cache_bytes) {
    cui__blur_cache_free(p->s->cache);
    p->s->cache = NULL;
    p->s->cache_bytes = 0;
  }
  while (p->pending) {
    cui_blur_cache *c = p->pending;
    p->pending = c->next;
    c->next = p->s->cache;
    p->s->cache = c;
    p->s->cache_bytes += c->bytes;
  }
}
static int material(painter *p, rect r, double amount, unsigned tint,
                    int shadow) {
  int rad = (int)clamp(ceil(amount * fmin(p->st.sx, p->st.sy)), 0, 128);
  rect extent = {r.x - rad - 2, r.y - rad - 2, r.w + rad * 2 + 4,
                 r.h + rad * 2 + 4, 0};
  int ox0, oy0, ox1, oy1, ex0, ey0, ex1, ey1;
  bounds(p, shadow ? extent : r, &ox0, &oy0, &ox1, &oy1);
  if (ox1 <= ox0 || oy1 <= oy0)
    return 1;
  surface_bounds(p, extent, &ex0, &ey0, &ex1, &ey1);
  int x0 = ox0 - rad > ex0 ? ox0 - rad : ex0;
  int y0 = oy0 - rad > ey0 ? oy0 - rad : ey0;
  int x1 = ox1 + rad < ex1 ? ox1 + rad : ex1;
  int y1 = oy1 + rad < ey1 ? oy1 + rad : ey1;
  int w = x1 - x0, h = y1 - y0;
  size_t bytes = (size_t)w * h * 4;
  uint32_t *source = malloc(bytes);
  if (!source)
    return 0;
  uint32_t tint_pixel = premul(tint);
  if (!shadow)
    for (int y = 0; y < h; ++y)
      memcpy(source + (size_t)y * w,
             p->pixels + (size_t)(y + y0) * p->s->width + x0, (size_t)w * 4);
  cui_blur_cache *cached =
      cached_blur(p, x0, y0, w, h, rad, shadow, r, tint, source);
  uint32_t *sample = cached ? cached->result : malloc(bytes);
  if (!sample) {
    free(source);
    return 0;
  }
  if (!cached) {
    if (shadow)
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
          source[(size_t)y * w + x] =
              over(0, tint_pixel, coverage(r, x + x0 + 0.5, y + y0 + 0.5));
    memcpy(sample, source, bytes);
    if (!blur(sample, w, h, rad)) {
      free(source);
      free(sample);
      return 0;
    }
  }
  for (int y = oy0; y < oy1; ++y)
    for (int x = ox0; x < ox1; ++x) {
      double a = shadow ? 1 : coverage(r, x + 0.5, y + 0.5);
      if (!a)
        continue;
      uint32_t c = sample[(size_t)(y - y0) * w + x - x0];
      if (shadow)
        pixel(p, x, y, c, a);
      else {
        size_t i = (size_t)y * p->s->width + x;
        p->pixels[i] =
            mix(p->pixels[i], over(c, tint_pixel, 1), a * clip(p, x, y));
      }
    }
  if (cached)
    free(source);
  else
    remember_blur(p, x0, y0, w, h, rad, shadow, r, tint, source, sample);
  return 1;
}
static int text(painter *p, const cui_draw_command *c) {
  int w = 0, h = 0;
  int max = (int)clamp(c->p[2] * p->st.sx, 1, 4096);
  uint32_t *rgba=cui__draw_text_color(c->text ? c->text : "",c->font,c->p[3]*p->st.sy,(int)c->p[4],max,&w,&h,c->color);
  unsigned char *mask = rgba ? NULL :
      cui__draw_text(c->text ? c->text : "", c->font, c->p[3] * p->st.sy,
                     (int)c->p[4], max, &w, &h);
  if (!rgba && !mask)
    return 0;
  int xx = (int)clamp(p->st.tx + c->p[0] * p->st.sx, -1000000, 1000000),
      yy = (int)clamp(p->st.ty + c->p[1] * p->st.sy, -1000000, 1000000);
  int left = w, right = 0, top = h, bottom = 0;
  if (c->p[5] || c->p[6]) {
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x)
        if (rgba ? rgba[(size_t)y*w+x]>>24 : mask[(size_t)y * w + x]) {
          if (x < left)
            left = x;
          if (x + 1 > right)
            right = x + 1;
          if (y < top)
            top = y;
          if (y + 1 > bottom)
            bottom = y + 1;
        }
    if (right > left) {
      if (c->p[5] == 1)
        xx += (max - (right - left)) / 2 - left;
      else if (c->p[5] == 2)
        xx += max - right;
    }
    if (c->p[6] > 0 && bottom > top)
      yy += (int)((c->p[6] * p->st.sy - (bottom - top)) / 2) - top;
  }
  double clip_top = p->st.ty + c->p[1] * p->st.sy,
         clip_bottom = clip_top + c->p[6] * p->st.sy;
  uint32_t color = premul(c->color);
  for (int y = 0; y < h; ++y)
    if (y + yy >= p->limit.y0 && y + yy < p->limit.y1 &&
        (c->p[6] == 0 || (y + yy >= clip_top && y + yy < clip_bottom)))
      for (int x = 0; x < w; ++x)
        if (x + xx >= p->limit.x0 && x + xx < p->limit.x1 &&
            (rgba ? rgba[(size_t)y*w+x]>>24 : mask[(size_t)y * w + x]))
          pixel(p, x + xx, y + yy, rgba ? rgba[(size_t)y*w+x] : color, rgba ? 1 : mask[(size_t)y * w + x] / 255.);
  free(rgba);free(mask);
  return 1;
}
static int icon(painter *p, const cui_draw_command *c) {
  if (!c->icon)
    return 0;
  rect r = box(p, c->p);
  if (r.w <= 0 || r.h <= 0)
    return 1;
  if (r.w > 4096 || r.h > 4096)
    return 0;
  int w = (int)ceil(r.w), h = (int)ceil(r.h);
  uint32_t *data = cui__draw_asset(c->icon, w, h, c->color);
  if (!data)
    return 0;
  int x0, y0, x1, y1;
  bounds(p, r, &x0, &y0, &x1, &y1);
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x) {
      int ix = (int)floor(x + 0.5 - r.x), iy = (int)floor(y + 0.5 - r.y);
      if (ix >= 0 && ix < w && iy >= 0 && iy < h)
        pixel(p, x, y, data[(size_t)iy * w + ix], 1);
    }
  free(data);
  return 1;
}
static int command(painter *p, const cui_draw_command *c) {
  for (int i = 0; i < 8; ++i)
    if (!isfinite(c->p[i]) || fabs(c->p[i]) > 1000000)
      return 0;
  rect r = box(p, c->p);
  switch (c->op) {
  case CUI_DRAW_CLEAR: {
    if (p->depth || p->groups || p->st.count)
      return 0;
    uint32_t color = premul(c->color);
    int width = p->s->width;
    cui_damage d = p->limit;
    uint32_t *pixels = p->pixels;
    for (int y = d.y0; y < d.y1; ++y) {
      uint32_t *row = pixels + (size_t)y * width;
      for (int x = d.x0; x < d.x1; ++x)
        row[x] = color;
    }
    return 1;
  }
  case CUI_DRAW_SAVE:
    if (p->depth == LIMIT)
      return 0;
    p->saved[p->depth++] = p->st;
    return 1;
  case CUI_DRAW_RESTORE:
    if (!p->depth || (p->groups && p->depth <= p->layer_depth[p->groups - 1]))
      return 0;
    p->st = p->saved[--p->depth];
    return 1;
  case CUI_DRAW_TRANSLATE:
    p->st.tx += c->p[0] * p->st.sx;
    p->st.ty += c->p[1] * p->st.sy;
    return fabs(p->st.tx) <= 1e6 && fabs(p->st.ty) <= 1e6;
  case CUI_DRAW_SCALE:
    if (c->p[0] <= 0 || c->p[1] <= 0)
      return 0;
    p->st.sx *= c->p[0];
    p->st.sy *= c->p[1];
    return p->st.sx <= 1000 && p->st.sy <= 1000 && p->st.sx >= 0.001 &&
           p->st.sy >= 0.001;
  case CUI_DRAW_CLIP:
    if (p->st.count == LIMIT || r.w < 0 || r.h < 0 || r.r < 0)
      return 0;
    p->st.clips[p->st.count++] = r;
    return 1;
  case CUI_DRAW_LAYER: {
    if (p->groups == 8 || c->p[0] < 0 || c->p[0] > 1 ||
        p->bytes * (size_t)(p->groups + 2) > 256000000)
      return 0;
    uint32_t *next = calloc(1, p->bytes);
    if (!next)
      return 0;
    p->layers[p->groups] = p->pixels;
    p->alpha[p->groups] = c->p[0];
    p->layer_depth[p->groups] = p->depth;
    ++p->groups;
    p->pixels = next;
    return 1;
  }
  case CUI_DRAW_END_LAYER: {
    if (!p->groups || p->depth != p->layer_depth[p->groups - 1])
      return 0;
    uint32_t *top = p->pixels;
    --p->groups;
    p->pixels = p->layers[p->groups];
    for (int y = p->limit.y0; y < p->limit.y1; ++y)
      for (int x = p->limit.x0; x < p->limit.x1; ++x) {
        size_t i = (size_t)y * p->s->width + x;
        p->pixels[i] = over(p->pixels[i], top[i], p->alpha[p->groups]);
      }
    free(top);
    return 1;
  }
  case CUI_DRAW_LINE:
    if (c->p[4] < 0)
      return 0;
    line(p, c->p[0], c->p[1], c->p[2], c->p[3], c->p[4], c->color);
    return 1;
  case CUI_DRAW_TEXT:
    if (c->p[2] <= 0 || c->p[3] < 1 || c->p[3] * p->st.sy > 512 ||
        c->p[4] < 100 || c->p[4] > 900 || c->p[5] < 0 || c->p[5] > 2 ||
        c->p[5] != floor(c->p[5]) || c->p[6] < 0)
      return 0;
    return text(p, c);
  default:
    if (r.w < 0 || r.h < 0 || r.r < 0)
      return 0;
  }
  switch (c->op) {
  case CUI_DRAW_RECT:
    shape(p, r, c->color, 0, 0);
    return 1;
  case CUI_DRAW_ELLIPSE:
    shape(p, r, c->color, 0, 1);
    return 1;
  case CUI_DRAW_GRADIENT:
    shape(p, r, c->color, c->color2, 2);
    return 1;
  case CUI_DRAW_SHADOW:
  case CUI_DRAW_MATERIAL:
    if (c->p[5] < 0 || c->p[5] > 32)
      return 0;
    return material(p, r, c->p[5], c->color, c->op == CUI_DRAW_SHADOW);
  case CUI_DRAW_ICON:
    return icon(p, c);
  default:
    return 0;
  }
}
int cui__render(cui_surface *s, const cui_draw_command *commands, size_t count,
                uint32_t *pixels, const cui_damage *damage) {
  painter p = {0};
  p.s = s;
  p.pixels = pixels;
  p.bytes = (size_t)s->width * s->height * 4;
  p.st.sx = p.st.sy = s->scale;
  p.limit = (cui_damage){0, 0, s->width, s->height};
  if (damage) {
    /* Each backdrop blur can propagate dependency by at most 128 physical
     * pixels. Include the sum for chained blurs, independent of transforms.
     * Shadows synthesize their own source and do not propagate dependencies. */
    int halo = 0;
    for (size_t i = 0; i < count && halo < 4096; ++i)
      if (commands[i].op == CUI_DRAW_MATERIAL)
        halo += 128;
    p.limit = (cui_damage){(int)fmax(0, damage->x0 - halo),
                           (int)fmax(0, damage->y0 - halo),
                           (int)fmin(s->width, damage->x1 + halo),
                           (int)fmin(s->height, damage->y1 + halo)};
  }
  int okay = 1;
  for (size_t i = 0; i < count; ++i)
    if (!command(&p, commands + i)) {
      okay = 0;
      break;
    }
  if (p.depth || p.groups)
    okay = 0;
  while (p.groups) {
    free(p.pixels);
    p.pixels = p.layers[--p.groups];
  }
  finish_cache(&p, okay);
  if (okay && damage) {
    /* The expanded area is working storage, not additional caller damage. */
    for (int y = p.limit.y0; y < p.limit.y1; ++y) {
      size_t row = (size_t)y * s->width;
      if (y < damage->y0 || y >= damage->y1)
        memcpy(pixels + row + p.limit.x0, s->pixels + row + p.limit.x0,
               (size_t)(p.limit.x1 - p.limit.x0) * 4);
      else {
        memcpy(pixels + row + p.limit.x0, s->pixels + row + p.limit.x0,
               (size_t)(damage->x0 - p.limit.x0) * 4);
        memcpy(pixels + row + damage->x1, s->pixels + row + damage->x1,
               (size_t)(p.limit.x1 - damage->x1) * 4);
      }
    }
  }
  return okay;
}
