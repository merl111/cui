#include "cui_desktop_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

int cui__backend_window_frame(cui_window *w) {
    HWND hwnd = (HWND)w->native;
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~(LONG_PTR)(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    if (w->decorated)
        style |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (w->resizable)
        style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);
    if (w->corner_radius > 0 && !w->decorated) {
        double scale = cui_window_scale(w);
        HRGN shape = CreateRoundRectRgn(
            0, 0, (int)(w->width * scale) + 1, (int)(w->height * scale) + 1,
            (int)(w->corner_radius * scale * 2), (int)(w->corner_radius * scale * 2));
        if (!shape)
            return 0;
        if (!SetWindowRgn(hwnd, shape, TRUE)) {
            DeleteObject(shape);
            return 0;
        }
    } else
        SetWindowRgn(hwnd, NULL, TRUE);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    return 1;
}
int cui__backend_window_size(cui_window *w) {
    HWND hwnd = (HWND)w->native;
    double scale = cui_window_scale(w);
    RECT rect = {0, 0, (int)(w->width * scale), (int)(w->height * scale)};
    AdjustWindowRectExForDpi(&rect, (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE), FALSE,
                             (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE), GetDpiForWindow(hwnd));
    if (!SetWindowPos(hwnd, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                      SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE))
        return 0;
    return cui__backend_window_frame(w);
}
int cui__backend_window_position(cui_window *w, int x, int y) {
    double scale = cui_window_scale(w);
    return SetWindowPos((HWND)w->native, NULL, (int)(x * scale), (int)(y * scale), 0, 0,
                        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != 0;
}
int cui__backend_window_move(cui_window *w) {
    if (!w->visible || !(GetKeyState(VK_LBUTTON) & 0x8000))
        return 0;
    ReleaseCapture();
    SendMessageW((HWND)w->native, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    return 1;
}

int cui__backend_window_get_size(cui_window *w, int *width, int *height) {
    RECT r;
    if (!GetClientRect((HWND)w->native, &r))
        return 0;
    double scale = cui_window_scale(w);
    *width = (int)((r.right - r.left) / scale);
    *height = (int)((r.bottom - r.top) / scale);
    return *width > 0 && *height > 0;
}
int cui__backend_window_resize(cui_window *w, int corner) {
    if (!(GetKeyState(VK_LBUTTON) & 0x8000))
        return 0;
    const int edges[] = {WMSZ_TOPLEFT, WMSZ_TOPRIGHT, WMSZ_BOTTOMLEFT, WMSZ_BOTTOMRIGHT};
    ReleaseCapture();
    SendMessageW((HWND)w->native, WM_SYSCOMMAND, SC_SIZE | edges[corner], 0);
    return 1;
}

int cui__backend_window_anchor(cui_window *w) {
    HWND parent = (HWND)w->anchor_parent->native;
    SetWindowLongPtrW((HWND)w->native, GWLP_HWNDPARENT, (LONG_PTR)parent);
    double scale = cui_window_scale(w->anchor_parent);
    POINT point = {(LONG)((w->anchor_x + w->anchor_width + 16) * scale),
                   (LONG)(w->anchor_y * scale)};
    if (w->popup) {
        point.x = (LONG)(w->anchor_x * scale);
        point.y = (LONG)((w->anchor_y + w->anchor_height) * scale);
    }
    ClientToScreen(parent, &point);
    if (w->popup) {
        MONITORINFO info = {sizeof(info)};
        RECT frame;
        GetWindowRect((HWND)w->native, &frame);
        if (GetMonitorInfoW(MonitorFromWindow(parent, MONITOR_DEFAULTTONEAREST), &info)) {
            LONG width = frame.right - frame.left, height = frame.bottom - frame.top;
            if (point.y + height > info.rcWork.bottom)
                point.y -= (LONG)(w->anchor_height * scale) + height;
            point.x = max(info.rcWork.left, min(point.x, info.rcWork.right - width));
            point.y = max(info.rcWork.top, min(point.y, info.rcWork.bottom - height));
        }
    }
    return SetWindowPos((HWND)w->native, NULL, point.x, point.y, 0, 0,
                        SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER) != 0;
}

int cui_widget_get_size(const cui_widget *w, int *width, int *height) {
    if (!w || !width || !height)
        return 0;
    int x = (int)w->frame.width, y = (int)w->frame.height;
    if (x <= 0 || y <= 0)
        return 0;
    *width = x;
    *height = y;
    return 1;
}

int cui__backend_popup_anchor(cui_window *panel, cui_widget *anchor, double x, double y,
                              double width, double height) {
    panel->anchor_parent = anchor->window;
    panel->anchor_x = (int)floor(anchor->frame.x + x - anchor->window->scroll_x);
    panel->anchor_y = (int)floor(anchor->frame.y + y - anchor->window->scroll_y);
    panel->anchor_width = (int)ceil(width);
    panel->anchor_height = (int)ceil(height);
    return cui__backend_window_anchor(panel);
}

wchar_t *cui__win32_wide(const char *text) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    wchar_t *wide;
    if (!count)
        return NULL;
    wide = (wchar_t *)malloc((size_t)count * sizeof(*wide));
    if (wide)
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, count);
    return wide;
}
void cui_clipboard_set_text(cui_window *window, const char *text) {
    wchar_t *wide = cui__win32_wide(text ? text : "");
    if (!window || !wide) {
        free(wide);
        return;
    }
    size_t bytes = (wcslen(wide) + 1) * sizeof(*wide);
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bytes);
    void *memory = data ? GlobalLock(data) : NULL;
    if (!memory) {
        if (data)
            GlobalFree(data);
        free(wide);
        return;
    }
    memcpy(memory, wide, bytes);
    GlobalUnlock(data);
    free(wide);
    if (OpenClipboard((HWND)window->native)) {
        if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, data))
            data = NULL;
        CloseClipboard();
    }
    if (data)
        GlobalFree(data);
}

char *cui__search_key(const char *text) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (!count)
        return NULL;
    wchar_t *wide = malloc((size_t)count * sizeof(*wide));
    if (!wide)
        return NULL;
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, count);
    int n = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide, -1, NULL, 0, NULL, NULL, 0);
    wchar_t *folded = n ? malloc((size_t)n * sizeof(*folded)) : NULL;
    if (!folded) {
        free(wide);
        return NULL;
    }
    LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide, -1, folded, n, NULL, NULL, 0);
    free(wide);
    int bytes = WideCharToMultiByte(CP_UTF8, 0, folded, -1, NULL, 0, NULL, NULL);
    char *out = bytes ? malloc((size_t)bytes) : NULL;
    if (out)
        WideCharToMultiByte(CP_UTF8, 0, folded, -1, out, bytes, NULL, NULL);
    free(folded);
    return out;
}
