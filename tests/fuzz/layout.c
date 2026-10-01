#include "cui_internal.h"
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <math.h>
cui_size cui__backend_measure(cui_widget *w){return w->minimum;}
void cui__backend_place(cui_widget *w){(void)w;}
int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size)
{
    if(size<8)return 0;
    cui_widget root={0},items[16]={{0}};
    const cui_kind kinds[]={CUI_BOX,CUI_GRID,CUI_WRAP,CUI_SPLIT};
    root.kind=kinds[data[0]%4];root.axis=(cui_axis)(data[1]%2);
    root.gap=data[2];root.padding=data[3];root.grid_columns=1+data[4]%8;
    root.value=(double)data[5]/255.;root.frame.width=data[6]*16;
    size_t count=(size-8)/4;if(count>16)count=16;
    if(root.kind==CUI_SPLIT && count>2)count=2;
    for(size_t i=0;i<count;++i){
        const uint8_t *p=data+8+i*4;
        items[i].kind=CUI_BUTTON;items[i].minimum=(cui_size){p[0]*8.f,p[1]*8.f};
        items[i].hidden=p[2]&1;items[i].expand=p[3]&1;
        items[i].grid_row=(unsigned)i/root.grid_columns;items[i].grid_column=(unsigned)i%root.grid_columns;
        items[i].grid_row_span=items[i].grid_column_span=1;
        items[i].parent=&root;items[i].next=i+1<count?items+i+1:NULL;
    }
    if(count){root.first=items;root.last=items+count-1;}
    cui_size measured=cui__measure(&root);
    assert(isfinite(measured.width)&&isfinite(measured.height));
    cui__arrange(&root,(cui_rect){0,0,data[6]*16.f,data[7]*16.f});
    for(size_t i=0;i<count;++i)if(!items[i].hidden){
        assert(isfinite(items[i].frame.x)&&isfinite(items[i].frame.y));
        assert(isfinite(items[i].frame.width)&&items[i].frame.width>=0);
        assert(isfinite(items[i].frame.height)&&items[i].frame.height>=0);
    }
    return 0;
}
