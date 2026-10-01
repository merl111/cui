#define COBJMACROS
#include "cui_desktop_internal.h"
#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
wchar_t *cui__win32_wide(const char *text);
typedef struct file_types {COMDLG_FILTERSPEC *specs;size_t count;} file_types;
static void types_free(file_types *types)
{
    for(size_t i=0;i<types->count;++i){free((void *)types->specs[i].pszName);free((void *)types->specs[i].pszSpec);}
    free(types->specs);
}
static wchar_t *extension_pattern(cui_file_type *type)
{
    size_t n=2;for(size_t i=0;i<type->count;++i)n+=strlen(type->extensions[i])+3;
    wchar_t *pattern=calloc(n,sizeof(*pattern));if(!pattern)return NULL;
    wchar_t *p=pattern;
    if(!type->count)*p++=L'*';
    for(size_t i=0;i<type->count;++i){
        if(i)*p++=L';';
        *p++=L'*';*p++=L'.';
        for(const char *s=type->extensions[i];*s;++s)*p++=(wchar_t)*s;
    }
    return pattern;
}
static HRESULT set_filters(IFileDialog *dialog,cui_file_settings *settings,file_types *types)
{
    if(!settings||!settings->count)return S_OK;
    types->specs=calloc(settings->count,sizeof(*types->specs));if(!types->specs)return E_OUTOFMEMORY;
    types->count=settings->count;
    for(size_t i=0;i<types->count;++i){
        types->specs[i].pszName=cui__win32_wide(settings->filters[i].name);
        types->specs[i].pszSpec=extension_pattern(settings->filters+i);
        if(!types->specs[i].pszName||!types->specs[i].pszSpec)return E_OUTOFMEMORY;
    }
    HRESULT hr=IFileDialog_SetFileTypes(dialog,(UINT)types->count,types->specs);
    return SUCCEEDED(hr)?IFileDialog_SetFileTypeIndex(dialog,(UINT)settings->initial+1):hr;
}
static void set_folder(IFileDialog *dialog,const wchar_t *path)
{
    IShellItem *item=NULL;
    if(SUCCEEDED(SHCreateItemFromParsingName(path,NULL,&IID_IShellItem,(void **)&item))){IFileDialog_SetFolder(dialog,item);IShellItem_Release(item);}
}
static void initial_path(IFileDialog *dialog,const char *text)
{
    if(!*text)return;
    wchar_t *hint=cui__win32_wide(text);if(!hint)return;
    DWORD length=GetFullPathNameW(hint,0,NULL,NULL);
    wchar_t *path=length?calloc(length,sizeof(*path)):NULL;
    if(!path||!GetFullPathNameW(hint,length,path,NULL)){free(hint);free(path);return;}
    free(hint);DWORD attributes=GetFileAttributesW(path);
    if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY))set_folder(dialog,path);
    else{
        wchar_t *base=wcsrchr(path,L'\\'),*forward=wcsrchr(path,L'/');
        if(!base||(forward&&forward>base))base=forward;
        if(base){IFileDialog_SetFileName(dialog,base+1);wchar_t saved=base[1];base[1]=0;set_folder(dialog,path);base[1]=saved;}
        else IFileDialog_SetFileName(dialog,path);
    }
    free(path);
}
static char *item_path(IShellItem *item)
{
    wchar_t *name=NULL;if(FAILED(IShellItem_GetDisplayName(item,SIGDN_FILESYSPATH,&name)))return NULL;
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name,-1,NULL,0,NULL,NULL);
    char *path=n?malloc((size_t)n):NULL;
    if(path)WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name,-1,path,n,NULL,NULL);
    CoTaskMemFree(name);return path;
}
static char **results(IFileDialog *dialog,cui_dialog_kind kind,size_t *count)
{
    if(kind==CUI_DIALOG_SAVE){
        IShellItem *item=NULL;if(FAILED(IFileDialog_GetResult(dialog,&item)))return NULL;
        char **paths=calloc(1,sizeof(*paths));if(paths){paths[0]=item_path(item);*count=1;}IShellItem_Release(item);return paths;
    }
    IFileOpenDialog *open=NULL;IShellItemArray *items=NULL;DWORD n=0;
    HRESULT hr=IFileDialog_QueryInterface(dialog,&IID_IFileOpenDialog,(void **)&open);
    if(SUCCEEDED(hr)){hr=IFileOpenDialog_GetResults(open,&items);IFileOpenDialog_Release(open);}
    if(FAILED(hr))return NULL;
    hr=IShellItemArray_GetCount(items,&n);char **paths=SUCCEEDED(hr)&&n&&n<=65536?calloc(n,sizeof(*paths)):NULL;
    if(paths){
        *count=n;
        for(DWORD i=0;i<n;++i){IShellItem *item=NULL;if(FAILED(IShellItemArray_GetItemAt(items,i,&item)))break;paths[i]=item_path(item);IShellItem_Release(item);}
    }
    IShellItemArray_Release(items);return paths;
}
static HRESULT configure(IFileDialog *dialog,cui_dialog *d,file_types *types)
{
    FILEOPENDIALOGOPTIONS options=0;HRESULT hr=IFileDialog_GetOptions(dialog,&options);if(FAILED(hr))return hr;
    options|=FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR|FOS_PATHMUSTEXIST;
    if(d->kind==CUI_DIALOG_FOLDER)options|=FOS_PICKFOLDERS;
    if(d->kind==CUI_DIALOG_OPEN)options|=FOS_FILEMUSTEXIST;
    if(d->kind==CUI_DIALOG_SAVE)options|=FOS_OVERWRITEPROMPT;
    if(d->files&&d->files->multiple)options|=FOS_ALLOWMULTISELECT;
    hr=IFileDialog_SetOptions(dialog,options);if(FAILED(hr))return hr;
    wchar_t *title=cui__win32_wide(d->title);if(!title)return E_OUTOFMEMORY;
    hr=IFileDialog_SetTitle(dialog,title);free(title);if(FAILED(hr))return hr;
    initial_path(dialog,d->path);return set_filters(dialog,d->files,types);
}
void cui__file_open_native(cui_dialog *d)
{
    IFileDialog *dialog=NULL;file_types types={0};char **paths=NULL;size_t count=0;UINT selected=0;
    HRESULT initialized=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    HRESULT hr=CoCreateInstance(d->kind==CUI_DIALOG_SAVE?&CLSID_FileSaveDialog:&CLSID_FileOpenDialog,NULL,CLSCTX_INPROC_SERVER,&IID_IFileDialog,(void **)&dialog);
    if(SUCCEEDED(hr))hr=configure(dialog,d,&types);
    if(SUCCEEDED(hr)){d->native=dialog;hr=IFileDialog_Show(dialog,(HWND)d->parent->native);d->native=NULL;}
    if(SUCCEEDED(hr)){paths=results(dialog,d->kind,&count);IFileDialog_GetFileTypeIndex(dialog,&selected);}
    if(dialog)IFileDialog_Release(dialog);
    types_free(&types);if(SUCCEEDED(initialized))CoUninitialize();
    cui_dialog_result result=SUCCEEDED(hr)?CUI_DIALOG_ACCEPTED:hr==HRESULT_FROM_WIN32(ERROR_CANCELLED)?CUI_DIALOG_CANCELLED:CUI_DIALOG_FAILED;
    cui__file_finish(d,result,(const char *const *)paths,count,selected?selected-1:(size_t)-1);cui__free_strings(paths,count);
}
