#include "cui_desktop.h"
#include "cui_draw_internal.h"
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/extensions/shape.h>
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
void cui__gtk_icon_paint(cairo_t *, const cui_icon_asset *, const GdkRGBA *);
static unsigned char *text_bitmap(const char *text, const char *family, double size,
                              int weight, int max_width, int *width,
                              int *height, unsigned color, int colored) {
  cairo_surface_t *probe = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1);
  cairo_t *cr = cairo_create(probe);
  PangoLayout *layout = pango_cairo_create_layout(cr);
  PangoFontDescription *font = pango_font_description_new();
  pango_font_description_set_family(font, family && *family ? family : "sans");
  pango_font_description_set_absolute_size(font, size * PANGO_SCALE);
  pango_font_description_set_weight(font, (PangoWeight)weight);
  pango_layout_set_font_description(layout, font);
  pango_font_description_free(font);
  pango_layout_set_text(layout, text, -1);
  pango_layout_set_width(layout, max_width * PANGO_SCALE);
  pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
  pango_layout_set_single_paragraph_mode(layout, TRUE);
  pango_layout_get_pixel_size(layout, width, height);
  *width = MAX(1, MIN(*width, max_width));
  *height = MAX(1, MIN(*height, 1024));
  cairo_destroy(cr);
  cairo_surface_destroy(probe);
  cairo_surface_t *bitmap =
      cairo_image_surface_create(colored ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_A8, *width, *height);
  cr = cairo_create(bitmap);
  cairo_set_source_rgba(cr,(color>>24)/255.,((color>>16)&255)/255.,((color>>8)&255)/255.,(color&255)/255.);
  pango_cairo_show_layout(cr, layout);
  g_object_unref(layout);
  cairo_destroy(cr);
  cairo_surface_flush(bitmap);
  unsigned char *result = NULL;
  if (cairo_surface_status(bitmap) == CAIRO_STATUS_SUCCESS) {
    result = malloc((size_t)*width * *height * (colored ? 4 : 1));
    if (result) {
      unsigned char *src = cairo_image_surface_get_data(bitmap);
      size_t row=(size_t)*width*(colored?4:1);
      int stride = cairo_image_surface_get_stride(bitmap);
      for (int y = 0; y < *height; ++y)
        memcpy(result + (size_t)y * row, src + (size_t)y * stride, row);
    }
  }
  cairo_surface_destroy(bitmap);
  return result;
}
unsigned char *cui__draw_text(const char *text,const char *family,double size,int weight,int max_width,int *width,int *height) {
  return text_bitmap(text,family,size,weight,max_width,width,height,0xffffffffu,0);
}
uint32_t *cui__draw_text_color(const char *text,const char *family,double size,int weight,int max_width,int *width,int *height,unsigned color) {
  return (uint32_t*)text_bitmap(text,family,size,weight,max_width,width,height,color,1);
}
uint32_t *cui__draw_asset(const cui_icon_asset *a, int width, int height,
                          unsigned color) {
  uint32_t *pixels = calloc((size_t)width * height, 4);
  if (!pixels)
    return NULL;
  cairo_surface_t *surface = cairo_image_surface_create_for_data(
      (unsigned char *)pixels, CAIRO_FORMAT_ARGB32, width, height, width * 4);
  cairo_t *cr = cairo_create(surface);
  double fit = fmin(width / a->width, height / a->height);
  cairo_translate(cr, (width - a->width * fit) / 2,
                  (height - a->height * fit) / 2);
  cairo_scale(cr, fit, fit);
  if (a->pixels) {
    size_t n = (size_t)a->width * (size_t)a->height;
    uint32_t *copy = malloc(n * 4);
    if (!copy) {
      cairo_destroy(cr);
      cairo_surface_destroy(surface);
      free(pixels);
      return NULL;
    }
    for (size_t i = 0; i < n; ++i) {
      const unsigned char *v = a->pixels + i * 4;
      copy[i] = (unsigned)v[3] << 24 | ((unsigned)v[0] * v[3] / 255) << 16 |
                ((unsigned)v[1] * v[3] / 255) << 8 |
                ((unsigned)v[2] * v[3] / 255);
    }
    cairo_surface_t *image = cairo_image_surface_create_for_data(
        (unsigned char *)copy, CAIRO_FORMAT_ARGB32, (int)a->width,
        (int)a->height, (int)a->width * 4);
    cairo_set_source_surface(cr, image, 0, 0);
    cairo_paint(cr);
    cairo_surface_destroy(image);
    free(copy);
  } else {
    GdkRGBA tint = {(color >> 24) / 255.f, ((color >> 16) & 255) / 255.f,
                    ((color >> 8) & 255) / 255.f, (color & 255) / 255.f};
    cui__gtk_icon_paint(cr, a, &tint);
  }
  int okay = cairo_status(cr) == CAIRO_STATUS_SUCCESS;
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  if (!okay) {
    free(pixels);
    return NULL;
  }
  return pixels;
}
/* A sole canvas in an undecorated, unpadded window supplies its silhouette.
 * Compositors retain fractional alpha for smooth edges; bare X11 gets a binary
 * fallback. Input follows nontransparent pixels, including separate toolbars. */
