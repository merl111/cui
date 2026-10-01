#include "cui_desktop_internal.h"
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
static cui_key key_value(WPARAM key)
{
    switch(key){
    case VK_RETURN:return CUI_KEY_ENTER;case VK_ESCAPE:return CUI_KEY_ESCAPE;
    case VK_BACK:return CUI_KEY_BACKSPACE;case VK_TAB:return CUI_KEY_TAB;
    case VK_UP:return CUI_KEY_UP;case VK_DOWN:return CUI_KEY_DOWN;
    case VK_HOME:return CUI_KEY_HOME;case VK_END:return CUI_KEY_END;
    case VK_PRIOR:return CUI_KEY_PAGE_UP;case VK_NEXT:return CUI_KEY_PAGE_DOWN;
    default:return 0;
    }
}
static LRESULT CALLBACK input_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    cui_widget *w=(cui_widget *)data;
    if(message==WM_PAINT&&w->kind==CUI_TEXTAREA&&w->placeholder&&!GetWindowTextLengthW(hwnd)&&!w->composing){
        LRESULT result=DefSubclassProc(hwnd,message,wp,lp);wchar_t *text=cui__win32_wide(w->placeholder);
        if(text){HDC dc=GetDC(hwnd);RECT rect;GetClientRect(hwnd,&rect);InflateRect(&rect,-8,-6);HGDIOBJ old=SelectObject(dc,(HFONT)SendMessageW(hwnd,WM_GETFONT,0,0));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,GetSysColor(COLOR_GRAYTEXT));DrawTextW(dc,text,-1,&rect,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,old);ReleaseDC(hwnd,dc);free(text);}return result;
    }
    if(message==WM_IME_STARTCOMPOSITION)w->composing=1;
    if(message==WM_IME_ENDCOMPOSITION)w->composing=0;
    if(message==WM_NCDESTROY)RemoveWindowSubclass(hwnd,input_proc,id);
    return DefSubclassProc(hwnd,message,wp,lp);
}
/* Dispatch before IsDialogMessage/TranslateMessage, so consumed Enter does not
 * enqueue WM_CHAR and unconsumed Tab retains native focus traversal. */
int cui__win32_key_message(MSG *message)
{
    if(message->message!=WM_KEYDOWN && message->message!=WM_SYSKEYDOWN)return 0;
    DWORD_PTR data=0;
    if(!GetWindowSubclass(message->hwnd,input_proc,9,&data))return 0;
    if(GetKeyState(VK_LWIN)<0||GetKeyState(VK_RWIN)<0)return 0;
    cui_key key=key_value(message->wParam);unsigned mods=0;
    if(GetKeyState(VK_SHIFT)<0)mods|=CUI_MOD_SHIFT;
    if(GetKeyState(VK_CONTROL)<0)mods|=CUI_MOD_CONTROL|CUI_MOD_PRIMARY;
    if(GetKeyState(VK_MENU)<0)mods|=CUI_MOD_ALT;
    return key&&cui__key((cui_widget *)data,key,mods);
}
int cui__backend_keys(cui_widget *w)
{return SetWindowSubclass((HWND)w->native,input_proc,9,(DWORD_PTR)w)!=0;}
int cui__backend_hover(const cui_widget *w)
{
    POINT point;if(!GetCursorPos(&point))return 0;
    HWND window=(HWND)w->window->native;
    HWND hit=WindowFromPoint(point);
    if(hit!=window&&!IsChild(window,hit))return 0;
    ScreenToClient(window,&point);
    float x=(float)point.x/w->window->scale+w->window->scroll_x,y=(float)point.y/w->window->scale+w->window->scroll_y;
    return x>=w->frame.x&&y>=w->frame.y&&x<w->frame.x+w->frame.width&&y<w->frame.y+w->frame.height;
}
void cui__backend_announce(cui_widget *w,const char *text,int urgent)
{
    cui_accessibility(w,text,"");
    NotifyWinEvent(urgent?EVENT_SYSTEM_ALERT:EVENT_OBJECT_NAMECHANGE,(HWND)w->native,OBJID_CLIENT,CHILDID_SELF);
}

char *cui__search_key(const char *text)
{
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,NULL,0);if(!count)return NULL;
    wchar_t *wide=malloc((size_t)count*sizeof(*wide));if(!wide)return NULL;
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,wide,count);
    int n=LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,wide,-1,NULL,0,NULL,NULL,0);
    wchar_t *folded=n?malloc((size_t)n*sizeof(*folded)):NULL;
    if(!folded){free(wide);return NULL;}
    LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,wide,-1,folded,n,NULL,NULL,0);free(wide);
    int bytes=WideCharToMultiByte(CP_UTF8,0,folded,-1,NULL,0,NULL,NULL);char *out=bytes?malloc((size_t)bytes):NULL;
    if(out)WideCharToMultiByte(CP_UTF8,0,folded,-1,out,bytes,NULL,NULL);free(folded);return out;
}
