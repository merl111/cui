#include "cui_win32_tables.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef struct cell_editor { cui_widget *widget; size_t row,column; int closing; } cell_editor;
static char *editor_text(HWND editor)
{
    int length=GetWindowTextLengthW(editor);
    wchar_t *wide=calloc((size_t)length+1,sizeof(*wide));if(!wide)return NULL;
    GetWindowTextW(editor,wide,length+1);
    int size=WideCharToMultiByte(CP_UTF8,0,wide,-1,NULL,0,NULL,NULL);
    char *text=size?malloc((size_t)size):NULL;
    if(text)WideCharToMultiByte(CP_UTF8,0,wide,-1,text,size,NULL,NULL);
    free(wide);return text;
}
static void finish_edit(HWND editor,int commit)
{
    cell_editor *context=(cell_editor *)GetWindowLongPtrW(editor,GWLP_USERDATA);
    if(!context||context->closing)return;
    context->closing=1;
    cui_widget *w=context->widget;size_t row=context->row,column=context->column;
    char *text=commit&&!w->window->app->destroying?editor_text(editor):NULL;
    ((cui_table_state *)w->payload)->editor=NULL;DestroyWindow(editor);
    if(text){cui__table_edit(w,row,column,text);free(text);}
}
static LRESULT CALLBACK edit_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    (void)id;(void)data;
    if(message==WM_GETDLGCODE)return DLGC_WANTALLKEYS;
    if(message==WM_KEYDOWN&&(wp==VK_RETURN||wp==VK_ESCAPE)){
        cell_editor *context=(cell_editor *)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
        HWND table=(HWND)context->widget->native;
        finish_edit(hwnd,wp==VK_RETURN);SetFocus(table);return 0;
    }
    if(message==WM_KILLFOCUS){finish_edit(hwnd,1);return 0;}
    if(message==WM_NCDESTROY){
        cell_editor *context=(cell_editor *)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
        if(context&&((cui_table_state *)context->widget->payload)->editor==hwnd)((cui_table_state *)context->widget->payload)->editor=NULL;
        free(context);SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);RemoveWindowSubclass(hwnd,edit_proc,0);
    }
    return DefSubclassProc(hwnd,message,wp,lp);
}
static void start_edit(cui_widget *w,size_t row,size_t column)
{
    cui_table_state *s=w->payload;
    if(!s||row>=s->rows||column>=w->columns||!s->editable[column]||!IsWindowEnabled((HWND)w->native))return;
    if(s->editor)finish_edit((HWND)s->editor,1);
    if(row>=s->rows)return;
    RECT rect;ListView_GetSubItemRect((HWND)w->native,(int)row,(int)column,LVIR_BOUNDS,&rect);
    if(column==0)rect.right=rect.left+ListView_GetColumnWidth((HWND)w->native,0);
    cell_editor *context=calloc(1,sizeof(*context));if(!context)return;
    *context=(cell_editor){w,row,column,0};
    wchar_t *text=cui__win32_wide(w->items[row*w->columns+column]);
    HWND editor=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",text?text:L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,
        rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,(HWND)w->native,NULL,GetModuleHandleW(NULL),NULL);
    free(text);if(!editor){free(context);return;}
    s->editor=editor;SetWindowLongPtrW(editor,GWLP_USERDATA,(LONG_PTR)context);
    SetWindowSubclass(editor,edit_proc,0,0);
    SendMessageW(editor,WM_SETFONT,SendMessageW((HWND)w->native,WM_GETFONT,0,0),TRUE);
    SetFocus(editor);SendMessageW(editor,EM_SETSEL,0,-1);
}
static LRESULT CALLBACK table_proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
    (void)id;cui_widget *w=(cui_widget *)data;
    if(message==WM_GETDLGCODE&&lp&&(((MSG *)lp)->wParam==VK_F2||((MSG *)lp)->wParam==VK_RETURN))
        return DefSubclassProc(hwnd,message,wp,lp)|DLGC_WANTMESSAGE;
    if(message==WM_KEYDOWN&&(wp==VK_F2||wp==VK_RETURN)){
        cui_table_state *s=w->payload;
        int row=ListView_GetNextItem(hwnd,-1,LVNI_SELECTED);
        if(s&&row>=0)for(size_t column=0;column<w->columns;++column)if(s->editable[column]){start_edit(w,(size_t)row,column);break;}
        return 0;
    }
    if(message==WM_LBUTTONDBLCLK){
        LVHITTESTINFO hit={0};hit.pt.x=(short)LOWORD(lp);hit.pt.y=(short)HIWORD(lp);
        if(ListView_SubItemHitTest(hwnd,&hit)>=0){start_edit(w,(size_t)hit.iItem,(size_t)hit.iSubItem);return 0;}
    }
    if(message==WM_NCDESTROY)RemoveWindowSubclass(hwnd,table_proc,0);
    return DefSubclassProc(hwnd,message,wp,lp);
}
void cui__win32_table_init(cui_widget *w)
{
    ListView_SetExtendedListViewStyle((HWND)w->native,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    SetWindowSubclass((HWND)w->native,table_proc,0,(DWORD_PTR)w);
}
static void read_selection(cui_widget *w)
{
    if(w->updating)return;
    cui_table_state *s=w->payload;if(s->rows)memset(s->selected,0,s->rows);
    int row=-1;
    while((row=ListView_GetNextItem((HWND)w->native,row,LVNI_SELECTED))>=0)if((size_t)row<s->rows)s->selected[row]=1;
    cui__table_selection_changed(w);
}
LRESULT cui__win32_table_notify(cui_widget *w,NMHDR *header)
{
    cui_table_state *s=w->payload;if(!s)return 0;
    if(header->code==LVN_GETDISPINFOW){
        LVITEMW *item=&((NMLVDISPINFOW *)header)->item;
        if((item->mask&LVIF_TEXT)&&item->pszText&&item->cchTextMax>0){
            item->pszText[0]=0;
            if(item->iItem>=0&&(size_t)item->iItem<s->rows&&item->iSubItem>=0&&(size_t)item->iSubItem<w->columns){
                wchar_t *text=cui__win32_wide(w->items[(size_t)item->iItem*w->columns+(size_t)item->iSubItem]);
                if(text){wcsncpy(item->pszText,text,(size_t)item->cchTextMax-1);item->pszText[item->cchTextMax-1]=0;free(text);}
            }
        }
    }else if(header->code==LVN_COLUMNCLICK){
        int column=((NMLISTVIEW *)header)->iSubItem;
        if(column>=0)cui__table_sort(w,(size_t)column,s->sort_column==column?!s->descending:0);
    }else if(header->code==LVN_ODSTATECHANGED)read_selection(w);
    else if(header->code==LVN_ITEMCHANGED){
        NMLISTVIEW *change=(NMLISTVIEW *)header;
        if((change->uChanged&LVIF_STATE)&&((change->uOldState^change->uNewState)&LVIS_SELECTED))read_selection(w);
    }
    return 0;
}
void cui__win32_table_items(cui_widget *w)
{
    cui_table_state *s=w->payload;HWND native=(HWND)w->native;
    if(s->editor)finish_edit((HWND)s->editor,0);
    LONG_PTR style=GetWindowLongPtrW(native,GWL_STYLE);
    SetWindowLongPtrW(native,GWL_STYLE,s->multiple?style&~LVS_SINGLESEL:style|LVS_SINGLESEL);
    int widths[64]={0};
    for(size_t i=0;i<w->columns;++i)widths[i]=ListView_GetColumnWidth(native,(int)i);
    ListView_SetItemCount(native,0);while(ListView_DeleteColumn(native,0)){}
    for(size_t i=0;i<w->columns;++i){
        LVCOLUMNW column={0};wchar_t *text=cui__win32_wide(w->headers[i]);
        column.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM|LVCF_FMT;
        column.pszText=text;column.cx=widths[i]>0?widths[i]:(int)(150*w->window->scale);column.iSubItem=(int)i;
        ListView_InsertColumn(native,(int)i,&column);free(text);
        HDITEMW header={0};header.mask=HDI_FORMAT;Header_GetItem(ListView_GetHeader(native),(int)i,&header);
        header.fmt&=~(HDF_SORTUP|HDF_SORTDOWN);
        if((int)i==s->sort_column)header.fmt|=s->descending?HDF_SORTDOWN:HDF_SORTUP;
        Header_SetItem(ListView_GetHeader(native),(int)i,&header);
    }
    ListView_SetItemCountEx(native,(int)s->rows,LVSICF_NOSCROLL);InvalidateRect(native,NULL,TRUE);
}
void cui__backend_table_selection(cui_widget *w)
{
    cui_table_state *s=w->payload;
    ListView_SetItemState((HWND)w->native,-1,0,LVIS_SELECTED);
    for(size_t i=0;i<s->rows;++i)if(s->selected[i])ListView_SetItemState((HWND)w->native,(int)i,LVIS_SELECTED,LVIS_SELECTED);
}
void cui__backend_table_cell(cui_widget *w,size_t row,size_t column)
{(void)column;ListView_RedrawItems((HWND)w->native,(int)row,(int)row);}

void cui__backend_table_reveal(cui_widget *w,size_t row)
{ListView_EnsureVisible((HWND)w->native,(int)row,FALSE);}

void cui__backend_table_headers(cui_widget *w,int visible)
{
    HWND hwnd=(HWND)w->native;
    LONG_PTR flags=GetWindowLongPtrW(hwnd,GWL_STYLE);
    SetWindowLongPtrW(hwnd,GWL_STYLE,visible?flags&~LVS_NOCOLUMNHEADER:flags|LVS_NOCOLUMNHEADER);
    SetWindowPos(hwnd,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
}