static void panel_input_shape(GdkFrameClock *clock, GtkWidget *canvas) {
  (void)clock;
  cairo_region_t *region=g_object_get_data(G_OBJECT(canvas),"cui-window-shape");
  GtkNative *host=gtk_widget_get_native(canvas);
  GdkSurface *surface=host ? gtk_native_get_surface(host) : NULL;
  if(region && surface)gdk_surface_set_input_region(surface,region);
}
static cairo_region_t *surface_input_region(cui_surface *s, int width, int height) {
  double fit=fmin((double)width/s->width,(double)height/s->height);
  double ox=(width-s->width*fit)/2, oy=(height-s->height*fit)/2;
  cairo_region_t *region=cairo_region_create();
  for(int y=0;y<height;++y) {
    int sy=(int)floor((y+.5-oy)/fit);
    if(sy<0 || sy>=s->height)continue;
    int start=-1;
    for(int x=0;x<=width;++x) {
      int sx=(int)floor((x+.5-ox)/fit);
      int visible=x<width && sx>=0 && sx<s->width && (s->pixels[(size_t)sy*s->width+sx]>>24)>0;
      if(visible && start<0)start=x;
      if(!visible && start>=0) {
        cairo_rectangle_int_t row={start,y,x-start,1};
        cairo_region_union_rectangle(region,&row);start=-1;
      }
    }
  }
  return region;
}

static void canvas_bounding_shape(GdkSurface *native, cairo_region_t *region) {
#ifdef GDK_WINDOWING_X11
  if(GDK_IS_X11_SURFACE(native)) {
    Display *display=gdk_x11_display_get_xdisplay(gdk_surface_get_display(native));
    Window xid=gdk_x11_surface_get_xid(native);
    if(gdk_display_is_composited(gdk_surface_get_display(native)))
      XShapeCombineMask(display,xid,ShapeBounding,0,0,None,ShapeSet);
    else {
      Region shape=XCreateRegion();
      if(shape) {
        int scale=gdk_surface_get_scale_factor(native);
        for(int i=0;i<cairo_region_num_rectangles(region);++i) {
          cairo_rectangle_int_t r;cairo_region_get_rectangle(region,i,&r);
          XRectangle rect={(short)(r.x*scale),(short)(r.y*scale),
                           (unsigned short)(r.width*scale),(unsigned short)(r.height*scale)};
          XUnionRectWithRegion(&rect,shape,shape);
        }
        XShapeCombineRegion(display,xid,ShapeBounding,0,0,shape,ShapeSet);
        XDestroyRegion(shape);
      }
    }
  }
#else
  (void)native; (void)region;
#endif
}

