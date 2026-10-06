#include "cui_win32_tables.h"
#include "cui_inputs_internal.h"
#include "cui_navigation_internal.h"
#include "cui_desktop_internal.h"
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <richedit.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <wchar.h>

typedef struct win_app_state {
    int dark, contrast, applying_theme, com_initialized;
    HMODULE rich_edit;
    COLORREF background, surface, foreground, secondary, border, accent, accent_text;
    HBRUSH background_brush, surface_brush;
} win_app_state;

typedef struct win_fonts { HFONT body, title, heading, caption, code; } win_fonts;
static cui_app *timer_app;
static const wchar_t window_class[] = L"CUI.NativeWindow.1";

static int pixels(float value, float scale) { return (int)(value * scale + 0.5f); }

wchar_t *cui__win32_wide(const char *text)
{
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    wchar_t *wide;
    if (!count) return NULL;
    wide = (wchar_t *)malloc((size_t)count * sizeof(*wide));
    if (wide) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, count);
    return wide;
}

static HFONT base_font(cui_widget *widget)
{
    win_fonts *fonts = (win_fonts *)widget->window->font;
    if (widget->role == CUI_ROLE_TITLE) return fonts->title;
    if (widget->role == CUI_ROLE_HEADING) return fonts->heading;
    if (widget->role == CUI_ROLE_CAPTION) return fonts->caption;
    if (widget->kind == CUI_CODE) return fonts->code;
    return fonts->body;
}

static HFONT widget_font(cui_widget *w)
{ return w->font_native ? (HFONT)w->font_native : base_font(w); }

static void apply_fonts(cui_widget *widget)
{
    cui_widget *child;
    cui__backend_font(widget);
    for (child = widget->first; child; child = child->next) apply_fonts(child);
}

static void free_fonts(win_fonts *fonts)
{
    if (!fonts) return;
    DeleteObject(fonts->body); DeleteObject(fonts->title);
    DeleteObject(fonts->heading); DeleteObject(fonts->caption);
    DeleteObject(fonts->code);
    free(fonts);
}

static int update_fonts(cui_window *window)
{
    NONCLIENTMETRICSW metrics = {0};
    LOGFONTW font = {0};
    win_fonts *fonts = (win_fonts *)calloc(1, sizeof(*fonts));
    if (!fonts) return 0;
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0,
                                  (UINT)pixels(96, window->scale))) font = metrics.lfMessageFont;
    else { font.lfHeight = -pixels(14, window->scale); wcscpy(font.lfFaceName, L"Segoe UI"); }
    font.lfQuality = CLEARTYPE_QUALITY;
    fonts->body = CreateFontIndirectW(&font);
    font.lfHeight = -pixels(28, window->scale); font.lfWeight = FW_SEMIBOLD;
    fonts->title = CreateFontIndirectW(&font);
    font.lfHeight = -pixels(15, window->scale);
    fonts->heading = CreateFontIndirectW(&font);
    font.lfHeight = -pixels(13, window->scale); font.lfWeight = FW_NORMAL;
    fonts->caption = CreateFontIndirectW(&font);
    wcscpy(font.lfFaceName, L"Consolas");
    fonts->code = CreateFontIndirectW(&font);
    if (!fonts->body || !fonts->title || !fonts->heading || !fonts->caption) { free_fonts(fonts); return 0; }
    win_fonts *old = (win_fonts *)window->font;
    window->font = fonts;
    if (window->root) apply_fonts(window->root);
    free_fonts(old);
    return 1;
}

static RECT native_rect(cui_rect r, float scale)
{
    RECT rect = {pixels(r.x, scale), pixels(r.y, scale), pixels(r.x + r.width, scale), pixels(r.y + r.height, scale)};
    return rect;
}

static void rounded_rect(HDC dc, RECT rect, int radius, COLORREF fill, COLORREF stroke)
{
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, stroke);
    HGDIOBJ old_brush = SelectObject(dc, brush), old_pen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius * 2, radius * 2);
    SelectObject(dc, old_brush); SelectObject(dc, old_pen);
    DeleteObject(brush); DeleteObject(pen);
}

static COLORREF surface_color(const cui_widget *w)
{
    win_app_state *state=(win_app_state *)w->window->app->native;
    if(state->contrast)return state->background;
    for(;w;w=w->parent){
        if(w->role==CUI_ROLE_OUTGOING)return state->dark?RGB(41,79,86):RGB(216,243,223);
        if(w->role==CUI_ROLE_CHAT_BACKGROUND)return state->dark?RGB(20,33,43):RGB(229,236,237);
        if(w->role==CUI_ROLE_CARD||w->role==CUI_ROLE_PANEL||w->role==CUI_ROLE_MESSAGE)return state->surface;
    }
    return state->background;
}
static int inside_card(cui_widget *widget)
{
    for (widget = widget->parent; widget; widget = widget->parent)
        if (widget->role == CUI_ROLE_CARD) return 1;
    return 0;
}

static COLORREF blend_background(COLORREF a, COLORREF b, double t)
{
    return RGB((BYTE)(GetRValue(a)*(1-t)+GetRValue(b)*t),
               (BYTE)(GetGValue(a)*(1-t)+GetGValue(b)*t),
               (BYTE)(GetBValue(a)*(1-t)+GetBValue(b)*t));
}

static void paint_ambient(cui_widget *widget, HDC dc)
{
    win_app_state *state = (win_app_state *)widget->window->app->native;
    if (state->contrast) return;
    RECT rect = native_rect(widget->frame, widget->window->scale);
    int width = rect.right-rect.left;
    COLORREF colors[3] = {state->dark ? RGB(61,48,92) : RGB(212,201,245),
        state->dark ? RGB(26,31,46) : RGB(240,240,250),
        state->dark ? RGB(23,59,59) : RGB(189,230,227)};
    /* Static native GDI gradient. At most 512 strips, independent of DPI. */
    for (int i=0; i<512 && width>0; ++i) {
        double t=i/511.0;
        COLORREF color=t<0.5 ? blend_background(colors[0],colors[1],t*2) :
            blend_background(colors[1],colors[2],(t-0.5)*2);
        RECT strip={rect.left+(int)((double)width*i/512),rect.top,
            rect.left+(int)((double)width*(i+1)/512),rect.bottom};
        HBRUSH brush=CreateSolidBrush(color);
        if(brush){FillRect(dc,&strip,brush);DeleteObject(brush);}
    }
}

static COLORREF custom_color(unsigned c) { return RGB(c>>24,(c>>16)&255,(c>>8)&255); }
void cui__backend_style(cui_widget *w) {
    if(w->native){
        if(cui__container(w)&&cui__in_layer(w))SetLayeredWindowAttributes((HWND)w->native,0,w->styled?(BYTE)(w->style.background&255):0,LWA_ALPHA);
        InvalidateRect((HWND)w->native,NULL,TRUE);
    }
}
static void paint_surfaces(cui_widget *widget, HDC dc)
{
    cui_widget *child;
    win_app_state *state = (win_app_state *)widget->window->app->native;
    if (widget->hidden) return;
    if(widget->styled&&cui__container(widget)&&!cui__in_layer(widget)){RECT r=native_rect(widget->frame,widget->window->scale);rounded_rect(dc,r,pixels(widget->style.radius,widget->window->scale),custom_color(widget->style.background),custom_color(widget->style.border_width?widget->style.border:widget->style.background));}
    if (widget->role == CUI_ROLE_AMBIENT) paint_ambient(widget, dc);
    if (widget->role == CUI_ROLE_CARD || (widget->role>=CUI_ROLE_PANEL&&widget->role<=CUI_ROLE_OUTGOING) || widget->kind == CUI_ENTRY || widget->kind == CUI_PASSWORD || widget->kind == CUI_SEARCH || widget->kind == CUI_NUMBER) {
        RECT rect = native_rect(widget->frame, widget->window->scale);
        COLORREF border = GetFocus() == widget->native ? state->accent : state->border;
        COLORREF fill=(widget->role>=CUI_ROLE_PANEL&&widget->role<=CUI_ROLE_OUTGOING)?surface_color(widget):state->surface;
        rounded_rect(dc,rect,pixels(widget->kind==CUI_ENTRY?5:8,widget->window->scale),fill,widget->role>=CUI_ROLE_PANEL?fill:border);
    }
    for (child = widget->first; child; child = child->next) paint_surfaces(child, dc);
}

