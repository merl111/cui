/* Linux diagnostic benchmark: cc -O2 -Iinclude benchmarks/performance.c
 * build/libcui.a $(pkg-config --cflags --libs gtk4) -lm -o /tmp/cui-perf
 * GSK_RENDERER=cairo GTK_A11Y=none xvfb-run -a /tmp/cui-perf
 * Times synchronous API work; excludes native presentation and compositor
 * latency. No production state is changed. JSON lines contain milliseconds, not
 * FPS. */
#include "cui.h"
#include "cui_draw.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NDEBUG
#error                                                                         \
    "Build this benchmark with assertions enabled; assertions execute the measured API calls."
#endif

#define SAMPLES 11
static int compare(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return (x > y) - (x < y);
}
static void report(const char *name, int scale, int items, double *times) {
  qsort(times, SAMPLES, sizeof(*times), compare);
  printf("{\"case\":\"%s\",\"scale\":%d,\"items\":%d,\"median_ms\":%.4f,\"p95_"
         "ms\":%.4f,\"samples_ms\":[",
         name, scale, items, times[SAMPLES / 2] * 1000,
         times[SAMPLES - 1] * 1000);
  for (int i = 0; i < SAMPLES; ++i)
    printf("%s%.4f", i ? "," : "", times[i] * 1000);
  puts("]}");
  fflush(stdout);
}
static void drawing(cui_widget *canvas, int scale) {
  cui_surface *s = cui_surface_create(1280, 900, scale);
  assert(s);
  cui_icon_asset *icon = cui_icon_symbol(CUI_SYMBOL_PLAY);
  assert(icon);
  cui_draw_command commands[81] = {0};
  commands[0].op = CUI_DRAW_CLEAR;
  commands[0].color = 0x172735ff;
  const char *names[] = {
      "clear",     "rounded_rectangles", "text_labels",  "vector_icons",
      "card_blur", "full_surface_blur",  "group_opacity"};
  for (int kind = 0; kind < 7; ++kind) {
    size_t count = 1;
    if (kind >= 1 && kind <= 3)
      for (int i = 0; i < 64; ++i) {
        cui_draw_command c = {0};
        c.p[0] = 16 + (i % 8) * 156;
        c.p[1] = 16 + (i / 8) * 106;
        c.color = 0x89bbffcc;
        if (kind == 1) {
          c.op = CUI_DRAW_RECT;
          c.p[2] = 140;
          c.p[3] = 90;
          c.p[4] = 16;
        }
        if (kind == 2) {
          c.op = CUI_DRAW_TEXT;
          c.p[2] = 140;
          c.p[3] = 16;
          c.p[4] = 500;
          c.text = "Waypoint · City";
        }
        if (kind == 3) {
          c.op = CUI_DRAW_ICON;
          c.p[2] = 24;
          c.p[3] = 24;
          c.icon = icon;
        }
        commands[count++] = c;
      }
    if (kind == 4 || kind == 5) {
      commands[count++] = (cui_draw_command){
          .op = CUI_DRAW_MATERIAL,
          .p = {kind == 4 ? 710 : 0, kind == 4 ? 155 : 0,
                kind == 4 ? 382 : 1280, kind == 4 ? 648 : 900, 23, 20},
          .color = 0x332632c9};
    }
    if (kind == 6) {
      commands[count++] = (cui_draw_command){.op = CUI_DRAW_LAYER, .p = {.8}};
      commands[count++] = (cui_draw_command){
          .op = CUI_DRAW_RECT, .p = {20, 20, 80, 40, 8}, .color = 0xffffffcc};
      commands[count++] = (cui_draw_command){.op = CUI_DRAW_END_LAYER};
    }
    double times[SAMPLES];
    for (int i = -2; i < SAMPLES; ++i) {
      double t = cui_time();
      assert(cui_surface_render(s, commands, count));
      if (i >= 0)
        times[i] = cui_time() - t;
    }
    report(names[kind], scale, (int)count, times);
  }
  size_t bytes = cui_surface_read(s, NULL, 0, NULL, NULL);
  unsigned char *rgba = malloc(bytes);
  assert(rgba);
  double times[SAMPLES];
  for (int i = -2; i < SAMPLES; ++i) {
    double t = cui_time();
    cui_surface_read(s, rgba, bytes, NULL, NULL);
    if (i >= 0)
      times[i] = cui_time() - t;
  }
  report("rgba_readback", scale, 1280 * 900 * scale * scale, times);
  for (int i = -2; i < SAMPLES; ++i) {
    double t = cui_time();
    assert(cui_canvas_set_surface(canvas, s));
    if (i >= 0)
      times[i] = cui_time() - t;
  }
  report("canvas_upload", scale, 1280 * 900 * scale * scale, times);
  cui_draw_command image = {
      .op = CUI_DRAW_ICON, .p = {0, 0, 1280, 900}, .color = 0xffffffff};
  cui_icon_asset *bitmap = cui_icon_rgba(rgba, 1280 * scale, 900 * scale);
  assert(bitmap);
  image.icon = bitmap;
  for (int i = -2; i < SAMPLES; ++i) {
    double t = cui_time();
    assert(cui_surface_render(s, &image, 1));
    if (i >= 0)
      times[i] = cui_time() - t;
  }
  report("cached_background_blit", scale, 1280 * 900 * scale * scale, times);
  free(rgba);
  cui_icon_release(bitmap);
  cui_icon_release(icon);
  cui_surface_release(s);
}
/* Same final pixels in both modes. Build CUI_PERF_BASELINE against the saved
 * pre-optimization library: it redraws the complete scene for the same change.
 */