static void canvas_window_shape(cui_widget *w, cui_surface *s) {
  cui_window *window = w->window;
  if (window->decorated || !s ||
      w->parent != window->root || window->root->padding ||
      window->root->first != w || window->root->last != w) return;
  GtkNative *host=gtk_widget_get_native(w->native);
  GdkSurface *native = host ? gtk_native_get_surface(host) : NULL;
  if (!native) return;
  /* GtkPopover updates its input shape after allocating its child. Restore
   * the cached canvas silhouette after paint, with no pixel rescan. The
   * object-bound signal disconnects automatically when the canvas dies. */
  if(window->attached_native && !g_object_get_data(G_OBJECT(w->native),"cui-panel-shape-clock")) {
    GdkFrameClock *clock=gtk_widget_get_frame_clock(w->native);
    if(clock) {
      g_signal_connect_object(clock,"after-paint",G_CALLBACK(panel_input_shape),w->native,0);
      g_object_set_data(G_OBJECT(w->native),"cui-panel-shape-clock",GINT_TO_POINTER(1));
    }
  }
  int width=gtk_widget_get_width(w->native), height=gtk_widget_get_height(w->native);
  if (width<1 || height<1) return;
  cairo_region_t *region=surface_input_region(s,width,height);
  cairo_region_t *old=g_object_get_data(G_OBJECT(w->native),"cui-window-shape");
  if(old && g_object_get_data(G_OBJECT(w->native),"cui-shape-surface")==native &&
     cairo_region_equal(old,region)) { cairo_region_destroy(region);return; }
  gdk_surface_set_input_region(native,region);
  canvas_bounding_shape(native,region);
  g_object_set_data_full(G_OBJECT(w->native),"cui-window-shape",region,(GDestroyNotify)cairo_region_destroy);
  g_object_set_data_full(G_OBJECT(w->native),"cui-shape-surface",g_object_ref(native),g_object_unref);
}
static void position(cui_widget *w, double *x, double *y) {
  cui_canvas_state *s = cui__canvas_state(w);
  if (!s || !s->surface)
    return;
  double width = gtk_widget_get_width(w->native),
         height = gtk_widget_get_height(w->native);
  double sw = s->surface->width / s->surface->scale,
         sh = s->surface->height / s->surface->scale,
         scale = MIN(width / sw, height / sh);
  if (scale <= 0)
    return;
  *x = (*x - (width - sw * scale) / 2) / scale;
  *y = (*y - (height - sh * scale) / 2) / scale;
}
static unsigned modifiers(GtkEventController *c) {
  GdkModifierType f = gtk_event_controller_get_current_event_state(c);
  unsigned m = 0;
  if (f & GDK_SHIFT_MASK)
    m |= CUI_MOD_SHIFT;
  if (f & GDK_ALT_MASK)
    m |= CUI_MOD_ALT;
  if (f & GDK_CONTROL_MASK)
    m |= CUI_MOD_CONTROL | CUI_MOD_PRIMARY;
  return m;
}
static void press(GtkGestureClick *gesture, int n, double x, double y,
                  gpointer data) {
  (void)gesture;
  (void)n;
  cui_widget *w = data;
  gtk_widget_grab_focus(w->native);
  position(w, &x, &y);
  w->window->pointer_event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(gesture));
  cui__canvas_event(w, CUI_CANVAS_PRESS, x, y, 0, 0,
                    modifiers(GTK_EVENT_CONTROLLER(gesture)));
  w->window->pointer_event = NULL;
}
static void release(GtkGestureClick *gesture, int n, double x, double y,
                    gpointer data) {
  (void)gesture;
  (void)n;
  position(data, &x, &y);
  cui__canvas_event(data, CUI_CANVAS_RELEASE, x, y, 0, 0,
                    modifiers(GTK_EVENT_CONTROLLER(gesture)));
}
static void context_press(GtkGestureClick *gesture, int n, double x, double y,
                          gpointer data) {
  (void)n;
  position(data, &x, &y);
  cui__canvas_event(data, CUI_CANVAS_CONTEXT, x, y, 0, 0,
                    modifiers(GTK_EVENT_CONTROLLER(gesture)));
  gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}
