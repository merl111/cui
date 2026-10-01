#include "cui_draw.h"
#include <stdint.h>
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  cui_draw_command commands[32] = {0};
  size_t count = size / 37;
  if (count > 32)
    count = 32;
  for (size_t i = 0; i < count; ++i) {
    const uint8_t *v = data + i * 37;
    commands[i].op = v[0] % 16;
    memcpy(commands[i].p, v + 1, 32);
    memcpy(&commands[i].color, v + 33, 4);
    /* Font/icon ownership is tested separately; no arbitrary pointers. */
    if (commands[i].op == CUI_DRAW_TEXT || commands[i].op == CUI_DRAW_ICON)
      commands[i].op = CUI_DRAW_RECT;
  }
  cui_surface *s = cui_surface_create(8, 8, 1);
  if (s) {
    cui_surface_render(s, commands, count);
    /* Repeat for cache hits, then replay bounded damage. Invalid command
     * sequences must remain harmless on both public entry points. */
    cui_surface_render(s, commands, count);
    unsigned x = size ? data[0] % 8 : 0;
    unsigned y = size > 1 ? data[1] % 8 : 0;
    cui_surface_render_region(s, commands, count, x, y, 8-x, 8-y);
    cui_surface_release(s);
  }
  return 0;
}