static void partial_updates(int scale) {
  cui_surface *s = cui_surface_create(1280, 900, scale);
  assert(s);
  cui_draw_command commands[66] = {{.op = CUI_DRAW_CLEAR, .color = 0x172735ff}};
  for (int i = 0; i < 64; ++i)
    commands[i + 1] = (cui_draw_command){
        .op = CUI_DRAW_RECT,
        .p = {16 + (i % 8) * 156, 16 + (i / 8) * 106, 140, 90, 16},
        .color = 0x89bbffcc};
  commands[65] = (cui_draw_command){
      .op = CUI_DRAW_RECT, .p = {32, 32, 80, 40, 8}, .color = 0x445566ff};
  assert(cui_surface_render(s, commands, 66));
  double times[SAMPLES];
  for (int i = -2; i < SAMPLES; ++i) {
    commands[65].color ^= 0x00110000;
    double t = cui_time();
#ifdef CUI_PERF_BASELINE
    assert(cui_surface_render(s, commands, 66));
#else
    assert(cui_surface_render_region(s, commands, 66, 30, 30, 84, 44));
#endif
    if (i >= 0)
      times[i] = cui_time() - t;
  }
  report("small_damage_update", scale, 66, times);
  /* Cold/miss workload complements the repeated-input blur cases above. */
  cui_draw_command material[] = {{.op = CUI_DRAW_CLEAR, .color = 0x172735ff},
                                 {.op = CUI_DRAW_MATERIAL,
                                  .p = {710, 155, 382, 648, 23, 20},
                                  .color = 0x332632c9}};
  for (int i = -2; i < SAMPLES; ++i) {
    material[0].color += 0x01000000;
    double t = cui_time();
    assert(cui_surface_render(s, material, 2));
    if (i >= 0)
      times[i] = cui_time() - t;
  }
  report("card_blur_changed_input", scale, 2, times);
  cui_surface_release(s);
}
static void models(cui_widget *root) {
  const char *headers[] = {"Name", "Status", "Owner"};
  cui_widget *table = cui_table(root, headers, 3);
  assert(table);
  const char **cells = malloc(4096 * 3 * sizeof(*cells));
  assert(cells);
  for (int i = 0; i < 4096 * 3; ++i)
    cells[i] = i % 3 == 0 ? "Harbor project" : i % 3 == 1 ? "Active" : "Studio";
  for (int count = 64; count <= 4096; count *= 8) {
    double times[SAMPLES];
    for (int i = -2; i < SAMPLES; ++i) {
      double t = cui_time();
      assert(cui_table_set_rows(table, cells, (size_t)count));
      if (i >= 0)
        times[i] = cui_time() - t;
    }
    report("table_replace", 1, count, times);
  }
  free(cells);
  cui_widget *canvas = cui_canvas(root);
  assert(canvas);
  cui_canvas_region regions[256];
  for (int i = 0; i < 256; ++i)
    regions[i] =
        (cui_canvas_region){(unsigned)i + 1, (float)i, 0, 10, 10, "Action", 1};
  assert(cui_canvas_set_regions(canvas, regions, 256));
  double times[SAMPLES];
  for (int i = -2; i < SAMPLES; ++i) {
    double t = cui_time();
    assert(cui_canvas_set_regions(canvas, regions, 256));
    if (i >= 0)
      times[i] = cui_time() - t;
  }
  report("regions_unchanged", 1, 256, times);
  for (int i = -2; i < SAMPLES; ++i) {
    regions[0].enabled = !regions[0].enabled;
    double t = cui_time();
    assert(cui_canvas_set_regions(canvas, regions, 256));
    if (i >= 0)
      times[i] = cui_time() - t;
  }
  report("regions_one_change", 1, 256, times);
}
int main(void) {
  cui_app *app = cui_app_create();
  assert(app);
  cui_window *w = cui_window_create(app, "Performance diagnostics", 1280, 900);
  assert(w);
  cui_widget *root = cui_window_root(w);
  cui_widget *canvas = cui_canvas(root);
  assert(canvas);
  drawing(canvas, 1);
  drawing(canvas, 2);
  partial_updates(1);
  partial_updates(2);
  models(root);
  cui_app_destroy(app);
  for (int count = 32; count <= 512; count *= 4) {
    double times[SAMPLES];
    for (int i = -1; i < SAMPLES; ++i) {
      app = cui_app_create();
      assert(app);
      w = cui_window_create(app, "Construction diagnostics", 640, 480);
      assert(w);
      root = cui_window_root(w);
      double t = cui_time();
      for (int n = 0; n < count; ++n)
        assert(cui_button(root, "Action"));
      if (i >= 0)
        times[i] = cui_time() - t;
      cui_app_destroy(app);
    }
    report("button_construction", 1, count, times);
  }
}
