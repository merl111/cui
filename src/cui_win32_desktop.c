#define COBJMACROS
#include "cui_desktop_internal.h"
#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <richedit.h>
#include <oleacc.h>
#include <stdlib.h>
#include <wchar.h>
static wchar_t *wide(const char *text)
{int n=MultiByteToWideChar(CP_UTF8,0,text,-1,NULL,0);wchar_t *p=calloc((size_t)n,sizeof(*p));if(p)MultiByteToWideChar(CP_UTF8,0,text,-1,p,n);return p;}
int cui_focus(cui_widget *w)
{if(!w||!w->native)return 0;for(cui_widget *p=w;p;p=p->parent)if(p->hidden||!p->enabled)return 0;SetFocus((HWND)w->native);return GetFocus()==(HWND)w->native;}
int cui_has_focus(const cui_widget *w){return w&&GetFocus()==(HWND)w->native;}
void cui_accessibility(cui_widget *w,const char *label,const char *description)
{
    /* Native text already supplies the MSAA name. Accessible property overrides
       use the system accessibility service rather than changing visible labels. */
    if(!w||!w->native)return;
    IAccPropServices *services=NULL;
    HRESULT hr=CoCreateInstance(&CLSID_AccPropServices,NULL,CLSCTX_INPROC_SERVER,&IID_IAccPropServices,(void **)&services);
    if(SUCCEEDED(hr)){wchar_t *text=wide(label?label:"");if(text){IAccPropServices_SetHwndPropStr(services,(HWND)w->native,OBJID_CLIENT,CHILDID_SELF,PROPID_ACC_NAME,text);free(text);}text=wide(description?description:"");if(text){IAccPropServices_SetHwndPropStr(services,(HWND)w->native,OBJID_CLIENT,CHILDID_SELF,PROPID_ACC_DESCRIPTION,text);free(text);}IAccPropServices_Release(services);}
}
void cui_set_read_only(cui_widget *w,int value)
{if(w&&(w->kind==CUI_ENTRY||w->kind==CUI_PASSWORD||w->kind==CUI_SEARCH||w->kind==CUI_TEXTAREA)){w->read_only=!!value;SendMessageW((HWND)w->native,EM_SETREADONLY,value,0);}}
void cui_undo(cui_widget *w){if(w)SendMessageW((HWND)w->native,EM_UNDO,0,0);}
void cui_redo(cui_widget *w){if(w)SendMessageW((HWND)w->native,w->kind==CUI_TEXTAREA?EM_REDO:EM_UNDO,0,0);}
static HRESULT CALLBACK alert_event(HWND hwnd,UINT event,WPARAM wp,LPARAM lp,LONG_PTR data)
{(void)wp;(void)lp;cui_dialog *d=(cui_dialog *)data;if(event==TDN_CREATED)d->native=hwnd;if(event==TDN_DESTROYED)d->native=NULL;return S_OK;}
void cui__desktop_open(cui_dialog *d)
{
    if(d->kind==CUI_DIALOG_COLOR || d->kind==CUI_DIALOG_FONT){cui__picker_open(d);return;}
    if(d->kind==CUI_DIALOG_ALERT){
        wchar_t *title=wide(d->title),*message=wide(d->message),*accept=wide(d->accept);int result=IDCANCEL;
        TASKDIALOG_BUTTON button={IDOK,accept};TASKDIALOGCONFIG config={0};config.cbSize=sizeof(config);config.hwndParent=(HWND)d->parent->native;
        config.dwFlags=TDF_ALLOW_DIALOG_CANCELLATION|TDF_SIZE_TO_CONTENT;config.dwCommonButtons=TDCBF_CANCEL_BUTTON;
        config.pszWindowTitle=title;config.pszMainInstruction=title;config.pszContent=message;config.cButtons=1;config.pButtons=&button;config.nDefaultButton=IDOK;
        config.pfCallback=alert_event;config.lpCallbackData=(LONG_PTR)d;
        HRESULT hr=TaskDialogIndirect(&config,&result,NULL,NULL);free(title);free(message);free(accept);d->native=NULL;
        cui__desktop_finish(d,FAILED(hr)?CUI_DIALOG_FAILED:result==IDOK?CUI_DIALOG_ACCEPTED:CUI_DIALOG_CANCELLED,"");return;
    }
    cui__file_open_native(d);
}
void cui__desktop_cancel(cui_dialog *d)
{
    if(!d->native)return;
    if(d->kind==CUI_DIALOG_COLOR || d->kind==CUI_DIALOG_FONT){cui__picker_cancel(d);return;}
    if(d->kind==CUI_DIALOG_ALERT)SendMessageW((HWND)d->native,TDM_CLICK_BUTTON,IDCANCEL,0);
    else IFileDialog_Close((IFileDialog *)d->native,HRESULT_FROM_WIN32(ERROR_CANCELLED));
}
void cui__desktop_command(cui_command *c){(void)c;}
void cui__desktop_window(cui_window *w){(void)w;}
static HMENU native_menu(cui_menu *m,int popup)
{
    HMENU native=popup?CreatePopupMenu():CreateMenu();
    for(size_t i=0;i<m->count;++i){cui_menu_item *item=m->items+i;
        if(item->submenu){wchar_t *text=wide(item->label);AppendMenuW(native,MF_POPUP,(UINT_PTR)native_menu(item->submenu,1),text);free(text);}
        else if(item->command){cui_command *c=item->command;wchar_t *text=wide(c->label);AppendMenuW(native,MF_STRING|(c->enabled?0:MF_GRAYED)|(c->checked?MF_CHECKED:0),c->id,text);free(text);}
        else AppendMenuW(native,MF_SEPARATOR,0,NULL);
    }return native;
}
void cui__desktop_menu(cui_window *w)
{
    HMENU old=(HMENU)w->menu_native;
    w->menu_native=w->menu?native_menu(w->menu,0):NULL;SetMenu((HWND)w->native,(HMENU)w->menu_native);DrawMenuBar((HWND)w->native);
    if(old)DestroyMenu(old);cui__backend_refresh(w);
}
void cui_menu_popup(cui_menu *m,cui_widget *anchor)
{
    if(!m||!anchor||m->app!=anchor->window->app)return;
    RECT r;GetWindowRect((HWND)anchor->native,&r);HMENU native=native_menu(m,1);
    UINT id=(UINT)TrackPopupMenu(native,TPM_RETURNCMD|TPM_NONOTIFY,r.left,r.bottom,0,(HWND)anchor->window->native,NULL);DestroyMenu(native);
    for(cui_command *c=m->app->commands;c;c=c->next)if(c->id==id){cui_command_invoke(c);break;}
}
void cui__desktop_dispose(cui_app *app)
{
    for(cui_window *w=app->windows;w;w=w->next)if(w->menu_native){SetMenu((HWND)w->native,NULL);DestroyMenu((HMENU)w->menu_native);w->menu_native=NULL;}
}
int cui__desktop_key(cui_window *w,unsigned key,unsigned mods)
{
    for(cui_command *c=w->app->commands;c;c=c->next){unsigned expected=(c->modifiers&~CUI_MOD_PRIMARY)|((c->modifiers&CUI_MOD_PRIMARY)?CUI_MOD_CONTROL:0);
        if(c->key&&c->key==key&&expected==mods)return cui_command_invoke(c);}
    return 0;
}
int cui__desktop_command_id(cui_window *w,unsigned id)
{for(cui_command *c=w->app->commands;c;c=c->next)if(c->id==id)return cui_command_invoke(c);return 0;}