static void paint_indicator(cui_widget *widget, NMCUSTOMDRAW *draw, RECT *text_rect)
{
    win_app_state *state = (win_app_state *)widget->window->app->native;
    RECT rect = draw->rc;
        float scale = widget->window->scale;
        int side = pixels(18, scale), width = widget->kind == CUI_SWITCH ? pixels(38, scale) : side;
        int checked = cui__backend_get_checked(widget);
        RECT check = {rect.left, rect.top + (rect.bottom - rect.top - side) / 2, rect.left + width, 0};
        check.bottom = check.top + side;
        FillRect(draw->hdc, &rect, inside_card(widget) ? state->surface_brush : state->background_brush);
        rounded_rect(draw->hdc, check, widget->kind == CUI_CHECKBOX ? pixels(4, scale) : side / 2, checked ? state->accent : state->surface,
                     checked ? state->accent : state->border);
        if (widget->kind == CUI_SWITCH || (widget->kind == CUI_RADIO && checked)) {
            RECT dot = check;
            if (widget->kind == CUI_SWITCH) {
                dot.left = checked ? check.right - side : check.left;
                dot.right = dot.left + side;
            }
            InflateRect(&dot, -pixels(4, scale), -pixels(4, scale));
            rounded_rect(draw->hdc, dot, side, checked ? state->accent_text : state->secondary,
                         checked ? state->accent_text : state->secondary);
        } else if (checked) {
            HPEN pen = CreatePen(PS_SOLID, pixels(2, scale), state->accent_text);
            HGDIOBJ old = SelectObject(draw->hdc, pen);
            MoveToEx(draw->hdc, check.left + pixels(4, scale), check.top + pixels(9, scale), NULL);
            LineTo(draw->hdc, check.left + pixels(8, scale), check.top + pixels(13, scale));
            LineTo(draw->hdc, check.left + pixels(14, scale), check.top + pixels(5, scale));
            SelectObject(draw->hdc, old); DeleteObject(pen);
        }
        text_rect->left += width + pixels(10, scale);
}

