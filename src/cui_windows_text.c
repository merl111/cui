#include "cui_desktop.h"
#include "cui_draw_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
wchar_t *cui__win32_wide(const char *text);
unsigned char *cui__draw_text(const char *text, const char *family, double size,
                              int weight, int max_width, int *width,
                              int *height) {
  wchar_t *value = cui__win32_wide(text),
          *face = cui__win32_wide(family ? family : "Segoe UI");
  if (!value || !face) {
    free(value);
    free(face);
    return NULL;
  }
  HDC dc = CreateCompatibleDC(NULL);
  if (!dc) {
    free(value);
    free(face);
    return NULL;
  }
  HFONT font =
      CreateFontW(-(int)ceil(size), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                  DEFAULT_PITCH, face);
  free(face);
  if (!font) {
    DeleteDC(dc);
    free(value);
    return NULL;
  }
  HGDIOBJ old_font = SelectObject(dc, font);
  RECT measure = {0, 0, max_width, 0};
  DrawTextW(dc, value, -1, &measure, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
  *width = max(1, min(max_width, measure.right));
  *height = max(1, min(1024, measure.bottom + 3));
  BITMAPINFO info = {0};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = *width;
  info.bmiHeader.biHeight = -*height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  unsigned char *pixels = NULL;
  HBITMAP bitmap =
      CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
  unsigned char *result = NULL;
  if (bitmap && pixels) {
    HGDIOBJ old = SelectObject(dc, bitmap);
    memset(pixels, 0, (size_t)*width * *height * 4);
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkMode(dc, TRANSPARENT);
    RECT r = {0, 0, *width, *height};
    DrawTextW(dc, value, -1, &r, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    GdiFlush();
    result = malloc((size_t)*width * *height);
    if (result)
      for (size_t i = 0; i < (size_t)*width * *height; ++i)
        result[i] = pixels[i * 4];
    SelectObject(dc, old);
    DeleteObject(bitmap);
  }
  SelectObject(dc, old_font);
  DeleteObject(font);
  DeleteDC(dc);
  free(value);
  return result;
}
/* GDI's coverage renderer remains the fallback on Windows. Color glyphs need
 * a future DirectWrite path; never interpret a monochrome bitmap as RGBA. */
uint32_t *cui__draw_text_color(const char *text,const char *family,double size,int weight,int max_width,int *width,int *height,unsigned color) {
  (void)text;(void)family;(void)size;(void)weight;(void)max_width;(void)width;(void)height;(void)color;return NULL;
}
