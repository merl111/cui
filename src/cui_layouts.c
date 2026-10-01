#include "cui_internal.h"
#include "cui_layouts.h"
#include <math.h>
#include <string.h>
#include <stdint.h>
cui_widget *cui_grid(cui_widget *parent,unsigned columns,int gap)
{
    if(!columns||columns>64||gap<0)return NULL;
    cui_widget *w=cui__append(parent,CUI_GRID,"",CUI_VERTICAL,gap);
    if(w){w->grid_columns=columns;cui__backend_container(w);}return w;
}
cui_widget *cui_grid_cell(cui_widget *grid,unsigned row,unsigned column,unsigned rows,unsigned columns)
{
    if(!grid||grid->kind!=CUI_GRID||!rows||!columns||row>=256||rows>256-row||column>=grid->grid_columns||columns>grid->grid_columns-column)return NULL;
    for(cui_widget *c=grid->first;c;c=c->next)
        if(row<c->grid_row+c->grid_row_span&&row+rows>c->grid_row&&column<c->grid_column+c->grid_column_span&&column+columns>c->grid_column)return NULL;
    cui_widget *cell=cui_box(grid,CUI_VERTICAL,8);if(!cell)return NULL;
    cell->grid_row=row;cell->grid_column=column;cell->grid_row_span=rows;cell->grid_column_span=columns;
    cui__backend_grid_cell(cell);cui__backend_refresh(grid->window);return cell;
}
cui_widget *cui_wrap(cui_widget *parent,int gap)
{if(gap<0)return NULL;cui_widget *w=cui__append(parent,CUI_WRAP,"",CUI_HORIZONTAL,gap);if(w)cui__backend_container(w);return w;}
cui_widget *cui_split(cui_widget *parent,cui_axis axis,double fraction)
{
    if((axis!=CUI_HORIZONTAL&&axis!=CUI_VERTICAL)||!isfinite(fraction))return NULL;
    cui_widget *w=cui__append(parent,CUI_SPLIT,"",axis,8);if(!w)return NULL;
    cui__backend_container(w);
    if(!cui_box(w,CUI_VERTICAL,12)||!cui_box(w,CUI_VERTICAL,12))return NULL;
    cui_split_set_position(w,fraction);cui_expand(w,1);return w;
}
cui_widget *cui_split_pane(cui_widget *w,unsigned index)
{return w&&w->kind==CUI_SPLIT&&index<2?(index?w->last:w->first):NULL;}
void cui_split_set_position(cui_widget *w,double fraction)
{
    if(!w||w->kind!=CUI_SPLIT||!isfinite(fraction))return;
    w->value=fraction<0?0:fraction>1?1:fraction;
    ++w->updating;cui__backend_split_position(w);--w->updating;cui__backend_refresh(w->window);
}
double cui_split_get_position(const cui_widget *w){return w&&w->kind==CUI_SPLIT?w->value:0;}

int cui__grid_default_slot(const cui_widget *grid, cui_widget *child)
{
    uint64_t occupied[256] = {0};
    for (const cui_widget *w=grid->first;w;w=w->next) {
        uint64_t mask=w->grid_column_span==64?UINT64_MAX:((UINT64_C(1)<<w->grid_column_span)-1)<<w->grid_column;
        for (unsigned row=w->grid_row;row<w->grid_row+w->grid_row_span;++row) occupied[row]|=mask;
    }
    for (unsigned row=0;row<256;++row)
        for (unsigned column=0;column<grid->grid_columns;++column)
            if (!(occupied[row]&(UINT64_C(1)<<column))) { child->grid_row=row;child->grid_column=column;return 1; }
    return 0;
}
