#include "cui_desktop.h"
#include "cui_draw_internal.h"
#include "safety/allocations.h"
#include <assert.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char pixels[64 * 64 * 4];
static void color_glyphs(void) {
  PangoFontFamily **families = NULL;
  int count = 0, available = 0;
  pango_font_map_list_families(pango_cairo_font_map_get_default(), &families,
                              &count);
  for (int i = 0; i < count; ++i)
    if (!strcmp(pango_font_family_get_name(families[i]), "Noto Color Emoji"))
      available = 1;
  g_free(families);
  if (!available) return;
  cui_surface *s = cui_surface_create(64, 64, 1);
  assert(s);
  cui_draw_command emoji = {.op = CUI_DRAW_TEXT, .p = {0, 0, 64, 32, 400},
                            .text = "👍", .font = "Noto Color Emoji",
                            .color = 0x000000ff};
  assert(cui_surface_render(s, &emoji, 1));
  int width, height;
  cui_surface_read(s, pixels, sizeof(pixels), &width, &height);
  unsigned colored = 0;
  for (size_t i = 0; i < sizeof(pixels); i += 4)
    if (pixels[i + 3] > 128 && pixels[i] > pixels[i + 2] + 40)
      ++colored;
  assert(colored > 30); /* The old alpha-only path tinted every glyph black. */
  cui_surface_release(s);
}
static unsigned char *read_pixel(cui_surface *s, int x, int y) {
  int w, h;
  assert(cui_surface_read(s, pixels, sizeof(pixels), &w, &h) < sizeof(pixels));
  return pixels + ((size_t)y * w + x) * 4;
}
static void raster(void) {
  color_glyphs();
  assert(!cui_surface_create(0, 10, 1));
  assert(!cui_surface_create(10, 10, NAN));
  assert(!cui_surface_create(4096, 4096, 1));
  cui_surface *s = cui_surface_create(16, 16, 2);
  assert(s);
  cui_draw_command group[] = {
      {.op = CUI_DRAW_CLEAR},
      {.op = CUI_DRAW_LAYER, .p = {.5}},
      {.op = CUI_DRAW_RECT, .p = {0, 0, 12, 12}, .color = 0xff0000ff},
      {.op = CUI_DRAW_RECT, .p = {4, 4, 12, 12}, .color = 0x0000ffff},
      {.op = CUI_DRAW_END_LAYER}};
  assert(cui_surface_render(s, group, 5));
  unsigned char *v = read_pixel(s, 12, 12);
  assert(v[0] == 0 && v[2] >= 254 && v[3] == 128);
  v = read_pixel(s, 2, 2);
  assert(v[0] >= 254 && v[3] == 128);
  cui_draw_command nested[] = {
      {.op = CUI_DRAW_CLEAR},
      {.op = CUI_DRAW_SAVE},
      {.op = CUI_DRAW_TRANSLATE, .p = {2, 2}},
      {.op = CUI_DRAW_CLIP, .p = {0, 0, 10, 10, 3}},
      {.op = CUI_DRAW_LAYER, .p = {.5}},
      {.op = CUI_DRAW_LAYER, .p = {.5}},
      {.op = CUI_DRAW_RECT, .p = {0, 0, 16, 16}, .color = 0xffffffff},
      {.op = CUI_DRAW_END_LAYER},
      {.op = CUI_DRAW_END_LAYER},
      {.op = CUI_DRAW_RESTORE}};
  assert(cui_surface_render(s, nested, 10));
  v = read_pixel(s, 12, 12);
  assert(v[3] == 64);
  v = read_pixel(s, 4, 4);
  assert(v[3] == 0);
  v = read_pixel(s, 27, 27);
  assert(v[3] == 0);
  unsigned char before[32 * 32 * 4];
  memcpy(before, pixels, sizeof(before));
  cui_draw_command bad[] = {{.op = CUI_DRAW_CLEAR, .color = 0xff0000ff},
                            {.op = CUI_DRAW_RESTORE}};
  assert(!cui_surface_render(s, bad, 2));
  read_pixel(s, 0, 0);
  assert(!memcmp(before, pixels, sizeof(before)));
  bad[1].op = CUI_DRAW_SCALE;
  bad[1].p[0] = NAN;
  assert(!cui_surface_render(s, bad, 2));
  cui_draw_command blur[] = {
      {.op = CUI_DRAW_CLEAR, .color = 0x000000ff},
      {.op = CUI_DRAW_RECT, .p = {8, 0, 8, 16}, .color = 0xffffffff},
      {.op = CUI_DRAW_MATERIAL, .p = {0, 0, 16, 16, 0, 2}}};
  assert(cui_surface_render(s, blur, 3));
  v = read_pixel(s, 15, 16);
  assert(v[0] > 30 && v[0] < 220 && v[3] == 255);
  cui_draw_command text = {.op = CUI_DRAW_TEXT,
                           .p = {0, 0, 16, 8, 500},
                           .color = 0xffffffff,
                           .text = "Å 字"};
  assert(cui_surface_render(s, &text, 1));
  cui_draw_command centered[] = {
      {.op = CUI_DRAW_CLEAR},
      {.op = CUI_DRAW_TEXT,
       .p = {0, 0, 16, 8, 500, CUI_DRAW_ALIGN_CENTER, 16},
       .text = "Hi",
       .color = 0xffffffff}};
  assert(cui_surface_render(s, centered, 2));
  read_pixel(s, 0, 0);
  int left = 32, right = 0, top = 32, bottom = 0;
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 32; ++x)
      if (pixels[(y * 32 + x) * 4 + 3]) {
        if (x < left)
          left = x;
        if (x + 1 > right)
          right = x + 1;
        if (y < top)
          top = y;
        if (y + 1 > bottom)
          bottom = y + 1;
      }
  assert(right > left && bottom > top);
  assert(abs(left + right - 32) <= 2 && abs(top + bottom - 32) <= 2);
  centered[1].p[5] = 1.5f;
  assert(!cui_surface_render(s, centered, 2));
  cui_icon_asset *asset = cui_icon_symbol(CUI_SYMBOL_PLAY);
  assert(asset);
  cui_draw_command icon = {.op = CUI_DRAW_ICON,
                           .p = {0, 0, 12, 12},
                           .icon = asset,
                           .color = 0xffffffff};
  assert(cui_surface_render(s, &icon, 1));
  cui_icon_release(asset);
  /* Mandatory allocation failures preserve pixels; optional cache allocation
   * failures may still succeed. Each trial begins cold, with identical input.
   */
  cui_draw_command work[] = {
      {.op = CUI_DRAW_LAYER, .p = {.5}},
      {.op = CUI_DRAW_SHADOW, .p = {2, 2, 12, 12, 3, 2}, .color = 0x00000088},
      {.op = CUI_DRAW_END_LAYER}};
  uint32_t original[32 * 32], expected[32 * 32];
  memcpy(original, s->pixels, sizeof(original));
  cui_surface *probe = cui_surface_create(16, 16, 2);
  assert(probe);
  memcpy(probe->pixels, original, sizeof(original));
  cui_test_fail_allocation(0);
  assert(cui_surface_render(probe, work, 3));
  size_t attempts = cui_test_allocation_attempts();
  memcpy(expected, probe->pixels, sizeof(expected));
  cui_surface_release(probe);
  size_t live = cui_test_live_allocations();
  for (size_t n = 1; n <= attempts; ++n) {
    cui_test_fail_allocation(0);
    probe = cui_surface_create(16, 16, 2);
    assert(probe);
    memcpy(probe->pixels, original, sizeof(original));
    cui_test_fail_allocation(n);
    int okay = cui_surface_render(probe, work, 3);
    assert(cui_test_allocation_failed());
    cui_test_fail_allocation(0);
    assert(
        !memcmp(probe->pixels, okay ? expected : original, sizeof(original)));
    cui_surface_release(probe);
    assert(cui_test_live_allocations() == live);
  }
  cui_surface_release(s);
}
static void incremental(void) {
  for (int scale = 1; scale <= 2; ++scale) {
    cui_surface *partial = cui_surface_create(640, 520, scale);
    cui_surface *full = cui_surface_create(640, 520, scale);
    assert(partial && full);
    cui_draw_command commands[] = {
        {.op = CUI_DRAW_CLEAR, .color = 0x172735ff},
        {.op = CUI_DRAW_GRADIENT,
         .p = {0, 0, 640, 520, 8},
         .color = 0xff1111ff,
         .color2 = 0x2244ffff},
        {.op = CUI_DRAW_SAVE},
        {.op = CUI_DRAW_TRANSLATE, .p = {2.25, 3.5}},
        {.op = CUI_DRAW_CLIP, .p = {1, 1, 633, 513, 9}},
        {.op = CUI_DRAW_RECT, .p = {275, 210, 26, 23, 4}, .color = 0x11ee2266},
        {.op = CUI_DRAW_MATERIAL,
         .p = {8, 8, 615, 495, 7, 5},
         .color = 0x12345678},
        {.op = CUI_DRAW_MATERIAL,
         .p = {18, 18, 597, 478, 6, 3},
         .color = 0x77665588},
        {.op = CUI_DRAW_LAYER, .p = {.6}},
        {.op = CUI_DRAW_SHADOW,
         .p = {20, 20, 13, 13, 4, 4},
         .color = 0x123456aa},
        {.op = CUI_DRAW_RECT, .p = {20, 20, 13, 13, 4}, .color = 0x1144ffaa},
        {.op = CUI_DRAW_END_LAYER},
        {.op = CUI_DRAW_RESTORE}};
    size_t count = sizeof(commands) / sizeof(*commands),
           bytes = (size_t)partial->width * partial->height * 4;
    assert(cui_surface_render(partial, commands, count));
    uint32_t *old = malloc(bytes);
    assert(old);
    memcpy(old, partial->pixels, bytes);
    for (int trial = 0; trial < 8; ++trial) {
      commands[5].color += 0x11000000;
      assert(cui_surface_render(full, commands, count));
      /* Full render as oracle: only committed damage changes, including when
       * the blur samples outside it or output is partially transparent. */
      assert(cui_surface_render_region(partial, commands, count, 280.25, 220.25,
                                       29.5, 28.5));
      for (int y = 0; y < partial->height; ++y)
        for (int x = 0; x < partial->width; ++x) {
          size_t i = (size_t)y * partial->width + x;
          int inside = x >= (int)floor(280.25 * scale) &&
                       x < (int)ceil(309.75 * scale) &&
                       y >= (int)floor(220.25 * scale) &&
                       y < (int)ceil(248.75 * scale);
          assert(partial->pixels[i] == (inside ? full->pixels[i] : old[i]));
        }
      assert(partial->cache_bytes <= 32u * 1024u * 1024u);
    }
    memcpy(old, partial->pixels, bytes);
    commands[count - 1].op = CUI_DRAW_END_LAYER;
    assert(!cui_surface_render_region(partial, commands, count, 1, 1, 20, 20));
    assert(!memcmp(old, partial->pixels, bytes));
    assert(!cui_surface_render_region(partial, commands, count, -1, 0, 4, 4));
    assert(!cui_surface_render_region(partial, commands, count, 0, 0, NAN, 4));
    assert(
        !cui_surface_render_region(partial, commands, count, 630, 510, 20, 20));
    free(old);
    cui_surface_release(partial);
    cui_surface_release(full);
  }
  cui_surface *s = cui_surface_create(256, 256, 1);
  assert(s);
  unsigned char *out = malloc(256 * 256 * 4);
  assert(out);
  for (unsigned a = 0; a < 256; ++a)
    for (unsigned c = 0; c < 256; ++c)
      s->pixels[a * 256 + c] = (a << 24) | (c * a / 255 << 16) |
                               ((255 - c) * a / 255 << 8) |
                               (((c * 73) % 256) * a / 255);
  cui_surface_read(s, out, 256 * 256 * 4, NULL, NULL);
  for (size_t i = 0; i < 256 * 256; ++i) {
    unsigned v = s->pixels[i], a = v >> 24;
    assert(out[4 * i] == (a ? ((v >> 16) & 255) * 255 / a : 0));
    assert(out[4 * i + 1] == (a ? ((v >> 8) & 255) * 255 / a : 0));
    assert(out[4 * i + 2] == (a ? (v & 255) * 255 / a : 0));
    assert(out[4 * i + 3] == a);
  }
  free(out);
  cui_surface_release(s);
}
static int activated, remove_on_release;
static double event_x;
static void event(cui_widget *w, const cui_canvas_event *e, void *data) {
  (void)data;
  event_x = e->x;
  if (e->kind == CUI_CANVAS_ACTIVATE)
    ++activated;
  if (remove_on_release && e->kind == CUI_CANVAS_RELEASE)
    assert(cui_canvas_set_regions(w, NULL, 0));
}
static cui_widget *canvas;
static cui_app *app;
static void native(void *unused) {
  (void)unused;
  GListModel *controllers = gtk_widget_observe_controllers(canvas->native);
  GtkGestureClick *gesture = NULL;
  for (guint i = 0; i < g_list_model_get_n_items(controllers); ++i) {
    GObject *c = g_list_model_get_item(controllers, i);
    if (GTK_IS_GESTURE_CLICK(c))
      gesture = GTK_GESTURE_CLICK(g_object_ref(c));
    g_object_unref(c);
  }
  assert(gesture);
  GtkFixed *regions = g_object_get_data(G_OBJECT(canvas->native), "regions");
  GtkWidget *button = gtk_widget_get_first_child(GTK_WIDGET(regions));
  assert(button && gtk_widget_grab_focus(button));
  assert(cui__canvas_state(canvas)->focus == 42);
  assert(cui__canvas_key(canvas, 0, 1) && activated == 2);
  double x = gtk_widget_get_width(canvas->native) / 2.,
         y = gtk_widget_get_height(canvas->native) / 2.;
  g_signal_emit_by_name(gesture, "pressed", 1, x, y);
  g_signal_emit_by_name(gesture, "released", 1, x, y);
  assert(activated == 3);
  assert(fabs(event_x - 8) < 0.01);
  g_object_unref(gesture);
  g_object_unref(controllers);
  cui_app_quit(app);
}
int main(void) {
  app = cui_app_create();
  assert(app);
  raster();
  incremental();
  cui_window *window = cui_window_create(app, "Drawing contracts", 400, 300);
  /* Both creation orders exercise popover ownership during app destruction. */
  cui_window *panel = cui_window_create(app, "Attached panel", 100, 100);
  cui_window *later_parent = cui_window_create(app, "Later parent", 200, 200);
  assert(!cui_window_set_anchor(panel, window, 0, 0, 20, 20));
  assert(cui_window_set_frame(panel, 0, 0, 12));
  assert(cui_window_set_frame(later_parent, 0, 0, 12));
  assert(!cui_window_set_anchor(panel, window, -1, 0, 20, 20));
  assert(!cui_window_set_anchor(panel, panel, 0, 0, 20, 20));
  assert(!cui_window_set_anchor(panel, window, 0, 0, 0, 20));
  assert(cui_window_set_anchor(panel, window, 0, 0, 20, 20));
  assert(!cui_window_is_visible(panel));
  assert(cui_window_set_anchor(panel, later_parent, 0, 0, 20, 20));
  assert(!cui_window_set_position(panel, 0, 0));
  assert(!cui_window_begin_move(panel));
  assert(!cui_window_begin_resize(panel, 0));
  assert(!cui_window_set_anchor(later_parent, panel, 0, 0, 20, 20));
  cui_app_set_theme(app, CUI_THEME_LIGHT);
  assert(!gtk_widget_has_css_class(panel->attached_native, "cui-dark"));
  cui_app_set_theme(app, CUI_THEME_DARK);
  assert(gtk_widget_has_css_class(panel->attached_native, "cui-dark"));
  assert(!cui_window_is_visible(window));
  int window_width=0,window_height=0;
  assert(cui_window_get_size(window,&window_width,&window_height));
  assert(window_width==400 && window_height==300);
  assert(!cui_window_get_size(window,NULL,&window_height));
  assert(!cui_window_begin_resize(window,-1));
  assert(!cui_window_begin_resize(window,4));
  assert(!cui_window_begin_resize(window,3));
  assert(!cui_window_set_frame(window, 0, 0, NAN));
  assert(!cui_window_set_frame(window, 0, 0, -1));
  assert(!cui_window_set_size(window, 0, 200));
  assert(!cui_window_set_size(window, 200, 4097));
  assert(window->width == 400 && window->height == 300);
  assert(!cui_window_set_position(window, 32768, 0));
  assert(!cui_window_begin_move(window));
  assert(cui_window_set_frame(window, 0, 0, 24));
  assert(!gtk_window_get_decorated(GTK_WINDOW(window->native)));
  assert(!gtk_window_get_resizable(GTK_WINDOW(window->native)));
  assert(cui_window_set_size(window, 420, 320));
  assert(window->width == 420 && window->height == 320);
  assert(cui_window_set_frame(window, 1, 1, 0));
  assert(gtk_window_get_decorated(GTK_WINDOW(window->native)));
  assert(gtk_window_get_resizable(GTK_WINDOW(window->native)));
  cui_widget *root = cui_window_root(window);
  canvas = cui_canvas(root);
  assert(canvas);
  cui_expand(canvas, 1);
  cui_surface *s = cui_surface_create(16, 16, 2);
  assert(s);
  assert(cui_canvas_set_surface(canvas, s));
  assert(s->refs == 2);
  assert(cui_canvas_set_surface(canvas, s) && s->refs == 2);
  cui_surface_release(s);
  cui_canvas_region region = {42, 0, 0, 16, 16, "Activate map", 1};
  assert(cui_canvas_set_regions(canvas, &region, 1));
  GtkWidget *fixed = g_object_get_data(G_OBJECT(canvas->native), "regions");
  GtkWidget *identity = gtk_widget_get_first_child(fixed);
  region.enabled = 0;
  region.label = "Updated name";
  assert(cui_canvas_set_regions(canvas, &region, 1));
  assert(gtk_widget_get_first_child(fixed) == identity);
  assert(!gtk_widget_get_sensitive(identity));
  assert(!strcmp(gtk_button_get_label(GTK_BUTTON(identity)), "Updated name"));
  region.enabled = 1;
  assert(cui_canvas_set_regions(canvas, &region, 1));
  cui_canvas_region pair[] = {region, {43, 1, 1, 2, 2, "Second", 1}};
  assert(cui_canvas_set_regions(canvas, pair, 2));
  GtkWidget *second = gtk_widget_get_last_child(fixed);
  pair[0] = pair[1];
  pair[1] = region;
  assert(cui_canvas_set_regions(canvas, pair, 2));
  assert(gtk_widget_get_first_child(fixed) == second &&
         gtk_widget_get_last_child(fixed) == identity);
  assert(cui_canvas_set_regions(canvas, &region, 1));
  /* GTK can retain a previously presented frame after a surface is rendered
   * again or destroyed. Its texture must remain immutable. */
  GdkTexture *old_texture = GDK_TEXTURE(
      g_object_ref(gtk_picture_get_paintable(GTK_PICTURE(canvas->aux))));
  cui_draw_command red = {.op = CUI_DRAW_CLEAR, .color = 0xff0000ff};
  s = cui__canvas_state(canvas)->surface;
  assert(cui_surface_render(s, &red, 1));
  unsigned char downloaded[32 * 32 * 4];
  gdk_texture_download(old_texture, downloaded, 32 * 4);
  for (size_t i = 0; i < sizeof(downloaded); ++i)
    assert(!downloaded[i]);
  assert(cui_canvas_set_surface(canvas, s));
  g_object_unref(old_texture);
  /* A rendered frame must not become the canvas preferred layout size. */
  int minimum, natural;
  cui_set_min_size(canvas,100,70);
  gtk_widget_measure(GTK_WIDGET(canvas->native),GTK_ORIENTATION_HORIZONTAL,-1,&minimum,&natural,NULL,NULL);
  assert(minimum==100 && natural==100);
  gtk_widget_measure(GTK_WIDGET(canvas->native),GTK_ORIENTATION_VERTICAL,-1,&minimum,&natural,NULL,NULL);
  assert(minimum==70 && natural==70);
  cui_canvas_on_event(canvas, event, NULL);
  assert(cui_canvas_activate_region(canvas, 42) && activated == 1);
  cui_set_enabled(root, 0);
  assert(!cui_canvas_activate_region(canvas, 42));
  cui_set_enabled(root, 1);
  remove_on_release = 1;
  cui__canvas_event(canvas, CUI_CANVAS_PRESS, 8, 8, 0, 0, 0);
  cui__canvas_event(canvas, CUI_CANVAS_RELEASE, 8, 8, 0, 0, 0);
  assert(activated == 1);
  remove_on_release = 0;
  assert(cui_canvas_set_regions(canvas, &region, 1));
  cui_canvas_region duplicate[] = {region, region};
  assert(!cui_canvas_set_regions(canvas, duplicate, 2));
  assert(cui_canvas_focus_region(canvas, 42));
  assert(cui_set_opacity(canvas, .5) && cui_get_opacity(canvas) == .5);
  assert(!cui_set_opacity(canvas, NAN));
  assert(cui_set_opacity(canvas, 1));
  size_t live = cui_test_live_allocations();
  for (int i = 0; i < 100; ++i) {
    s = cui_surface_create(16, 16, 1);
    assert(s);
    assert(cui_canvas_set_surface(canvas, s));
    cui_surface_release(s);
    assert(cui_test_live_allocations() == live);
  }
  cui_window_show(window);
  assert(cui_window_set_anchor(panel, window, 0, 0, 20, 20));
  cui_window_show(panel);
  gtk_popover_popdown(GTK_POPOVER(panel->attached_native));
  assert(!cui_window_is_visible(panel));
  assert(cui_window_set_anchor(panel, later_parent, 0, 0, 20, 20));
  cui_every(app, 100, native, NULL);
  cui_app_run(app);
  cui_app_destroy(app);
  assert(!cui_test_live_allocations());
  puts("Drawing: composition, clipping, blur, native input, lifetime and "
       "allocation failures passed");
}