void cui__win_icon(cui_widget *,HDC,RECT,COLORREF);
void cui__win_icons_shutdown(void);
static LRESULT paint_button(cui_widget *widget, NMCUSTOMDRAW *draw)
{
    win_app_state *state = (win_app_state *)widget->window->app->native;
    RECT rect = draw->rc, text_rect = rect;
    int disabled = !IsWindowEnabled((HWND)widget->native);
    int primary = widget->role == CUI_ROLE_PRIMARY || (widget->kind == CUI_TOGGLE && cui_get_checked(widget));
    COLORREF fill = primary ? state->accent : state->surface;
    COLORREF foreground = primary ? state->accent_text : state->foreground;
    wchar_t *text;
    int length;
    if ((state->contrast && !widget->icon) || draw->dwDrawStage != CDDS_PREPAINT) return CDRF_DODEFAULT;
    if (!primary && (draw->uItemState & (CDIS_HOT | CDIS_SELECTED)))
        fill = state->dark ? RGB(58, 61, 67) : RGB(238, 240, 243);
    if(!primary && widget->role==CUI_ROLE_FLAT && !(draw->uItemState & (CDIS_HOT|CDIS_SELECTED)))fill=surface_color(widget);
    if (disabled) foreground = state->secondary;
    int indicator = widget->kind == CUI_CHECKBOX || widget->kind == CUI_SWITCH || widget->kind == CUI_RADIO;
    if (indicator) {
        paint_indicator(widget, draw, &text_rect);
    } else rounded_rect(draw->hdc, rect, pixels(5, widget->window->scale), fill, primary || widget->role==CUI_ROLE_FLAT ? fill : state->border);
    if(widget->icon){
        int size=pixels(widget->icon_size?widget->icon_size:20,widget->window->scale);
        RECT icon_rect=rect;icon_rect.left=widget->icon_only?(rect.left+rect.right-size)/2:widget->icon_trailing?rect.right-pixels(12,widget->window->scale)-size:rect.left+pixels(12,widget->window->scale);
        icon_rect.top=(rect.top+rect.bottom-size)/2;icon_rect.right=icon_rect.left+size;icon_rect.bottom=icon_rect.top+size;
        cui__win_icon(widget,draw->hdc,icon_rect,foreground);if(widget->icon_trailing)text_rect.right=icon_rect.left-pixels(8,widget->window->scale);else text_rect.left=icon_rect.right+pixels(8,widget->window->scale);
    }
    length = GetWindowTextLengthW((HWND)widget->native);
    text = (wchar_t *)calloc((size_t)length + 1, sizeof(*text));
    if (text && !widget->icon_only) {
        HGDIOBJ old_font = SelectObject(draw->hdc, widget_font(widget));
        GetWindowTextW((HWND)widget->native, text, length + 1);
        SetBkMode(draw->hdc, TRANSPARENT); SetTextColor(draw->hdc, foreground);
        DrawTextW(draw->hdc, text, -1, &text_rect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX |
                  (indicator ? DT_LEFT : DT_CENTER));
        SelectObject(draw->hdc, old_font);
    }
    free(text);
    if (!widget->window->app->hide_focus && (draw->uItemState & CDIS_FOCUS) && !(SendMessageW((HWND)widget->native, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS)) {
        InflateRect(&rect, -3, -3); DrawFocusRect(draw->hdc, &rect);
    }
    return CDRF_SKIPDEFAULT;
}

static void update_minimum(cui_window *window, MINMAXINFO *info)
{
    if (window->scrollable) { info->ptMinTrackSize.x = pixels(240, window->scale); info->ptMinTrackSize.y = pixels(180, window->scale); return; }
    cui_size size = cui__measure(window->root);
    RECT rect = {0, 0, pixels(size.width, window->scale), pixels(size.height, window->scale)};
    AdjustWindowRectExForDpi(&rect, WS_OVERLAPPEDWINDOW, window->menu != NULL, 0, (UINT)pixels(96, window->scale));
    info->ptMinTrackSize.x = rect.right - rect.left;
    info->ptMinTrackSize.y = rect.bottom - rect.top;
}

static void update_scrollbars(cui_window *window)
{
    RECT client;
    float old_x = window->scroll_x, old_y = window->scroll_y;
    SCROLLINFO info = {0};
    if (!window->scrollable) return;
    GetClientRect((HWND)window->native, &client);
    info.cbSize = sizeof(info); info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    info.nMax = pixels(window->document_height, window->scale) - 1;
    info.nPage = (UINT)client.bottom; info.nPos = pixels(window->scroll_y, window->scale);
    SetScrollInfo((HWND)window->native, SB_VERT, &info, TRUE);
    window->scroll_y = (float)GetScrollPos((HWND)window->native, SB_VERT) / window->scale;
    info.nMax = pixels(window->document_width, window->scale) - 1;
    info.nPage = (UINT)client.right; info.nPos = pixels(window->scroll_x, window->scale);
    SetScrollInfo((HWND)window->native, SB_HORZ, &info, TRUE);
    window->scroll_x = (float)GetScrollPos((HWND)window->native, SB_HORZ) / window->scale;
    if (old_x != window->scroll_x || old_y != window->scroll_y)
        cui__layout(window, (float)client.right / window->scale, (float)client.bottom / window->scale);
}
static void scroll_window(cui_window *window, int bar, int command, int wheel)
{
    SCROLLINFO info = {0};
    int position;
    info.cbSize = sizeof(info); info.fMask = SIF_ALL;
    GetScrollInfo((HWND)window->native, bar, &info);
    position = info.nPos;
    if (wheel) position -= wheel;
    else switch (command) {
    case SB_LINEUP: position -= pixels(24, window->scale); break;
    case SB_LINEDOWN: position += pixels(24, window->scale); break;
    case SB_PAGEUP: position -= (int)info.nPage; break;
    case SB_PAGEDOWN: position += (int)info.nPage; break;
    case SB_THUMBPOSITION: case SB_THUMBTRACK: position = info.nTrackPos; break;
    case SB_TOP: position = 0; break;
    case SB_BOTTOM: position = info.nMax; break;
    default: return;
    }
    info.nPos = position; info.fMask = SIF_POS;
    SetScrollInfo((HWND)window->native, bar, &info, TRUE);
    position = GetScrollPos((HWND)window->native, bar);
    if (bar == SB_VERT) window->scroll_y = (float)position / window->scale;
    else window->scroll_x = (float)position / window->scale;
    cui__backend_refresh(window);
}

static LRESULT paint_badge(cui_widget *widget, DRAWITEMSTRUCT *draw)
{
    win_app_state *state = (win_app_state *)widget->window->app->native;
    RECT rect = draw->rcItem;
    wchar_t text[256];
    HGDIOBJ old = SelectObject(draw->hDC, widget_font(widget));
    COLORREF fill = state->dark ? RGB(51,59,82) : RGB(231,236,255);
    COLORREF ink = state->dark ? RGB(196,208,255) : RGB(52,79,189);
    if (widget->role == CUI_ROLE_SUCCESS) { fill = state->dark ? RGB(37,60,50) : RGB(228,244,234); ink = state->dark ? RGB(156,224,182) : RGB(34,115,69); }
    if (widget->role == CUI_ROLE_WARNING) { fill = state->dark ? RGB(68,58,37) : RGB(255,241,211); ink = state->dark ? RGB(243,205,133) : RGB(133,84,0); }
    if (widget->role == CUI_ROLE_DANGER) { fill = state->dark ? RGB(69,45,52) : RGB(252,232,233); ink = state->dark ? RGB(243,172,178) : RGB(167,49,58); }
    if (state->contrast) { fill = state->surface; ink = state->foreground; }
    rounded_rect(draw->hDC, rect, pixels(12, widget->window->scale), fill, fill);
    GetWindowTextW((HWND)widget->native, text, 256);
    SetTextColor(draw->hDC, ink); SetBkMode(draw->hDC, TRANSPARENT);
    DrawTextW(draw->hDC, text, -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    SelectObject(draw->hDC, old);
    return TRUE;
}

static LRESULT control_color(cui_window *window, HDC dc, HWND control)
{
    win_app_state *state = (win_app_state *)window->app->native;
    cui_widget *widget = (cui_widget *)GetWindowLongPtrW(control, GWLP_USERDATA);
    int surface = widget && (widget->kind == CUI_ENTRY || inside_card(widget));
    SetTextColor(dc, widget && widget->role == CUI_ROLE_CAPTION ? state->secondary : state->foreground);
    COLORREF background=surface?state->surface:widget?surface_color(widget):state->background;
    if(widget&&widget->styled){background=custom_color(widget->style.background);SetTextColor(dc,custom_color(widget->style.foreground));}
    SetBkColor(dc,background);SetDCBrushColor(dc,background);
    return (LRESULT)GetStockObject(DC_BRUSH);
}

static LRESULT paint_media(cui_widget *w, DRAWITEMSTRUCT *d)
{
    RECT r = d->rcItem;
    win_app_state *state = (win_app_state *)w->window->app->native;
    SetDCBrushColor(d->hDC,w->kind==CUI_ICON?surface_color(w):state->surface);FillRect(d->hDC,&r,(HBRUSH)GetStockObject(DC_BRUSH));
    if(w->kind==CUI_ICON){int size=pixels(w->icon_size?w->icon_size:20,w->window->scale);RECT target={(r.left+r.right-size)/2,(r.top+r.bottom-size)/2,0,0};target.right=target.left+size;target.bottom=target.top+size;cui__win_icon(w,d->hDC,target,state->foreground);}
    else if (w->kind == CUI_CHART && w->series_count) {
        double low = w->series[0], high = low;
        size_t i;
        for (i = 1; i < w->series_count; ++i) { if (w->series[i] < low) low = w->series[i]; if (w->series[i] > high) high = w->series[i]; }
        if (high == low) high = low + 1;
        HPEN pen = CreatePen(PS_SOLID, 2, state->accent);
        HGDIOBJ old = SelectObject(d->hDC, pen);
        for (i = 0; i < w->series_count; ++i) {
            int x = 12 + (int)((r.right - 24) * (double)i / (double)(w->series_count > 1 ? w->series_count - 1 : 1));
            int y = r.bottom - 12 - (int)((r.bottom - 24) * (w->series[i] - low) / (high - low));
            if (!i) MoveToEx(d->hDC, x, y, NULL); else LineTo(d->hDC, x, y);
        }
        SelectObject(d->hDC, old); DeleteObject(pen);
    } else if (w->pixels) {
        size_t n = (size_t)w->image_width * w->image_height, i;
        unsigned char *bgra = (unsigned char *)malloc(n * 4);
        if (!bgra) return TRUE;
        for (i = 0; i < n; ++i) {
            unsigned a = w->pixels[i*4+3];
            bgra[i*4] = (unsigned char)((w->pixels[i*4+2]*a + GetBValue(state->surface)*(255-a))/255);
            bgra[i*4+1] = (unsigned char)((w->pixels[i*4+1]*a + GetGValue(state->surface)*(255-a))/255);
            bgra[i*4+2] = (unsigned char)((w->pixels[i*4]*a + GetRValue(state->surface)*(255-a))/255);
            bgra[i*4+3] = 255;
        }
        BITMAPINFO info = {0}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w->image_width; info.bmiHeader.biHeight = -w->image_height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        double scale = min((double)r.right / w->image_width, (double)r.bottom / w->image_height);
        int width = (int)(w->image_width*scale), height = (int)(w->image_height*scale);
        SetStretchBltMode(d->hDC, HALFTONE);
        StretchDIBits(d->hDC, (r.right-width)/2, (r.bottom-height)/2, width, height, 0, 0,
            w->image_width, w->image_height, bgra, &info, DIB_RGB_COLORS, SRCCOPY);
        free(bgra);
    }
    return TRUE;
}

static int is_action(cui_widget *w, unsigned code)
{
    switch (w->kind) {
    case CUI_ENTRY: case CUI_PASSWORD: case CUI_SEARCH: case CUI_TEXTAREA: return code == EN_CHANGE;
    case CUI_BUTTON: case CUI_CHECKBOX: case CUI_TOGGLE: case CUI_SWITCH: case CUI_RADIO: return code == BN_CLICKED;
    case CUI_SELECT: return code == CBN_SELCHANGE;
    case CUI_LIST: return code == LBN_SELCHANGE;
    default: return 0;
    }
}
static void tree_notification(cui_widget *w, NMHDR *header)
{
    cui_tree_model *m = w->payload;
    if (!m || w->updating) return;
    if (header->code == TVN_SELCHANGEDW || header->code == TVN_ITEMEXPANDEDW) {
        NMTREEVIEWW *change = (NMTREEVIEWW *)header;
        size_t slot = (size_t)change->itemNew.lParam;
        cui_item_id id = slot && slot <= m->count ? m->nodes[slot - 1].id : 0;
        cui__tree_event(w, header->code == TVN_SELCHANGEDW ? CUI_TREE_SELECTION :
            (change->itemNew.state & TVIS_EXPANDED) ? CUI_TREE_EXPAND : CUI_TREE_COLLAPSE, id);
    } else if (header->code == NM_DBLCLK || (header->code == TVN_KEYDOWN && ((NMTVKEYDOWN *)header)->wVKey == VK_RETURN)) {
        cui__tree_event(w, CUI_TREE_ACTIVATE, m->selected);
    }
}
static LRESULT control_notification(NMHDR *header)
{
    cui_widget *w = (cui_widget *)GetWindowLongPtrW(header->hwndFrom, GWLP_USERDATA);
    if (!w) return 0;
    if ((w->kind == CUI_DATE || w->kind == CUI_TIME_INPUT) && header->code == DTN_DATETIMECHANGE) {
        NMDATETIMECHANGE *change=(NMDATETIMECHANGE *)header;
        if(change->dwFlags==GDT_VALID){
            SYSTEMTIME v=change->st;
            if(w->kind==CUI_DATE)cui__date_user(w,(cui_date_value){v.wYear,v.wMonth,v.wDay});
            else cui__time_user(w,(cui_time_value){v.wHour,v.wMinute,v.wSecond});
        }return 0;
    }
    if (w->kind == CUI_NUMBER && header->code == UDN_DELTAPOS) {
        cui__number_step(w, -((NMUPDOWN *)header)->iDelta); return 1;
    }
    if (w->kind == CUI_TREE) { tree_notification(w, header); return 0; }
    if (header->code == NM_CUSTOMDRAW && (w->kind == CUI_BUTTON || cui__is_checkable(w))) return paint_button(w, (NMCUSTOMDRAW *)header);
    if (w->kind == CUI_TABLE) return cui__win32_table_notify(w,header);
    return 0;
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    cui_window *window = (cui_window *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (message == WM_NCCREATE) {
        window = (cui_window *)((CREATESTRUCTW *)lp)->lpCreateParams;
        window->native = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)window);
    }
    if (!window) return DefWindowProcW(hwnd, message, wp, lp);
    switch (message) {
    case WM_CLOSE: cui_window_close(window); return 0;
    case WM_ACTIVATE:
        if(window->popup&&LOWORD(wp)==WA_INACTIVE)cui_window_close(window);
        break;
    case WM_MOVE:
        for(cui_window *p=window->app->windows;p;p=p->next)
            if(p->anchor_parent==window)cui__backend_window_anchor(p);
        break;
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) {
            cui__layout(window, (float)LOWORD(lp) / window->scale, (float)HIWORD(lp) / window->scale);
            update_scrollbars(window);
        }
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_GETMINMAXINFO:
        if (window->root) update_minimum(window, (MINMAXINFO *)lp);
        return 0;
    case WM_DPICHANGED: {
        RECT *rect = (RECT *)lp;
        window->scale = (float)HIWORD(wp) / 96.0f;
        if (!update_fonts(window)) window->app->error = "Could not update DPI fonts";
        SetWindowPos(hwnd, NULL, rect->left, rect->top, rect->right - rect->left,
                     rect->bottom - rect->top, SWP_NOACTIVATE | SWP_NOZORDER);
        cui__backend_refresh(window);
        return 0;
    }
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        cui__backend_theme(window->app);
        if (window->font) update_fonts(window);
        cui__backend_refresh(window);
        return 0;
    case WM_COMMAND: {
        cui_widget *widget = lp ? (cui_widget *)GetWindowLongPtrW((HWND)lp, GWLP_USERDATA) : NULL;
        if (!lp && cui__desktop_command_id(window, LOWORD(wp))) return 0;
        if (!lp && LOWORD(wp) == IDCANCEL) { cui_window_close(window); return 0; }
        if (!widget) break;
        if (is_action(widget, HIWORD(wp))) cui__emit(widget);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_NOTIFY: return control_notification((NMHDR *)lp);
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *draw = (DRAWITEMSTRUCT *)lp;
        cui_widget *widget = (cui_widget *)GetWindowLongPtrW(draw->hwndItem, GWLP_USERDATA);
        if (widget && widget->kind == CUI_BADGE) return paint_badge(widget, draw);
        if (widget && (widget->kind == CUI_ICON || widget->kind == CUI_CHART || (widget->kind == CUI_IMAGE || widget->kind == CUI_CANVAS))) return paint_media(widget, draw);
        break;
    }
    case WM_HSCROLL: case WM_VSCROLL:
        if (lp) {
            cui_widget *widget = (cui_widget *)GetWindowLongPtrW((HWND)lp, GWLP_USERDATA);
            if (widget && widget->kind == CUI_SLIDER && LOWORD(wp) != TB_ENDTRACK) cui__emit(widget);
        } else if (window->scrollable) scroll_window(window, message == WM_VSCROLL ? SB_VERT : SB_HORZ, LOWORD(wp), 0);
        return 0;
    case WM_MOUSEWHEEL:
        if (window->scrollable) { scroll_window(window, SB_VERT, 0, pixels((float)(short)HIWORD(wp) / WHEEL_DELTA * 48, window->scale)); return 0; }
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: return control_color(window, (HDC)wp, (HWND)lp);
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        FillRect(dc, &paint.rcPaint, ((win_app_state *)window->app->native)->background_brush);
        SetViewportOrgEx(dc, -pixels(window->scroll_x, window->scale), -pixels(window->scroll_y, window->scale), NULL);
        if (window->root) paint_surfaces(window->root, dc);
        EndPaint(hwnd, &paint);
        return 0;
    }
    default: break;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}

