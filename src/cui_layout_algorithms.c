#include "cui_internal.h"
#include <string.h>
static float maxf(float a,float b){return a>b?a:b;}
static unsigned grid_sizes(cui_widget *w,float *widths,float *heights)
{
    unsigned rows=0;memset(widths,0,64*sizeof(*widths));memset(heights,0,256*sizeof(*heights));
    for(cui_widget *c=w->first;c;c=c->next){if(c->hidden)continue;cui_size size=cui__measure(c);unsigned end=c->grid_row+c->grid_row_span;
        if(end>256||c->grid_column+c->grid_column_span>w->grid_columns)continue;
        if(end>rows)rows=end;
        float width=(float)w->gap*(c->grid_column_span-1),height=(float)w->gap*(c->grid_row_span-1);
        for(unsigned i=c->grid_column;i<c->grid_column+c->grid_column_span;++i)width+=widths[i];
        for(unsigned i=c->grid_row;i<end;++i)height+=heights[i];
        float dx=maxf(0,size.width-width)/c->grid_column_span,dy=maxf(0,size.height-height)/c->grid_row_span;
        for(unsigned i=c->grid_column;i<c->grid_column+c->grid_column_span;++i)widths[i]+=dx;
        for(unsigned i=c->grid_row;i<end;++i)heights[i]+=dy;
    }return rows;
}
static cui_size wrap_items(cui_widget *w,float width,int arrange)
{
    float x=0,y=0,row_height=0,max_width=0;int count=0;
    for(cui_widget *c=w->first;c;c=c->next){if(c->hidden)continue;cui_size size=arrange?c->minimum:cui__measure(c);
        if(count&&x+size.width>width){y+=row_height+w->gap;x=0;row_height=0;count=0;}
        if(arrange)cui__arrange(c,(cui_rect){w->frame.x+w->padding+x,w->frame.y+w->padding+y,size.width,size.height});
        max_width=maxf(max_width,x+size.width);x+=size.width+w->gap;row_height=maxf(row_height,size.height);++count;
    }return(cui_size){max_width,y+row_height};
}
cui_size cui__layout_measure(cui_widget *w)
{
    cui_size size={0,0};
    if(w->kind==CUI_GRID){float widths[64],heights[256];unsigned rows=grid_sizes(w,widths,heights);
        for(unsigned i=0;i<w->grid_columns;++i)size.width+=widths[i]+(i?w->gap:0);
        for(unsigned i=0;i<rows;++i)size.height+=heights[i]+(i?w->gap:0);
    }else if(w->kind==CUI_WRAP){
        float width=w->frame.width>0?w->frame.width-2*w->padding:320;
        size=wrap_items(w,width,0);size.width=0;
        for(cui_widget *c=w->first;c;c=c->next)if(!c->hidden)size.width=maxf(size.width,c->minimum.width);
    }else if(w->first&&w->first->next){cui_size a=cui__measure(w->first),b=cui__measure(w->last);
        size=w->axis==CUI_HORIZONTAL?(cui_size){a.width+b.width+w->gap,maxf(a.height,b.height)}:(cui_size){maxf(a.width,b.width),a.height+b.height+w->gap};
    }
    size.width+=2*w->padding;size.height+=2*w->padding;return size;
}
void cui__layout_arrange(cui_widget *w,cui_rect rect)
{
    if(w->kind==CUI_WRAP){wrap_items(w,maxf(0,rect.width-2*w->padding),1);return;}
    if(w->kind==CUI_GRID){float widths[64],heights[256];unsigned rows=grid_sizes(w,widths,heights);
        float extra=w->grid_columns?maxf(0,rect.width-w->minimum.width)/w->grid_columns:0;
        for(unsigned i=0;i<w->grid_columns;++i)widths[i]+=extra;
        for(cui_widget *c=w->first;c;c=c->next){if(c->hidden||c->grid_row+c->grid_row_span>rows||c->grid_column+c->grid_column_span>w->grid_columns)continue;
            cui_rect cell={rect.x+w->padding,rect.y+w->padding,0,0};
            for(unsigned i=0;i<c->grid_column;++i)cell.x+=widths[i]+w->gap;
            for(unsigned i=0;i<c->grid_row;++i)cell.y+=heights[i]+w->gap;
            for(unsigned i=0;i<c->grid_column_span;++i)cell.width+=widths[c->grid_column+i]+(i?w->gap:0);
            for(unsigned i=0;i<c->grid_row_span;++i)cell.height+=heights[c->grid_row+i]+(i?w->gap:0);
            cui__arrange(c,cell);
        }return;
    }
    if(!w->first||!w->first->next)return;
    cui_rect a={rect.x+w->padding,rect.y+w->padding,maxf(0,rect.width-2*w->padding),maxf(0,rect.height-2*w->padding)},b=a;
    float total=maxf(0,(w->axis==CUI_HORIZONTAL?a.width:a.height)-w->gap);
    float minimum=w->axis==CUI_HORIZONTAL?w->first->minimum.width:w->first->minimum.height;
    float other=w->axis==CUI_HORIZONTAL?w->last->minimum.width:w->last->minimum.height;
    float first=maxf(minimum,(float)(total*w->value));if(first>total-other)first=maxf(minimum,total-other);
    if(w->axis==CUI_HORIZONTAL){a.width=first;b.x+=first+w->gap;b.width=maxf(other,total-first);}else{a.height=first;b.y+=first+w->gap;b.height=maxf(other,total-first);}
    cui__arrange(w->first,a);cui__arrange(w->last,b);cui__backend_place(w);
}
