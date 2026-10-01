/* Write deterministic pixel output for byte-for-byte comparison of two builds.
 * Uses only the surface renderer; no display/server is needed. */
#include "cui_draw.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef NDEBUG
#error "Assertions must be enabled"
#endif
int main(void) {
  for (int scale = 1; scale <= 2; ++scale) {
    cui_surface *s = cui_surface_create(128, 96, scale);
    assert(s);
    size_t bytes = cui_surface_read(s, NULL, 0, NULL, NULL);
    unsigned char *rgba = malloc(bytes);
    assert(rgba);
    for (int scene = 0; scene < 32; ++scene) {
      cui_draw_command c[] = {
          {.op = CUI_DRAW_CLEAR, .color = 0x12345678},
          {.op = CUI_DRAW_GRADIENT,
           .p = {0, 0, 128, 96, 4},
           .color = 0x76543266,
           .color2 = 0xcc551199},
          {.op = CUI_DRAW_SAVE},
          {.op = CUI_DRAW_TRANSLATE, .p = {-3.25f + scene / 3.f, 2.75f}},
          {.op = CUI_DRAW_SCALE, .p = {.75f + scene / 32.f, 1.25f}},
          {.op = CUI_DRAW_CLIP, .p = {8, 5, 83, 66, 12}},
          {.op = CUI_DRAW_LINE, .p = {-50, 2, 170, 80, 7}, .color = 0x1122ffaa},
          {.op = CUI_DRAW_RECT, .p = {3, 7, 40, 37, 8}, .color = 0xff992244},
          {.op = CUI_DRAW_ELLIPSE, .p = {19, 8, 32, 45}, .color = 0xaa2233cc},
          {.op = CUI_DRAW_MATERIAL,
           .p = {-8, 16, 95, 32, 7, scene % 9},
           .color = 0x12345678},
          {.op = CUI_DRAW_MATERIAL,
           .p = {8, -16, 95, 72, 7, scene % 7},
           .color = 0x55667766},
          {.op = CUI_DRAW_LAYER, .p = {.7}},
          {.op = CUI_DRAW_SHADOW,
           .p = {13, 7, 66, 32, 8, scene % 6},
           .color = 0x11111199},
          {.op = CUI_DRAW_RECT, .p = {13, 7, 66, 32, 8}, .color = 0x11ff99bb},
          {.op = CUI_DRAW_END_LAYER},
          {.op = CUI_DRAW_RESTORE}};
      for (int repeat = 0; repeat < 2; ++repeat) {
        assert(cui_surface_render(s, c, sizeof(c) / sizeof(*c)));
        assert(cui_surface_read(s, rgba, bytes, NULL, NULL) == bytes);
        assert(fwrite(rgba, 1, bytes, stdout) == bytes);
      }
    }
    free(rgba);
    cui_surface_release(s);
  }
}