static void theme_control(cui_widget *w)
{
    win_app_state *state=w->window->app->native;
    if(w->kind==CUI_TABLE){
        ListView_SetBkColor((HWND)w->native,state->surface);
        ListView_SetTextBkColor((HWND)w->native,state->surface);
        ListView_SetTextColor((HWND)w->native,state->foreground);
    }else if(w->kind==CUI_TREE){
        TreeView_SetBkColor((HWND)w->native,state->surface);
        TreeView_SetTextColor((HWND)w->native,state->foreground);
    }else if(w->kind==CUI_TEXTAREA||w->kind==CUI_CODE){
        SendMessageW((HWND)w->native,EM_SETBKGNDCOLOR,0,state->surface);
        CHARFORMAT2W format={0};format.cbSize=sizeof(format);format.dwMask=CFM_COLOR;format.crTextColor=state->foreground;
        SendMessageW((HWND)w->native,EM_SETCHARFORMAT,SCF_ALL,(LPARAM)&format);
    }
    for(cui_widget *child=w->first;child;child=child->next)theme_control(child);
}
cui_theme cui__backend_resolved_theme(cui_app *app)
{
    (void)app;
    DWORD light = 1, bytes = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &light, &bytes);
    return light ? CUI_THEME_LIGHT : CUI_THEME_DARK;
}

void cui__backend_theme(cui_app *app)
{
    win_app_state *state = (win_app_state *)app->native;
    DWORD light = 1, bytes = sizeof(light), color = 0;
    BOOL opaque = TRUE;
    HIGHCONTRASTW contrast = {0};
    cui_window *window;
    if (state->applying_theme) return;
    state->applying_theme = 1;
    contrast.cbSize = sizeof(contrast);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &light, &bytes);
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    state->contrast = !!(contrast.dwFlags & HCF_HIGHCONTRASTON);
    state->dark = app->theme == CUI_THEME_DARK || (app->theme == CUI_THEME_SYSTEM && !light);
    state->background = state->dark ? RGB(32,32,32) : RGB(243,243,243);
    state->surface = state->dark ? RGB(43,43,43) : RGB(255,255,255);
    state->foreground = state->dark ? RGB(242,242,242) : RGB(26,26,26);
    state->secondary = state->dark ? RGB(178,178,178) : RGB(96,96,96);
    state->border = state->dark ? RGB(68,68,68) : RGB(218,218,218);
    state->accent = RGB(0,95,184);
    if (SUCCEEDED(DwmGetColorizationColor(&color, &opaque)))
        state->accent = RGB((color >> 16) & 255, (color >> 8) & 255, color & 255);
    state->accent_text = (GetRValue(state->accent) * 299 + GetGValue(state->accent) * 587 + GetBValue(state->accent) * 114 > 150000)
                        ? RGB(0,0,0) : RGB(255,255,255);
    if (state->contrast) {
        state->background = state->surface = GetSysColor(COLOR_WINDOW);
        state->foreground = state->secondary = GetSysColor(COLOR_WINDOWTEXT);
        state->border = GetSysColor(COLOR_WINDOWTEXT);
        state->accent = GetSysColor(COLOR_HIGHLIGHT);
        state->accent_text = GetSysColor(COLOR_HIGHLIGHTTEXT);
    }
    DeleteObject(state->background_brush); DeleteObject(state->surface_brush);
    state->background_brush = CreateSolidBrush(state->background);
    state->surface_brush = CreateSolidBrush(state->surface);
    for (window = app->windows; window; window = window->next) {
        BOOL dark = state->dark && !state->contrast;
        int rounded = 2; /* DWMWCP_ROUND. Unsupported attributes safely fail on Windows 10. */
        DwmSetWindowAttribute((HWND)window->native, 20, &dark, sizeof(dark));
        DwmSetWindowAttribute((HWND)window->native, 33, &rounded, sizeof(rounded));
        if(window->root)theme_control(window->root);
        RedrawWindow((HWND)window->native, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    state->applying_theme = 0;
}

int cui__backend_init(cui_app *app)
{
    WNDCLASSEXW cls = {0};
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_TREEVIEW_CLASSES | ICC_DATE_CLASSES | ICC_UPDOWN_CLASS};
    app->native = calloc(1, sizeof(win_app_state));
    if (!app->native) return 0;
    win_app_state *state = (win_app_state *)app->native;
    state->com_initialized = SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));
    state->rich_edit = LoadLibraryExW(L"Msftedit.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    InitCommonControlsEx(&controls);
    cls.cbSize = sizeof(cls); cls.lpfnWndProc = window_proc;
    cls.hInstance = GetModuleHandleW(NULL); cls.hCursor = LoadCursorW(NULL, IDC_ARROW);
    cls.lpszClassName = window_class;
    if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        if (state->rich_edit) FreeLibrary(state->rich_edit);
        if (state->com_initialized) CoUninitialize();
        free(state); app->native=NULL; return 0;
    }
    timer_app = app;
    cui__backend_theme(app);
    return 1;
}

void cui__backend_shutdown(cui_app *app)
{
    cui__win_icons_shutdown();
    win_app_state *state = (win_app_state *)app->native;
    DeleteObject(state->background_brush); DeleteObject(state->surface_brush);
    if(state->rich_edit) FreeLibrary(state->rich_edit);
    if(state->com_initialized) CoUninitialize();
    free(state); timer_app = NULL;
    UnregisterClassW(window_class, GetModuleHandleW(NULL));
}