static void motion(GtkEventControllerMotion *controller, double x, double y,
                   gpointer data) {
  (void)controller;
  position(data, &x, &y);
  cui__canvas_event(data, CUI_CANVAS_MOVE, x, y, 0, 0,
                    modifiers(GTK_EVENT_CONTROLLER(controller)));
}
static void leave(GtkEventControllerMotion *controller, gpointer data) {
  cui__canvas_event(data, CUI_CANVAS_MOVE, -1, -1, 0, 0,
                    modifiers(GTK_EVENT_CONTROLLER(controller)));
}
static gboolean scroll(GtkEventControllerScroll *controller, double dx,
                       double dy, gpointer data) {
  GdkEvent *event =
      gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
  double x = 0, y = 0;
  if (event) {
    /* Discrete wheel events can have no position (NaN on GTK/X11).
     * Recover it from the device instead of discarding otherwise valid input.
     */
    if (!gdk_event_get_position(event, &x, &y) || !isfinite(x) ||
        !isfinite(y)) {
      GdkSurface *surface = gdk_event_get_surface(event);
      GdkSeat *seat =
          surface
              ? gdk_display_get_default_seat(gdk_surface_get_display(surface))
              : NULL;
      /* The event device may be a scroll-only source, which cannot answer
       * pointer queries on X11. Query the seat's logical pointer instead. */
      GdkDevice *device = seat ? gdk_seat_get_pointer(seat) : NULL;
      if (!surface || !device ||
          !gdk_surface_get_device_position(surface, device, &x, &y, NULL) ||
          !isfinite(x) || !isfinite(y))
        x = y = 0;
    }
    GtkWidget *w = GTK_WIDGET(((cui_widget *)data)->native);
    graphene_point_t in = GRAPHENE_POINT_INIT((float)x, (float)y), out;
    GtkRoot *root = gtk_widget_get_root(w);
    if (root && gtk_widget_compute_point(GTK_WIDGET(root), w, &in, &out)) {
      x = out.x;
      y = out.y;
    }
  }
  position(data, &x, &y);
  cui__canvas_event(data, CUI_CANVAS_SCROLL, x, y, dx, dy,
                    modifiers(GTK_EVENT_CONTROLLER(controller)));
  return TRUE;
}
static gboolean key(GtkEventControllerKey *controller, guint keyval, guint code,
                    GdkModifierType mods, gpointer data) {
  (void)controller;
  (void)code;
  (void)
      mods; /* Native region buttons provide Tab traversal and focus escape. */
  if ((keyval == GDK_KEY_c || keyval == GDK_KEY_C) && (mods & GDK_CONTROL_MASK))
    return cui__canvas_copy(data);
  if (keyval == GDK_KEY_Return || keyval == GDK_KEY_space)
    return cui__canvas_key(data, 0, 1);
  return FALSE;
}
static void region_positions(GtkDrawingArea *, int, int, gpointer);
int cui__canvas_attach(cui_widget *w) {
  g_signal_connect(g_object_get_data(G_OBJECT(w->native), "resize"), "resize",
                   G_CALLBACK(region_positions), w);
  gtk_widget_set_focusable(w->native, TRUE);
  GtkGesture *context = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(context),
                                GDK_BUTTON_SECONDARY);
  gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(context),
                                             GTK_PHASE_CAPTURE);
  g_signal_connect(context, "pressed", G_CALLBACK(context_press), w);
  gtk_widget_add_controller(w->native, GTK_EVENT_CONTROLLER(context));
  GtkGesture *click = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
  g_signal_connect(click, "pressed", G_CALLBACK(press), w);
  g_signal_connect(click, "released", G_CALLBACK(release), w);
  gtk_widget_add_controller(w->native, GTK_EVENT_CONTROLLER(click));
  GtkEventController *move = gtk_event_controller_motion_new();
  g_signal_connect(move, "motion", G_CALLBACK(motion), w);
  g_signal_connect(move, "leave", G_CALLBACK(leave), w);
  gtk_widget_add_controller(w->native, move);
  GtkEventController *wheel =
      gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
  gtk_event_controller_set_propagation_phase(wheel, GTK_PHASE_CAPTURE);
  g_signal_connect(wheel, "scroll", G_CALLBACK(scroll), w);
  gtk_widget_add_controller(w->native, wheel);
  GtkEventController *keys = gtk_event_controller_key_new();
  g_signal_connect(keys, "key-pressed", G_CALLBACK(key), w);
  gtk_widget_add_controller(w->native, keys);
  return 1;
}
static void region_click(GtkButton *button, gpointer data) {
  cui_widget *w = data;
  unsigned id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "id"));
  if (w->updating)
    return;
  cui__canvas_focus(w, id);
  cui_canvas_activate_region(w, id);
}
static void region_focus(GObject *button, GParamSpec *spec, gpointer data) {
  (void)spec;
  if (!((cui_widget *)data)->updating &&
      gtk_widget_has_focus(GTK_WIDGET(button)))
    cui__canvas_focus(data, GPOINTER_TO_UINT(g_object_get_data(button, "id")));
}
static void region_positions(GtkDrawingArea *area, int width, int height,
                             gpointer data) {
  (void)area;
  cui_widget *w = data;
  cui_canvas_state *s = cui__canvas_state(w);
  /* Present attached popovers during parent allocation, before committing the
   * resized Wayland surface. A later timer is too late for popup constraints. */
  for(cui_window *panel=w->window->app->windows;panel;panel=panel->next)
    if(panel->anchor_parent==w->window)cui__backend_window_anchor(panel);
  canvas_window_shape(w, s ? s->surface : NULL);
  if (!s || !s->surface)
    return;
  double sw = s->surface->width / s->surface->scale,
         sh = s->surface->height / s->surface->scale,
         k = MIN(width / sw, height / sh);
  GtkFixed *fixed = g_object_get_data(G_OBJECT(w->native), "regions");
  GtkWidget *button = gtk_widget_get_first_child(GTK_WIDGET(fixed));
  for (size_t i = 0; i < s->count && button; ++i) {
    cui_canvas_region *r = s->regions + i;
    gtk_fixed_move(fixed, button, (width - sw * k) / 2 + r->x * k,
                   (height - sh * k) / 2 + r->y * k);
    gtk_widget_set_size_request(button, MAX(1, (int)(r->width * k)),
                                MAX(1, (int)(r->height * k)));
    button = gtk_widget_get_next_sibling(button);
  }
}
GtkWidget *cui__gtk_canvas_new(void) {
  GtkWidget *overlay = gtk_overlay_new(), *image = gtk_picture_new(),
            *fixed = gtk_fixed_new(), *resize = gtk_drawing_area_new();
  gtk_widget_add_css_class(overlay, "cui-canvas");
  gtk_picture_set_can_shrink(GTK_PICTURE(image), TRUE);
  /* Raster dimensions describe the previous frame, never a layout request.
   * A non-measuring overlay prevents feedback when a pane shrinks or grows. */
  gtk_overlay_set_child(GTK_OVERLAY(overlay), resize);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), image);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), fixed);
  gtk_widget_set_can_target(image, FALSE);
  gtk_widget_set_can_target(resize, FALSE);
  gtk_widget_set_can_target(fixed, FALSE);
  gtk_widget_set_size_request(overlay, 260, 160);
  g_object_set_data(G_OBJECT(overlay), "image", image);
  g_object_set_data(G_OBJECT(overlay), "regions", fixed);
  g_object_set_data(G_OBJECT(overlay), "resize", resize);
  return overlay;
}
void cui__canvas_regions(cui_widget *w) {
  cui_canvas_state *s = cui__canvas_state(w);
  GtkFixed *fixed = g_object_get_data(G_OBJECT(w->native), "regions");
  ++w->updating;
  GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(fixed));
  while (child) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    unsigned id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(child), "id"));
    size_t i = 0;
    while (i < s->count && s->regions[i].id != id)
      ++i;
    if (i == s->count || GTK_IS_LABEL(child) != (s->regions[i].role == CUI_CANVAS_TEXT))
      gtk_fixed_remove(fixed, child);
    child = next;
  }
  GtkWidget *previous = NULL;
  for (size_t i = 0; i < s->count; ++i) {
    cui_canvas_region *r = s->regions + i;
    GtkWidget *button = gtk_widget_get_first_child(GTK_WIDGET(fixed));
    while (button &&
           GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "id")) != r->id)
      button = gtk_widget_get_next_sibling(button);
    if (!button) {
      button = r->role == CUI_CANVAS_TEXT ? gtk_label_new(r->label) : gtk_button_new_with_label(r->label);
      if (GTK_IS_LABEL(button)) {
        gtk_label_set_wrap(GTK_LABEL(button), TRUE);
        gtk_label_set_selectable(GTK_LABEL(button), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(button), 1);
      }
      gtk_widget_set_focusable(button, TRUE);
      gtk_widget_set_opacity(button, 0);
      /* Pointer gestures belong to the canvas; these proxies provide keyboard
       * focus and accessibility without intercepting drag/press events. */
      gtk_widget_set_can_target(button, FALSE);
      g_object_set_data(G_OBJECT(button), "id", GUINT_TO_POINTER(r->id));
      if (GTK_IS_BUTTON(button)) g_signal_connect(button, "clicked", G_CALLBACK(region_click), w);
      g_signal_connect(button, "notify::has-focus", G_CALLBACK(region_focus),
                       w);
      gtk_fixed_put(fixed, button, 0, 0);
    } else if (GTK_IS_LABEL(button)) {
      if (strcmp(gtk_label_get_text(GTK_LABEL(button)), r->label)) gtk_label_set_text(GTK_LABEL(button), r->label);
    } else if (strcmp(gtk_button_get_label(GTK_BUTTON(button)), r->label))
      gtk_button_set_label(GTK_BUTTON(button), r->label);
    gtk_widget_set_sensitive(button, r->enabled);
    if (gtk_widget_get_prev_sibling(button) != previous)
      gtk_widget_insert_after(button, GTK_WIDGET(fixed), previous);
    previous = button;
  }
  --w->updating;
  region_positions(NULL, gtk_widget_get_width(w->native),
                   gtk_widget_get_height(w->native), w);
}
int cui__native_opacity(cui_widget *w, double value) {
  gtk_widget_set_opacity(w->native, value);
  return 1;
}

int cui__canvas_present(cui_widget *w, cui_surface *s, double opacity) {
  if (!cui__pixels_retain(s->buffer))
    return 0;
  GBytes *bytes =
      g_bytes_new_with_free_func(s->pixels, (size_t)s->width * s->height * 4,
                                 cui__pixels_release, s->buffer);
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  GdkMemoryFormat format = GDK_MEMORY_B8G8R8A8_PREMULTIPLIED;
#else
  GdkMemoryFormat format = GDK_MEMORY_A8R8G8B8_PREMULTIPLIED;
#endif
  GdkTexture *texture = gdk_memory_texture_new(s->width, s->height, format,
                                               bytes, (size_t)s->width * 4);
  gtk_widget_set_opacity(GTK_WIDGET(w->aux), opacity);
  gtk_picture_set_paintable(GTK_PICTURE(w->aux), GDK_PAINTABLE(texture));
  g_object_unref(texture);
  g_bytes_unref(bytes);
  free(w->pixels);
  w->pixels = NULL;
  w->image_width = s->width;
  w->image_height = s->height;
  canvas_window_shape(w, s);
  return 1;
}
