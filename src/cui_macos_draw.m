#include "cui_desktop.h"
#include "cui_draw_internal.h"
#import <AppKit/AppKit.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
void cui__mac_draw_icon_asset(cui_icon_asset *, NSRect, BOOL);
static NSBitmapImageRep *bitmap(int w, int h) {
  return [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:NULL
                    pixelsWide:w
                    pixelsHigh:h
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSDeviceRGBColorSpace
                  bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
                   bytesPerRow:w * 4
                  bitsPerPixel:32];
}
unsigned char *cui__draw_text(const char *text, const char *family, double size,
                              int weight, int max_width, int *width,
                              int *height) {
  @autoreleasepool {
    NSString *value = [NSString stringWithUTF8String:text];
    if (!value)
      return NULL;
    NSFont *font =
        family ? [NSFont fontWithName:[NSString stringWithUTF8String:family]
                                 size:size]
               : nil;
    if (!font)
      font = [NSFont systemFontOfSize:size
                               weight:weight >= 600 ? NSFontWeightSemibold
                                                    : NSFontWeightRegular];
    NSMutableParagraphStyle *paragraph =
        [[[NSMutableParagraphStyle alloc] init] autorelease];
    [paragraph setLineBreakMode:NSLineBreakByTruncatingTail];
    NSDictionary *attrs = @{
      NSFontAttributeName : font,
      NSForegroundColorAttributeName : [NSColor whiteColor],
      NSParagraphStyleAttributeName : paragraph
    };
    NSSize measured = [value sizeWithAttributes:attrs];
    *width = MAX(1, MIN(max_width, (int)ceil(measured.width)));
    *height = MAX(1, MIN(1024, (int)ceil(measured.height + 3)));
    NSBitmapImageRep *rep = bitmap(*width, *height);
    if (!rep)
      return NULL;
    [NSGraphicsContext saveGraphicsState];
    NSGraphicsContext *ctx =
        [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    [NSGraphicsContext setCurrentContext:ctx];
    NSAffineTransform *flip = [NSAffineTransform transform];
    [flip translateXBy:0 yBy:*height];
    [flip scaleXBy:1 yBy:-1];
    [flip concat];
    [value drawInRect:NSMakeRect(0, 0, *width, *height) withAttributes:attrs];
    [NSGraphicsContext restoreGraphicsState];
    unsigned char *result = malloc((size_t)*width * *height);
    if (result) {
      unsigned char *pixels = [rep bitmapData];
      for (size_t i = 0; i < (size_t)*width * *height; ++i)
        result[i] = pixels[i * 4 + 3];
    }
    [rep release];
    return result;
  }
}
uint32_t *cui__draw_asset(const cui_icon_asset *source, int width, int height,
                          unsigned color) {
  @autoreleasepool {
    cui_icon_asset a = *source;
    cui_icon_command *commands = NULL;
    if (a.count) {
      commands = malloc(a.count * sizeof(*commands));
      if (!commands)
        return NULL;
      memcpy(commands, a.commands, a.count * sizeof(*commands));
      for (size_t i = 0; i < a.count; ++i)
        if (commands[i].current_color) {
          commands[i].current_color = 0;
          commands[i].rgba = color;
        }
      a.commands = commands;
    }
    NSBitmapImageRep *rep = bitmap(width, height);
    if (!rep) {
      free(commands);
      return NULL;
    }
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext
        setCurrentContext:[NSGraphicsContext
                              graphicsContextWithBitmapImageRep:rep]];
    NSAffineTransform *flip = [NSAffineTransform transform];
    [flip translateXBy:0 yBy:height];
    [flip scaleXBy:1 yBy:-1];
    [flip concat];
    cui__mac_draw_icon_asset(&a, NSMakeRect(0, 0, width, height), YES);
    [NSGraphicsContext restoreGraphicsState];
    uint32_t *out = malloc((size_t)width * height * 4);
    if (out) {
      const unsigned char *v = [rep bitmapData];
      for (size_t i = 0; i < (size_t)width * height; ++i) {
        const unsigned char *p = v + i * 4;
        out[i] = (unsigned)p[3] << 24 | ((unsigned)p[0] * p[3] / 255) << 16 |
                 ((unsigned)p[1] * p[3] / 255) << 8 |
                 ((unsigned)p[2] * p[3] / 255);
      }
    }
    [rep release];
    free(commands);
    return out;
  }
}
static unsigned modifiers(NSEvent *event) {
  NSEventModifierFlags f = [event modifierFlags];
  unsigned m = 0;
  if (f & NSEventModifierFlagShift)
    m |= CUI_MOD_SHIFT;
  if (f & NSEventModifierFlagOption)
    m |= CUI_MOD_ALT;
  if (f & NSEventModifierFlagControl)
    m |= CUI_MOD_CONTROL;
  if (f & NSEventModifierFlagCommand)
    m |= CUI_MOD_PRIMARY;
  return m;
}
void cui__mac_canvas_event(cui_widget *w, NSView *view, NSEvent *event,
                           cui_canvas_event_kind kind) {
  cui_canvas_state *s = cui__canvas_state(w);
  if (!s || !s->surface)
    return;
  NSPoint point = [view convertPoint:[event locationInWindow] fromView:nil];
  NSSize size = [view bounds].size;
  double sw = s->surface->width / s->surface->scale,
         sh = s->surface->height / s->surface->scale,
         k = MIN(size.width / sw, size.height / sh);
  if (k <= 0)
    return;
  if (kind == CUI_CANVAS_PRESS)
    [[view window] makeFirstResponder:view];
  cui__canvas_event(w, kind, (point.x - (size.width - sw * k) / 2) / k,
                    (point.y - (size.height - sh * k) / 2) / k,
                    kind == CUI_CANVAS_SCROLL ? [event scrollingDeltaX] : 0,
                    kind == CUI_CANVAS_SCROLL ? [event scrollingDeltaY] : 0,
                    modifiers(event));
}
@interface CUICanvasRegion : NSAccessibilityElement {
@public
  cui_widget *model;
  unsigned identifier;
}
@end
@implementation CUICanvasRegion
- (BOOL)accessibilityPerformPress {
  return model && cui_canvas_activate_region(model, identifier);
}
@end
void cui__mac_canvas_detach(NSView *view) {
  for (CUICanvasRegion *item in [view accessibilityChildren])
    if ([item isKindOfClass:[CUICanvasRegion class]])
      item->model = NULL;
  [view setAccessibilityChildren:nil];
}
int cui__canvas_attach(cui_widget *w) {
  [(NSView *)w->native setAccessibilityElement:YES];
  [(NSView *)w->native setAccessibilityRole:NSAccessibilityGroupRole];
  return 1;
}
void cui__canvas_regions(cui_widget *w) {
  cui_canvas_state *s = cui__canvas_state(w);
  NSView *view = w->native;
  NSArray *old = [view accessibilityChildren];
  NSMutableArray *items = [NSMutableArray array];
  if (!s->surface)
    return;
  NSSize size = [view bounds].size;
  double sw = s->surface->width / s->surface->scale,
         sh = s->surface->height / s->surface->scale,
         k = MIN(size.width / sw, size.height / sh);
  for (size_t i = 0; i < s->count; ++i) {
    cui_canvas_region *r = s->regions + i;
    CUICanvasRegion *item = nil;
    for (CUICanvasRegion *candidate in old)
      if ([candidate isKindOfClass:[CUICanvasRegion class]] &&
          candidate->identifier == r->id) {
        item = candidate;
        break;
      }
    if (!item)
      item = [[[CUICanvasRegion alloc] init] autorelease];
    item->model = w;
    item->identifier = r->id;
    [item setAccessibilityParent:view];
    [item setAccessibilityRole:NSAccessibilityButtonRole];
    [item setAccessibilityLabel:[NSString stringWithUTF8String:r->label]];
    [item setAccessibilityEnabled:r->enabled];
    NSRect rect = NSMakeRect((size.width - sw * k) / 2 + r->x * k,
                             (size.height - sh * k) / 2 + r->y * k,
                             r->width * k, r->height * k);
    [item
        setAccessibilityFrame:[[view window]
                                  convertRectToScreen:[view convertRect:rect
                                                                 toView:nil]]];
    [items addObject:item];
  }
  for (CUICanvasRegion *item in old)
    if ([item isKindOfClass:[CUICanvasRegion class]] &&
        ![items containsObject:item])
      item->model = NULL;
  [view setAccessibilityChildren:items];
}
int cui__native_opacity(cui_widget *w, double value) {
  if (cui__container(w) && value != 1)
    return 0;
  [(NSView *)w->native setAlphaValue:value];
  return 1;
}

int cui__canvas_present(cui_widget *w, cui_surface *s, double opacity) {
  (void)w;
  (void)s;
  (void)opacity;
  return 0; /* These backends consume the adopted straight-RGBA buffer. */
}