static unsigned key_modifiers(void)
{
    unsigned result=0;
    if(GetKeyState(VK_SHIFT)&0x8000)result|=CUI_MOD_SHIFT;
    if(GetKeyState(VK_MENU)&0x8000)result|=CUI_MOD_ALT;
    if(GetKeyState(VK_CONTROL)&0x8000)result|=CUI_MOD_CONTROL;
    return result;
}
int cui__win32_key_message(MSG *message);
static int route_message(cui_app *app, MSG *message)
{
    int key=message->message==WM_KEYDOWN || message->message==WM_SYSKEYDOWN;
    HWND active=GetActiveWindow();
    for(cui_window *w=app->windows;w;w=w->next){
        if(!w->visible || active!=(HWND)w->native)continue;
        if(key&&w->popup&&message->wParam==VK_ESCAPE){cui_window_close(w);return 1;}
        if(cui__win32_key_message(message))return 1;
        if(key && cui__desktop_key(w,(unsigned)message->wParam,key_modifiers()))return 1;
        return IsDialogMessageW((HWND)w->native,message);
    }
    return 0;
}
void cui__backend_run(cui_app *app)
{
    MSG message;
    while(app->running){
        int result=GetMessageW(&message,NULL,0,0);
        if(result<=0){if(result<0)app->error="GetMessage failed";break;}
        if(!route_message(app,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
    }
}
void cui__backend_quit(cui_app *app) { (void)app; /* Called on the UI thread; loop observes running. */ }

int cui__backend_window_create(cui_window *window, const char *title)
{
    wchar_t *wide = cui__win32_wide(title);
    HWND native;
    RECT rect;
    if (!wide) return 0;
    native = CreateWindowExW(0, window_class, wide, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, window->width, window->height, NULL, NULL, GetModuleHandleW(NULL), window);
    free(wide);
    if (!native) return 0;
    window->scale = (float)GetDpiForWindow(native) / 96.0f;
    if (!update_fonts(window)) { DestroyWindow(native); return 0; }
    rect = (RECT){0, 0, pixels((float)window->width, window->scale), pixels((float)window->height, window->scale)};
    AdjustWindowRectExForDpi(&rect, WS_OVERLAPPEDWINDOW, window->menu != NULL, 0, GetDpiForWindow(native));
    SetWindowPos(native, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER);
    return 1;
}
void cui__backend_window_destroy(cui_window *window)
{ DestroyWindow((HWND)window->native); free_fonts((win_fonts *)window->font); }
void cui__backend_window_show(cui_window *window)
{
    cui__backend_theme(window->app);
    ShowWindow((HWND)window->native, SW_SHOW);
    UpdateWindow((HWND)window->native);
}
void cui__backend_window_hide(cui_window *window) { ShowWindow((HWND)window->native, SW_HIDE); }

void cui__backend_refresh(cui_window *window)
{
    RECT rect;
    cui_size minimum;
    if (!window->root || window->laying_out) return;
    GetClientRect((HWND)window->native, &rect);
    minimum = cui__measure(window->root);
    if (!window->scrollable && (rect.right < pixels(minimum.width, window->scale) || rect.bottom < pixels(minimum.height, window->scale))) {
        rect.right = max(rect.right, pixels(minimum.width, window->scale));
        rect.bottom = max(rect.bottom, pixels(minimum.height, window->scale));
        AdjustWindowRectExForDpi(&rect, WS_OVERLAPPEDWINDOW, window->menu != NULL, 0, GetDpiForWindow((HWND)window->native));
        SetWindowPos((HWND)window->native, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        GetClientRect((HWND)window->native, &rect);
    }
    cui__layout(window, (float)rect.right / window->scale, (float)rect.bottom / window->scale);
    update_scrollbars(window);
    InvalidateRect((HWND)window->native, NULL, TRUE);
}

static void control_class(cui_kind kind, const wchar_t **name, DWORD *style)
{
    switch (kind) {
    case CUI_LABEL: *name = L"STATIC"; *style |= SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX; break;
    case CUI_ICON: case CUI_CHART: case CUI_IMAGE: case CUI_CANVAS: case CUI_BADGE: *name = L"STATIC"; *style |= SS_OWNERDRAW; break;
    case CUI_SEPARATOR: *name = L"STATIC"; *style |= SS_ETCHEDHORZ; break;
    case CUI_NUMBER: case CUI_ENTRY: case CUI_PASSWORD: case CUI_SEARCH:
        *name = L"EDIT"; *style |= ES_AUTOHSCROLL | WS_TABSTOP | (kind == CUI_PASSWORD ? ES_PASSWORD : 0); break;
    case CUI_TEXTAREA: case CUI_CODE:
        *name = MSFTEDIT_CLASS; *style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP | WS_BORDER |
            (kind == CUI_CODE ? ES_READONLY | ES_AUTOHSCROLL | WS_HSCROLL : 0); break;
    case CUI_DATE: case CUI_TIME_INPUT: *name=DATETIMEPICK_CLASSW; *style|=WS_TABSTOP|(kind==CUI_DATE?DTS_SHORTDATECENTURYFORMAT:DTS_TIMEFORMAT);break;
    case CUI_SELECT: *name = L"COMBOBOX"; *style |= CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL; break;
    case CUI_TREE: *name = WC_TREEVIEWW; *style |= TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS | WS_TABSTOP; break;
    case CUI_LIST: *name = L"LISTBOX"; *style |= LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_TABSTOP | WS_VSCROLL | WS_BORDER; break;
    case CUI_TABLE: *name = WC_LISTVIEWW; *style |= LVS_REPORT | LVS_OWNERDATA | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP; break;
    case CUI_SLIDER: *name = TRACKBAR_CLASSW; *style |= TBS_HORZ | TBS_NOTICKS | WS_TABSTOP; break;
    case CUI_PROGRESS: case CUI_SPINNER: *name = PROGRESS_CLASSW; *style |= kind == CUI_SPINNER ? PBS_MARQUEE : PBS_SMOOTH; break;
    default:
        *style |= WS_TABSTOP;
        if (kind == CUI_TOGGLE) *style |= BS_AUTOCHECKBOX | BS_PUSHLIKE;
        else if (kind == CUI_RADIO) *style |= BS_RADIOBUTTON;
        else if (kind == CUI_CHECKBOX || kind == CUI_SWITCH) *style |= BS_AUTOCHECKBOX;
        else *style |= BS_PUSHBUTTON;
        break;
    }
}
static void split_drag(cui_widget *w, POINT point)
{
    ScreenToClient((HWND)w->window->native, &point);
    double position = w->axis == CUI_HORIZONTAL ? (double)point.x / w->window->scale + w->window->scroll_x - w->frame.x :
        (double)point.y / w->window->scale + w->window->scroll_y - w->frame.y;
    double size = w->axis == CUI_HORIZONTAL ? w->frame.width : w->frame.height;
    if(size <= 0)return;
    w->value = position / size; if(w->value < 0)w->value = 0; if(w->value > 1)w->value = 1;
    cui__backend_refresh(w->window);cui__emit(w);
}
static LRESULT CALLBACK split_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    cui_widget *w=(cui_widget *)data;(void)id;
    switch(message){
    case WM_LBUTTONDOWN:SetFocus(hwnd);SetCapture(hwnd);return 0;
    case WM_LBUTTONUP:if(GetCapture()==hwnd)ReleaseCapture();return 0;
    case WM_MOUSEMOVE:if(GetCapture()==hwnd){POINT point;GetCursorPos(&point);split_drag(w,point);}return 0;
    case WM_SETCURSOR:SetCursor(LoadCursorW(NULL,w->axis==CUI_HORIZONTAL?IDC_SIZEWE:IDC_SIZENS));return TRUE;
    case WM_GETDLGCODE:return DLGC_WANTARROWS;
    case WM_KEYDOWN:
        if(wp==VK_LEFT||wp==VK_UP)w->value-=0.02;else if(wp==VK_RIGHT||wp==VK_DOWN)w->value+=0.02;else if(wp==VK_HOME)w->value=0;else if(wp==VK_END)w->value=1;else break;
        if(w->value<0)w->value=0;if(w->value>1)w->value=1;cui__backend_refresh(w->window);cui__emit(w);return 0;
    case WM_PAINT:{PAINTSTRUCT paint;HDC dc=BeginPaint(hwnd,&paint);RECT r;GetClientRect(hwnd,&r);FillRect(dc,&r,GetSysColorBrush(GetFocus()==hwnd?COLOR_HIGHLIGHT:COLOR_3DSHADOW));EndPaint(hwnd,&paint);return 0;}
    case WM_SETFOCUS:case WM_KILLFOCUS:InvalidateRect(hwnd,NULL,TRUE);return 0;
    case WM_NCDESTROY:RemoveWindowSubclass(hwnd,split_proc,0);break;
    }
    return DefSubclassProc(hwnd,message,wp,lp);
}

static void number_commit(cui_widget *w)
{
    if (!w->payload || w->updating) return;
    char text[128], *end;
    GetWindowTextA((HWND)w->native, text, sizeof(text));
    double value = strtod(text, &end);
    while (*end == ' ' || *end == '\t') ++end;
    cui_number_state *s = w->payload;
    if (end == text || *end || !isfinite(value) || value < s->minimum || value > s->maximum) value = w->value;
    cui__number_user(w, value);
}
static LRESULT CALLBACK number_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
{
    cui_widget *w = (cui_widget *)data; (void)id;
    if (message == WM_KEYDOWN && (wp == VK_UP || wp == VK_DOWN)) { cui__number_step(w, wp == VK_UP ? 1 : -1); return 0; }
    if (message == WM_KEYDOWN && wp == VK_RETURN) { number_commit(w); return 0; }
    if (message == WM_KILLFOCUS) number_commit(w);
    if (message == WM_GETDLGCODE) return DLGC_WANTARROWS | (lp && ((MSG *)lp)->wParam == VK_RETURN ? DLGC_WANTMESSAGE : 0);
    if (message == WM_NCDESTROY) RemoveWindowSubclass(hwnd, number_proc, 0);
    return DefSubclassProc(hwnd, message, wp, lp);
}
static LRESULT CALLBACK layer_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    cui_widget *w=(cui_widget *)data;(void)id;
    if(message==WM_PAINT){
        PAINTSTRUCT paint;HDC dc=BeginPaint(hwnd,&paint);RECT r;GetClientRect(hwnd,&r);
        if(w->styled)rounded_rect(dc,r,pixels(w->style.radius,w->window->scale),custom_color(w->style.background),custom_color(w->style.border_width?w->style.border:w->style.background));
        EndPaint(hwnd,&paint);return 0;
    }
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_NCDESTROY)RemoveWindowSubclass(hwnd,layer_proc,0);
    return DefSubclassProc(hwnd,message,wp,lp);
}
int cui__backend_widget_create(cui_widget *widget, const char *text)
{
    const wchar_t *class_name = L"BUTTON";
    DWORD style = WS_CHILD | WS_VISIBLE;
    wchar_t *wide;
    HWND native;
    if(widget->kind==CUI_SPLIT){
        widget->native=CreateWindowExW(0,L"STATIC",L"Resize panes",WS_CHILD|WS_VISIBLE|WS_TABSTOP|SS_NOTIFY,0,0,8,8,(HWND)widget->window->native,NULL,GetModuleHandleW(NULL),NULL);
        if(!widget->native)return 0;SetWindowSubclass((HWND)widget->native,split_proc,0,(DWORD_PTR)widget);return 1;
    }
    if (cui__container(widget)) {
        if(cui__in_layer(widget)){
            widget->native=CreateWindowExW(WS_EX_LAYERED,L"STATIC",L"",WS_CHILD|WS_VISIBLE,0,0,1,1,(HWND)widget->window->native,NULL,GetModuleHandleW(NULL),NULL);
            if(!widget->native)return 0;
            SetWindowSubclass((HWND)widget->native,layer_proc,0,(DWORD_PTR)widget);
            SetLayeredWindowAttributes((HWND)widget->native,0,0,LWA_ALPHA);
        }
        return 1;
    }
    control_class(widget->kind, &class_name, &style);
    wide = cui__win32_wide(text);
    if (!wide) return 0;
    native = CreateWindowExW(0, class_name, wide, style, 0, 0, 0, 0,
        (HWND)widget->window->native, NULL, GetModuleHandleW(NULL), NULL);
    free(wide);
    if (!native) return 0;
    widget->native = native;
    SetWindowLongPtrW(native, GWLP_USERDATA, (LONG_PTR)widget);
    SendMessageW(native, WM_SETFONT, (WPARAM)widget_font(widget), FALSE);
    if (widget->kind == CUI_TEXTAREA || widget->kind == CUI_CODE) {
        SendMessageW(native, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE);
        SendMessageW(native, EM_SETUNDOLIMIT, 100, 0);
    }
    if (widget->kind == CUI_NUMBER) {
        HWND stepper = CreateWindowExW(0, UPDOWN_CLASSW, L"", WS_CHILD | WS_VISIBLE | UDS_ARROWKEYS | UDS_HOTTRACK,
            0, 0, 0, 0, (HWND)widget->window->native, NULL, GetModuleHandleW(NULL), NULL);
        if (!stepper) { DestroyWindow(native); widget->native = NULL; return 0; }
        widget->aux = stepper; SetWindowLongPtrW(stepper, GWLP_USERDATA, (LONG_PTR)widget);
        SendMessageW(stepper, UDM_SETRANGE32, 0, 100); SendMessageW(stepper, UDM_SETPOS32, 0, 50);
        SetWindowSubclass(native, number_proc, 0, (DWORD_PTR)widget);
    }
    if (widget->kind == CUI_SLIDER) SendMessageW(native, TBM_SETRANGE, TRUE, MAKELPARAM(0, 10000));
    if (widget->kind == CUI_PROGRESS) SendMessageW(native, PBM_SETRANGE32, 0, 10000);
    if (widget->kind == CUI_SPINNER) SendMessageW(native, PBM_SETMARQUEE, TRUE, 30);
    if (widget->kind == CUI_TABLE) cui__win32_table_init(widget);
    theme_control(widget);
    return 1;
}
void cui__backend_expand(cui_widget *widget) { (void)widget; }
void cui__backend_padding(cui_widget *widget) { (void)widget; }
void cui__backend_role(cui_widget *widget)
{
    if (widget->native) {
        SendMessageW((HWND)widget->native, WM_SETFONT, (WPARAM)widget_font(widget), TRUE);
        InvalidateRect((HWND)widget->native, NULL, TRUE);
    }
}
void cui__backend_set_text(cui_widget *widget, const char *text)
{
    wchar_t *wide = cui__win32_wide(text);
    if (!wide) { widget->window->app->error = "Invalid UTF-8 or out of memory"; return; }
    SetWindowTextW((HWND)widget->native, wide);
    free(wide);
}

