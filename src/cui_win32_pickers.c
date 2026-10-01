#include "cui_desktop_internal.h"
#include "cui_win32_tables.h"
#include <windows.h>
#include <commdlg.h>
#include <stdlib.h>
#include <wchar.h>
static void initialize_picker(HWND hwnd,cui_dialog *d)
{
    d->native=hwnd;
    wchar_t *title=cui__win32_wide(d->title);
    if(title){SetWindowTextW(hwnd,title);free(title);}
    if(d->cancelled)PostMessageW(hwnd,WM_COMMAND,IDCANCEL,0);
}
static UINT_PTR CALLBACK color_hook(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    (void)wp;
    if(message==WM_INITDIALOG)initialize_picker(hwnd,(cui_dialog *)((CHOOSECOLORW *)lp)->lCustData);
    return 0;
}
static UINT_PTR CALLBACK font_hook(HWND hwnd,UINT message,WPARAM wp,LPARAM lp)
{
    (void)wp;
    if(message==WM_INITDIALOG)initialize_picker(hwnd,(cui_dialog *)((CHOOSEFONTW *)lp)->lCustData);
    return 0;
}
static BOOL choose_color(cui_dialog *d)
{
    COLORREF custom[16]={0};CHOOSECOLORW cc={0};cc.lStructSize=sizeof(cc);
    cc.hwndOwner=(HWND)d->parent->native;cc.rgbResult=RGB(d->color>>16,(d->color>>8)&255,d->color&255);
    cc.lpCustColors=custom;cc.Flags=CC_RGBINIT|CC_FULLOPEN|CC_ENABLEHOOK;cc.lpfnHook=color_hook;cc.lCustData=(LPARAM)d;
    BOOL accepted=ChooseColorW(&cc);
    if(accepted)d->color=(GetRValue(cc.rgbResult)<<16)|(GetGValue(cc.rgbResult)<<8)|GetBValue(cc.rgbResult);
    return accepted;
}
static BOOL choose_font(cui_dialog *d)
{
    LOGFONTW font={0};wchar_t *family=cui__win32_wide(d->font.family);
    if(family){wcsncpy(font.lfFaceName,family,LF_FACESIZE-1);free(family);}
    HDC dc=GetDC((HWND)d->parent->native);int dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC((HWND)d->parent->native,dc);
    font.lfHeight=-(int)(d->font.points*dpi/72+0.5);font.lfWeight=d->font.weight;font.lfItalic=(BYTE)d->font.italic;
    CHOOSEFONTW cf={0};cf.lStructSize=sizeof(cf);cf.hwndOwner=(HWND)d->parent->native;cf.lpLogFont=&font;
    cf.iPointSize=(int)(d->font.points*10+0.5);cf.nSizeMin=6;cf.nSizeMax=200;
    cf.Flags=CF_SCREENFONTS|CF_INITTOLOGFONTSTRUCT|CF_ENABLEHOOK|CF_LIMITSIZE;
    cf.lpfnHook=font_hook;cf.lCustData=(LPARAM)d;
    BOOL accepted=ChooseFontW(&cf);
    if(accepted){
        WideCharToMultiByte(CP_UTF8,0,font.lfFaceName,-1,d->font.family,sizeof(d->font.family),NULL,NULL);
        d->font.points=cf.iPointSize/10.0;d->font.weight=font.lfWeight;d->font.italic=!!font.lfItalic;
    }
    return accepted;
}
void cui__picker_open(cui_dialog *d)
{
    BOOL accepted=d->kind==CUI_DIALOG_COLOR?choose_color(d):choose_font(d);
    DWORD error=accepted?0:CommDlgExtendedError();d->native=NULL;
    cui_dialog_result result=accepted?CUI_DIALOG_ACCEPTED:error?CUI_DIALOG_FAILED:CUI_DIALOG_CANCELLED;
    if(accepted && d->kind==CUI_DIALOG_FONT && !cui__font_valid(&d->font))result=CUI_DIALOG_FAILED;
    cui__desktop_finish(d,result,"");
}
void cui__picker_cancel(cui_dialog *d)
{ SendMessageW((HWND)d->native,WM_COMMAND,IDCANCEL,0); }
