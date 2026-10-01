#include "cui_desktop.h"
#include "cui_draw_internal.h"
#include <commctrl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>
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
static unsigned modifiers(void) {
  unsigned m = 0;
  if (GetKeyState(VK_SHIFT) & 0x8000)
    m |= CUI_MOD_SHIFT;
  if (GetKeyState(VK_MENU) & 0x8000)
    m |= CUI_MOD_ALT;
  if (GetKeyState(VK_CONTROL) & 0x8000)
    m |= CUI_MOD_CONTROL | CUI_MOD_PRIMARY;
  return m;
}
static void position(cui_widget *w, double *x, double *y) {
  cui_canvas_state *s = cui__canvas_state(w);
  if (!s || !s->surface)
    return;
  RECT r;
  GetClientRect(w->native, &r);
  double sw = s->surface->width / s->surface->scale,
         sh = s->surface->height / s->surface->scale,
         k = fmin(r.right / sw, r.bottom / sh);
  if (k > 0) {
    *x = (*x - (r.right - sw * k) / 2) / k;
    *y = (*y - (r.bottom - sh * k) / 2) / k;
  }
}
static LRESULT CALLBACK region_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                    UINT_PTR id, DWORD_PTR data) {
  (void)id;
  cui_widget *w = (cui_widget *)data;
  if (msg == WM_NCHITTEST)
    return HTTRANSPARENT;
  if (msg == WM_SETFOCUS && !w->updating)
    cui__canvas_focus(w, (unsigned)GetDlgCtrlID(hwnd));
  if (msg == WM_NCDESTROY)
    RemoveWindowSubclass(hwnd, region_proc, 1);
  return DefSubclassProc(hwnd, msg, wp, lp);
}
static LRESULT CALLBACK canvas_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                    UINT_PTR id, DWORD_PTR data) {
  (void)id;
  cui_widget *w = (cui_widget *)data;
  double x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
  cui_canvas_event_kind kind;
  switch (msg) {
  case WM_RBUTTONUP:
    kind = CUI_CANVAS_CONTEXT;
    break;
  case WM_LBUTTONDOWN:
    SetFocus(hwnd);
    SetCapture(hwnd);
    kind = CUI_CANVAS_PRESS;
    break;
  case WM_LBUTTONUP:
    ReleaseCapture();
    kind = CUI_CANVAS_RELEASE;
    break;
  case WM_MOUSEMOVE: {
    TRACKMOUSEEVENT tracking = {sizeof(tracking), TME_LEAVE, hwnd, 0};
    TrackMouseEvent(&tracking);
    kind = CUI_CANVAS_MOVE;
    break;
  }
  case WM_MOUSELEAVE:
    cui__canvas_event(w, CUI_CANVAS_MOVE, -1, -1, 0, 0, modifiers());
    return 0;
  case WM_MOUSEWHEEL: {
    POINT point = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    ScreenToClient(hwnd, &point);
    x = point.x;
    y = point.y;
    position(w, &x, &y);
    cui__canvas_event(w, CUI_CANVAS_SCROLL, x, y, 0,
                      -GET_WHEEL_DELTA_WPARAM(wp) / (double)WHEEL_DELTA,
                      modifiers());
    return 0;
  }
  case WM_GETDLGCODE:
    return DLGC_WANTALLKEYS;
  case WM_KEYDOWN:
    if (wp == VK_TAB) {
      cui__canvas_key(w, (GetKeyState(VK_SHIFT) & 0x8000) != 0, 0);
      return 0;
    }
    if (wp == VK_RETURN || wp == VK_SPACE) {
      cui__canvas_key(w, 0, 1);
      return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
  case WM_COMMAND:
    if (HIWORD(wp) == BN_CLICKED) {
      cui_canvas_activate_region(w, (unsigned)GetDlgCtrlID((HWND)lp));
      return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
  case WM_DRAWITEM:
    return TRUE; /* Accessible native region buttons use the canvas artwork. */
  case WM_SIZE:
    cui__canvas_regions(w);
    return DefSubclassProc(hwnd, msg, wp, lp);
  case WM_NCDESTROY:
    RemoveWindowSubclass(hwnd, canvas_proc, 1);
    return DefSubclassProc(hwnd, msg, wp, lp);
  default:
    return DefSubclassProc(hwnd, msg, wp, lp);
  }
  position(w, &x, &y);
  cui__canvas_event(w, kind, x, y, 0, 0, modifiers());
  return 0;
}
int cui__canvas_attach(cui_widget *w) {
  SetWindowLongPtrW(w->native, GWL_STYLE,
                    GetWindowLongPtrW(w->native, GWL_STYLE) | WS_TABSTOP |
                        SS_NOTIFY);
  return SetWindowSubclass(w->native, canvas_proc, 1, (DWORD_PTR)w) != 0;
}
void cui__canvas_regions(cui_widget *w) {
  cui_canvas_state *s = cui__canvas_state(w);
  if (!s || !s->surface)
    return;
  ++w->updating;
  HWND child = GetWindow(w->native, GW_CHILD);
  while (child) {
    HWND next = GetWindow(child, GW_HWNDNEXT);
    unsigned id = (unsigned)GetDlgCtrlID(child);
    size_t i = 0;
    while (i < s->count && s->regions[i].id != id)
      ++i;
    if (i == s->count)
      DestroyWindow(child);
    child = next;
  }
  RECT rect;
  GetClientRect(w->native, &rect);
  double sw = s->surface->width / s->surface->scale,
         sh = s->surface->height / s->surface->scale,
         k = fmin(rect.right / sw, rect.bottom / sh);
  HWND previous = HWND_TOP;
  for (size_t i = 0; i < s->count; ++i) {
    cui_canvas_region *r = s->regions + i;
    wchar_t *label = cui__win32_wide(r->label);
    if (!label)
      continue;
    HWND button = GetDlgItem(w->native, (int)r->id);
    if (!button) {
      button = CreateWindowExW(
          WS_EX_TRANSPARENT, L"BUTTON", label,
          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 1, 1,
          w->native, (HMENU)(UINT_PTR)r->id, GetModuleHandleW(NULL), NULL);
      if (button)
        SetWindowSubclass(button, region_proc, 1, (DWORD_PTR)w);
    } else
      SetWindowTextW(button, label);
    free(label);
    if (button) {
      EnableWindow(button, r->enabled);
      SetWindowPos(button, previous,
                   (int)((rect.right - sw * k) / 2 + r->x * k),
                   (int)((rect.bottom - sh * k) / 2 + r->y * k),
                   (int)(r->width * k), (int)(r->height * k), SWP_NOACTIVATE);
      previous = button;
    }
  }
  --w->updating;
}

int cui__native_opacity(cui_widget *w, double value) {
  (void)w;
  return value == 1;
}

int cui__canvas_present(cui_widget *w, cui_surface *s, double opacity) {
  (void)w;
  (void)s;
  (void)opacity;
  return 0; /* These backends consume the adopted straight-RGBA buffer. */
}