size_t cui__backend_get_text(const cui_widget *widget, char *buffer, size_t capacity)
{
    int length = GetWindowTextLengthW((HWND)widget->native), bytes;
    wchar_t *wide = (wchar_t *)calloc((size_t)length + 1, sizeof(*wide));
    char *text;
    size_t result;
    if (!wide) return cui__copy_text("", buffer, capacity);
    GetWindowTextW((HWND)widget->native, wide, length + 1);
    bytes = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (bytes <= 0) { free(wide); return cui__copy_text("", buffer, capacity); }
    text = (char *)malloc((size_t)bytes);
    if (!text) { free(wide); return cui__copy_text("", buffer, capacity); }
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, text, bytes, NULL, NULL);
    result = cui__copy_text(text, buffer, capacity);
    free(text); free(wide);
    return result;
}
void cui__backend_set_checked(cui_widget *widget, int checked)
{ SendMessageW((HWND)widget->native, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0); }
int cui__backend_get_checked(const cui_widget *widget)
{ return SendMessageW((HWND)widget->native, BM_GETCHECK, 0, 0) == BST_CHECKED; }
void cui__backend_set_enabled(cui_widget *widget, int enabled)
{
    if (widget->kind == CUI_NUMBER) EnableWindow((HWND)widget->aux, enabled);
    if (widget->native) EnableWindow((HWND)widget->native, enabled);
}

cui_size cui__backend_measure(cui_widget *widget)
{
    HWND native = (HWND)widget->native;
    HDC dc = GetDC(native);
    HGDIOBJ old = SelectObject(dc, widget_font(widget));
    int length = GetWindowTextLengthW(native);
    wchar_t *text = (wchar_t *)calloc((size_t)length + 1, sizeof(*text));
    SIZE size = {0, 0};
    TEXTMETRICW metrics = {0};
    GetTextMetricsW(dc, &metrics);
    if (text) { GetWindowTextW(native, text, length + 1); GetTextExtentPoint32W(dc, text, length, &size); free(text); }
    SelectObject(dc, old); ReleaseDC(native, dc);
    cui_size result = {(float)size.cx / widget->window->scale, (float)metrics.tmHeight / widget->window->scale};
    switch (widget->kind) {
    case CUI_ICON: result=(cui_size){widget->icon_size?widget->icon_size:20,widget->icon_size?widget->icon_size:20};break;
    case CUI_BUTTON: case CUI_TOGGLE: {float side=(float)(widget->icon_size?widget->icon_size:20)+16;result.width=widget->icon_only?side:max(80,result.width+36+(widget->icon?side:0));result.height=widget->icon_only?side:max(result.height+20,widget->icon?side:0);break;}
    case CUI_NUMBER: case CUI_ENTRY: case CUI_PASSWORD: case CUI_SEARCH: result.width = 200; result.height += 20; break;
    case CUI_CHECKBOX: case CUI_RADIO: case CUI_SWITCH: result.width += widget->kind == CUI_SWITCH ? 50 : 30; result.height = max(28, result.height + 8); break;
    case CUI_TEXTAREA: case CUI_CODE: result = (cui_size){240, widget->min_height ? widget->min_height : 120}; break;
    case CUI_TREE: result = (cui_size){240, 180}; break;
    case CUI_LIST: result = (cui_size){240, 160}; break;
    case CUI_CHART: case CUI_IMAGE: case CUI_CANVAS: result = (cui_size){260, 160}; break;
    case CUI_TABLE: result = (cui_size){360, 180}; break;
    case CUI_DATE: case CUI_TIME_INPUT: result=(cui_size){200,max(32,result.height+16)};break;
    case CUI_SELECT: result = (cui_size){160, max(32, result.height + 20)}; break;
    case CUI_SLIDER: result = (cui_size){160, 28}; break;
    case CUI_PROGRESS: case CUI_SPINNER: result = (cui_size){120, 8}; break;
    case CUI_SEPARATOR: result = (cui_size){1, 1}; break;
    case CUI_BADGE: result.width += 20; result.height += 8; break;
    default: break;
    }
    return result;
}
void cui__backend_place(cui_widget *widget)
{
    cui_rect r = widget->frame;
    if(cui__container(widget)&&cui__in_layer(widget)&&widget->kind!=CUI_SPLIT){
        RECT rect=native_rect(r,widget->window->scale);
        SetWindowPos((HWND)widget->native,HWND_TOP,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,SWP_NOACTIVATE);return;
    }
    if(widget->kind==CUI_SPLIT){
        if(!widget->first)return;
        if(widget->axis==CUI_HORIZONTAL){r.x=widget->first->frame.x+widget->first->frame.width;r.width=(float)widget->gap;}
        else{r.y=widget->first->frame.y+widget->first->frame.height;r.height=(float)widget->gap;}
        r.x-=widget->window->scroll_x;r.y-=widget->window->scroll_y;RECT rect=native_rect(r,widget->window->scale);
        SetWindowPos((HWND)widget->native,NULL,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,SWP_NOZORDER|SWP_NOACTIVATE);return;
    }
    float height = widget->minimum.height;
    r.y += (r.height - height) / 2;
    r.height = height;
    if (widget->kind == CUI_ENTRY || widget->kind == CUI_PASSWORD || widget->kind == CUI_SEARCH || widget->kind == CUI_NUMBER) { r.x += 10; r.y += 10; r.width -= 20; r.height -= 20; }
    if (widget->kind == CUI_CANVAS || widget->kind == CUI_TEXTAREA || widget->kind == CUI_CODE || widget->kind == CUI_LIST || widget->kind == CUI_TABLE || widget->kind == CUI_TREE) r = widget->frame;
    r.x -= widget->window->scroll_x; r.y -= widget->window->scroll_y;
    if (widget->kind == CUI_SELECT) r.height += 240; /* Combo box dropdown list height. */
    RECT rect = native_rect(r, widget->window->scale);
    if (widget->kind == CUI_NUMBER) {
        int width = pixels(24, widget->window->scale);
        SetWindowPos((HWND)widget->aux, NULL, rect.right-width, rect.top, width, rect.bottom-rect.top, SWP_NOZORDER|SWP_NOACTIVATE);
        rect.right -= width + pixels(4, widget->window->scale);
    }
    SetWindowPos((HWND)widget->native, cui__in_layer(widget)?HWND_TOP:NULL, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                 (cui__in_layer(widget)?0:SWP_NOZORDER) | SWP_NOACTIVATE);
}

void cui__backend_scrollable(cui_window *window)
{
    HWND native = (HWND)window->native;
    LONG_PTR style = GetWindowLongPtrW(native, GWL_STYLE);
    style = window->scrollable ? style | WS_VSCROLL | WS_HSCROLL : style & ~(WS_VSCROLL | WS_HSCROLL);
    SetWindowLongPtrW(native, GWL_STYLE, style);
    SetWindowPos(native, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
void cui__backend_visible(cui_widget *w, int visible)
{ if (w->native) ShowWindow((HWND)w->native, visible ? SW_SHOWNA : SW_HIDE); if (w->kind == CUI_NUMBER) ShowWindow((HWND)w->aux, visible ? SW_SHOWNA : SW_HIDE); }
void cui__backend_min_size(cui_widget *w) { (void)w; }

void cui__backend_items(cui_widget *w)
{
    size_t i;
    HWND native = (HWND)w->native;
    if (w->kind == CUI_TABLE) { cui__win32_table_items(w); return; }
    SendMessageW(native, w->kind == CUI_SELECT ? CB_RESETCONTENT : LB_RESETCONTENT, 0, 0);
    for (i = 0; i < w->item_count; ++i) {
        wchar_t *text = cui__win32_wide(w->items[i]);
        if (text) SendMessageW(native, w->kind == CUI_SELECT ? CB_ADDSTRING : LB_ADDSTRING, 0, (LPARAM)text);
        free(text);
    }
}
void cui__backend_set_selected(cui_widget *w, int index)
{
    if (w->kind == CUI_TABLE) {
        ListView_SetItemState((HWND)w->native, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        if (index >= 0) ListView_SetItemState((HWND)w->native, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    } else SendMessageW((HWND)w->native, w->kind == CUI_SELECT ? CB_SETCURSEL : LB_SETCURSEL, (WPARAM)index, 0);
}
int cui__backend_get_selected(const cui_widget *w)
{
    if (w->kind == CUI_TABLE) return ListView_GetNextItem((HWND)w->native, -1, LVNI_SELECTED);
    return (int)SendMessageW((HWND)w->native, w->kind == CUI_SELECT ? CB_GETCURSEL : LB_GETCURSEL, 0, 0);
}
void cui__backend_set_value(cui_widget *w, double value)
{
    int position = (int)(value * 10000 + 0.5);
    if (w->kind == CUI_SLIDER) SendMessageW((HWND)w->native, TBM_SETPOS, TRUE, position);
    else SendMessageW((HWND)w->native, PBM_SETPOS, (WPARAM)position, 0);
}
double cui__backend_get_value(const cui_widget *w)
{ return (double)SendMessageW((HWND)w->native, w->kind == CUI_SLIDER ? TBM_GETPOS : PBM_GETPOS, 0, 0) / 10000; }
void cui__backend_placeholder(cui_widget *w, const char *text)
{
    if(w->kind==CUI_TEXTAREA){cui__backend_keys(w);InvalidateRect(w->native,NULL,TRUE);return;}
    wchar_t *wide = cui__win32_wide(text);
    if (wide) SendMessageW((HWND)w->native, EM_SETCUEBANNER, FALSE, (LPARAM)wide);
    free(wide);
}
static LRESULT CALLBACK tooltip_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
{
    (void)data;
    if (message == WM_NCDESTROY) {
        free((void *)RemovePropW(hwnd, L"cui-tooltip-text"));
        RemoveWindowSubclass(hwnd, tooltip_proc, id);
    }
    return DefSubclassProc(hwnd, message, wp, lp);
}
void cui__backend_tooltip(cui_widget *w, const char *text)
{
    TOOLINFOW info = {0};
    wchar_t *wide;
    int creating = !w->aux;
    if (!w->native) return;
    wide = cui__win32_wide(text);
    if (!wide) return;
    if (creating) {
        w->aux = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL, WS_POPUP | TTS_ALWAYSTIP,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, (HWND)w->window->native, NULL, GetModuleHandleW(NULL), NULL);
        if (!w->aux) { free(wide); return; }
        SetWindowSubclass((HWND)w->aux, tooltip_proc, 1, 0);
    }
    void *previous = (void *)GetPropW((HWND)w->aux, L"cui-tooltip-text");
    info.cbSize = sizeof(info); info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    info.hwnd = (HWND)w->window->native; info.uId = (UINT_PTR)w->native; info.lpszText = wide;
    SendMessageW((HWND)w->aux, creating ? TTM_ADDTOOLW : TTM_UPDATETIPTEXTW, 0, (LPARAM)&info);
    SetPropW((HWND)w->aux, L"cui-tooltip-text", wide);
    free(previous);
}

static void CALLBACK timer_tick(HWND window, UINT message, UINT_PTR id, DWORD time)
{
    cui_timer *timer;
    (void)window; (void)message; (void)time;
    if (!timer_app) return;
    for (timer = timer_app->timers; timer; timer = timer->next)
        if (timer->active && (UINT_PTR)timer->native == id) { timer->task(timer->userdata); break; }
}
int cui__backend_timer(cui_timer *timer)
{ timer->native = (void *)SetTimer(NULL, 0, timer->interval, timer_tick); return timer->native != NULL; }
void cui__backend_timer_stop(cui_timer *timer)
{ if (timer->native) KillTimer(NULL, (UINT_PTR)timer->native); timer->native = NULL; }
double cui_time(void) { return (double)GetTickCount64() / 1000; }
void cui__backend_media(cui_widget *w) { InvalidateRect((HWND)w->native, NULL, TRUE); }
void cui_clipboard_set_text(cui_window *window, const char *text)
{
    wchar_t *wide = cui__win32_wide(text ? text : "");
    if (!window || !wide) { free(wide); return; }
    size_t bytes = (wcslen(wide) + 1) * sizeof(*wide);
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bytes);
    void *memory = data ? GlobalLock(data) : NULL;
    if (!memory) { if (data) GlobalFree(data); free(wide); return; }
    memcpy(memory, wide, bytes); GlobalUnlock(data); free(wide);
    if (OpenClipboard((HWND)window->native)) {
        if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, data)) data = NULL;
        CloseClipboard();
    }
    if (data) GlobalFree(data);
}

int cui__backend_insert_text(cui_widget *w, const char *text)
{
    wchar_t *value = cui__win32_wide(text);
    if (!value) return 0;
    SendMessageW((HWND)w->native, EM_REPLACESEL, TRUE, (LPARAM)value);
    free(value); return 1;
}

size_t cui_get_selected_text(const cui_widget *w, char *buffer, size_t capacity)
{
    if (w && (w->kind == CUI_CANVAS || w->kind == CUI_BOX)) return cui__chat_selected_text(w, buffer, capacity);
    if (!w || !(w->kind == CUI_ENTRY || w->kind == CUI_SEARCH || w->kind == CUI_TEXTAREA || w->kind == CUI_CODE)) return cui__copy_text("", buffer, capacity);
    DWORD a = 0, z = 0;
    SendMessageW((HWND)w->native, EM_GETSEL, (WPARAM)&a, (LPARAM)&z);
    int length = GetWindowTextLengthW((HWND)w->native);
    wchar_t *wide = (wchar_t *)calloc((size_t)length + 1, sizeof(*wide));
    if (!wide) return cui__copy_text("", buffer, capacity);
    GetWindowTextW((HWND)w->native, wide, length+1);
    if (z > (DWORD)length) z = (DWORD)length;
    if (a > z) a = z;
    wide[z] = 0;
    int bytes = WideCharToMultiByte(CP_UTF8, 0, wide+a, -1, NULL, 0, NULL, NULL);
    char *text = bytes ? (char *)malloc((size_t)bytes) : NULL;
    if (text) WideCharToMultiByte(CP_UTF8, 0, wide+a, -1, text, bytes, NULL, NULL);
    size_t result = cui__copy_text(text ? text : "", buffer, capacity); free(text); free(wide); return result;
}

void cui__backend_font(cui_widget *w)
{
    if (cui__container(w) || !w->window->font) return;
    const char *family; double points; int weight;
    cui__font_resolve(w, &family, &points, &weight);
    LOGFONTW font = {0}; GetObjectW(base_font(w), sizeof(font), &font);
    if (family) {
        wchar_t *wide = cui__win32_wide(family);
        if (wide) { wcsncpy(font.lfFaceName, wide, LF_FACESIZE-1); font.lfFaceName[LF_FACESIZE-1] = 0; free(wide); }
    }
    if (points) font.lfHeight = -pixels((float)(points * 96 / 72), w->window->scale);
    font.lfHeight = -(LONG)(abs(font.lfHeight) * w->window->app->text_scale + 0.5);
    if (weight) font.lfWeight = weight;
    font.lfItalic = (BYTE)cui__font_italic(w);
    HFONT native = CreateFontIndirectW(&font);
    if (native) { HFONT old = (HFONT)w->font_native; w->font_native = native;
        SendMessageW((HWND)w->native, WM_SETFONT, (WPARAM)native, TRUE); if (old) DeleteObject(old); }
}
void cui__backend_font_free(cui_widget *w) { if (w->font_native) DeleteObject((HFONT)w->font_native); }
double cui_window_scale(const cui_window *w) { return w ? w->scale : 1; }

void cui__backend_container(cui_widget *w){(void)w;}
void cui__backend_grid_cell(cui_widget *w){(void)w;}
void cui__backend_split_position(cui_widget *w){if(w->native)InvalidateRect((HWND)w->native,NULL,TRUE);}

void cui__backend_tree_items(cui_widget *w)
{
    cui_tree_model *m = w->payload;
    HWND view = (HWND)w->native;
    SendMessageW(view, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(view);
    for (size_t i = 0; i < m->count; ++i) {
        cui_tree_node *n = m->nodes + i;
        wchar_t *text = cui__win32_wide(n->text);
        TVINSERTSTRUCTW item = {0};
        item.hParent = n->parent < 0 ? TVI_ROOT : (HTREEITEM)m->nodes[n->parent].native;
        item.hInsertAfter = TVI_LAST;
        item.item.mask = TVIF_TEXT | TVIF_PARAM;
        item.item.pszText = text; item.item.lParam = (LPARAM)(i + 1);
        n->native = (void *)SendMessageW(view, TVM_INSERTITEMW, 0, (LPARAM)&item);
        free(text);
    }
    for (size_t i = 0; i < m->count; ++i)
        if (m->nodes[i].expanded) TreeView_Expand(view, (HTREEITEM)m->nodes[i].native, TVE_EXPAND);
    TreeView_SelectItem(view, NULL);
    SendMessageW(view, WM_SETREDRAW, TRUE, 0); InvalidateRect(view, NULL, TRUE);
}
void cui__backend_tree_select(cui_widget *w, cui_tree_node *node)
{
    TreeView_SelectItem((HWND)w->native, node ? (HTREEITEM)node->native : NULL);
    if (node) TreeView_EnsureVisible((HWND)w->native, (HTREEITEM)node->native);
}
void cui__backend_tree_expand(cui_widget *w, cui_tree_node *node)
{ TreeView_Expand((HWND)w->native, (HTREEITEM)node->native, node->expanded ? TVE_EXPAND : TVE_COLLAPSE); }

void cui__backend_number(cui_widget *w)
{
    cui_number_state *s = w->payload; char text[96];
    snprintf(text, sizeof(text), "%.*f", (int)s->digits, w->value);
    SetWindowTextA((HWND)w->native, text);
}
void cui__backend_invalid(cui_widget *w, int invalid)
{
    /* The shared field displays and describes the error. Native keyboard focus
       and selection remain the standard EDIT behavior. */
    (void)w; (void)invalid;
}

void cui__backend_datetime(cui_widget *w)
{
    cui_datetime_state *s=w->payload;SYSTEMTIME value={0};
    value.wYear=(WORD)s->date.year;value.wMonth=(WORD)s->date.month;value.wDay=(WORD)s->date.day;
    value.wHour=(WORD)s->time.hour;value.wMinute=(WORD)s->time.minute;value.wSecond=(WORD)s->time.second;
    DateTime_SetSystemtime((HWND)w->native,GDT_VALID,&value);
}

int cui__backend_window_frame(cui_window *w)
{
    HWND hwnd=(HWND)w->native;
    LONG_PTR style=GetWindowLongPtrW(hwnd,GWL_STYLE);
    style &= ~(LONG_PTR)(WS_CAPTION|WS_THICKFRAME|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX);
    if(w->decorated) style |= WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    if(w->resizable) style |= WS_THICKFRAME|WS_MAXIMIZEBOX;
    SetWindowLongPtrW(hwnd,GWL_STYLE,style);
    if(w->corner_radius>0 && !w->decorated) {
        double scale=cui_window_scale(w);
        HRGN shape=CreateRoundRectRgn(0,0,(int)(w->width*scale)+1,(int)(w->height*scale)+1,
                                      (int)(w->corner_radius*scale*2),(int)(w->corner_radius*scale*2));
        if(!shape)return 0;
        if(!SetWindowRgn(hwnd,shape,TRUE)){DeleteObject(shape);return 0;}
    } else SetWindowRgn(hwnd,NULL,TRUE);
    SetWindowPos(hwnd,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
    return 1;
}
int cui__backend_window_size(cui_window *w)
{
    HWND hwnd=(HWND)w->native; double scale=cui_window_scale(w);
    RECT rect={0,0,(int)(w->width*scale),(int)(w->height*scale)};
    AdjustWindowRectExForDpi(&rect,(DWORD)GetWindowLongPtrW(hwnd,GWL_STYLE),
                             w->menu!=NULL,(DWORD)GetWindowLongPtrW(hwnd,GWL_EXSTYLE),GetDpiForWindow(hwnd));
    if(!SetWindowPos(hwnd,NULL,0,0,rect.right-rect.left,rect.bottom-rect.top,
                     SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE))return 0;
    return cui__backend_window_frame(w);
}
int cui__backend_window_position(cui_window *w,int x,int y)
{
    double scale=cui_window_scale(w);
    return SetWindowPos((HWND)w->native,NULL,(int)(x*scale),(int)(y*scale),0,0,
                         SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE)!=0;
}
int cui__backend_window_move(cui_window *w)
{
    if(!w->visible || !(GetKeyState(VK_LBUTTON)&0x8000))return 0;
    ReleaseCapture(); SendMessageW((HWND)w->native,WM_NCLBUTTONDOWN,HTCAPTION,0);return 1;
}

int cui__backend_window_get_size(cui_window *w,int *width,int *height)
{
    RECT r; if(!GetClientRect((HWND)w->native,&r))return 0;
    double scale=cui_window_scale(w);
    *width=(int)((r.right-r.left)/scale); *height=(int)((r.bottom-r.top)/scale);
    return *width>0 && *height>0;
}
int cui__backend_window_resize(cui_window *w,int corner)
{
    if(!(GetKeyState(VK_LBUTTON)&0x8000))return 0;
    const int edges[]={WMSZ_TOPLEFT,WMSZ_TOPRIGHT,WMSZ_BOTTOMLEFT,WMSZ_BOTTOMRIGHT};
    ReleaseCapture(); SendMessageW((HWND)w->native,WM_SYSCOMMAND,SC_SIZE|edges[corner],0);
    return 1;
}

int cui__backend_window_anchor(cui_window *w)
{
    HWND parent=(HWND)w->anchor_parent->native;
    SetWindowLongPtrW((HWND)w->native,GWLP_HWNDPARENT,(LONG_PTR)parent);
    double scale=cui_window_scale(w->anchor_parent);
    POINT point={(LONG)((w->anchor_x+w->anchor_width+16)*scale),(LONG)(w->anchor_y*scale)};
    if(w->popup){point.x=(LONG)(w->anchor_x*scale);point.y=(LONG)((w->anchor_y+w->anchor_height)*scale);}
    ClientToScreen(parent,&point);
    if(w->popup){
        MONITORINFO info={sizeof(info)};RECT frame;GetWindowRect((HWND)w->native,&frame);
        if(GetMonitorInfoW(MonitorFromWindow(parent,MONITOR_DEFAULTTONEAREST),&info)){
            LONG width=frame.right-frame.left,height=frame.bottom-frame.top;
            if(point.y+height>info.rcWork.bottom)point.y-=(LONG)(w->anchor_height*scale)+height;
            point.x=max(info.rcWork.left,min(point.x,info.rcWork.right-width));
            point.y=max(info.rcWork.top,min(point.y,info.rcWork.bottom-height));
        }
    }
    return SetWindowPos((HWND)w->native,NULL,point.x,point.y,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER)!=0;
}

int cui_widget_get_size(const cui_widget *w, int *width, int *height)
{
    if (!w || !width || !height) return 0;
    int x = (int)w->frame.width, y = (int)w->frame.height;
    if (x <= 0 || y <= 0) return 0;
    *width = x; *height = y; return 1;
}

int cui__backend_popup_anchor(cui_window *panel,cui_widget *anchor,double x,double y,double width,double height)
{
    panel->anchor_parent=anchor->window;
    panel->anchor_x=(int)floor(anchor->frame.x+x-anchor->window->scroll_x);
    panel->anchor_y=(int)floor(anchor->frame.y+y-anchor->window->scroll_y);
    panel->anchor_width=(int)ceil(width);panel->anchor_height=(int)ceil(height);
    return cui__backend_window_anchor(panel);
}
