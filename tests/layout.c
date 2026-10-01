#include "cui_internal.h"
#include "cui_layouts.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)

cui_size cui__backend_measure(cui_widget *widget) { return widget->minimum; }
void cui__backend_place(cui_widget *widget) { (void)widget; }

int main(void)
{
    cui_widget root = {0}, row = {0}, first = {0}, second = {0}, third = {0};
    cui_size size;
    root.kind = CUI_BOX; root.axis = CUI_VERTICAL; root.padding = 10; root.gap = 8;
    row.kind = CUI_BOX; row.axis = CUI_HORIZONTAL; row.gap = 6;
    first.kind = second.kind = third.kind = CUI_BUTTON;
    first.minimum = (cui_size){80, 24};
    second.minimum = (cui_size){100, 32};
    third.minimum = (cui_size){40, 20};
    root.first = &row; row.next = &third;
    row.first = &first; first.next = &second;
    first.expand = second.expand = 1;
    third.expand = 1;
    size = cui__measure(&root);
    CHECK(size.width == 206 && size.height == 80);
    cui__arrange(&root, (cui_rect){0, 0, 306, 180});
    CHECK(first.frame.x == 10 && first.frame.y == 10);
    CHECK(first.frame.width == 130 && first.frame.height == 32);
    CHECK(second.frame.x == 146 && second.frame.width == 150);
    CHECK(third.frame.y == 50 && third.frame.height == 120);
    CHECK(third.frame.width == 286);
    /* Minimums survive undersized native allocations without negative sizes. */
    cui__arrange(&root, (cui_rect){0, 0, 1, 1});
    CHECK(first.frame.width == 80 && second.frame.width == 100);
    CHECK(third.frame.height == 20 && third.frame.width == 40);
    /* Empty boxes can act as flexible spacers without artificial minimums. */
    root.first = NULL;
    size = cui__measure(&root);
    CHECK(size.width == 20 && size.height == 20);
    cui__arrange(&root, (cui_rect){0, 0, 0, 0});
    cui_widget grid={0}, a={0}, b={0}, c={0};
    grid.kind=CUI_GRID;grid.grid_columns=2;grid.gap=10;grid.first=&a;a.next=&b;b.next=&c;
    a.kind=b.kind=c.kind=CUI_BUTTON;
    a.minimum=(cui_size){100,30};b.minimum=(cui_size){200,40};c.minimum=(cui_size){320,50};
    a.grid_row_span=b.grid_row_span=c.grid_row_span=1;
    a.grid_column_span=b.grid_column_span=1;b.grid_column=1;c.grid_row=1;c.grid_column_span=2;
    size=cui__measure(&grid);CHECK(size.width==320&&size.height==100);
    cui__arrange(&grid,(cui_rect){0,0,420,100});CHECK(c.frame.width==420&&c.frame.y==50);
    CHECK(b.frame.x>=a.frame.x+a.frame.width+10);
    grid.kind=CUI_WRAP;grid.frame.width=330;a.minimum=(cui_size){100,30};b.minimum=(cui_size){200,40};c.minimum=(cui_size){150,50};
    size=cui__measure(&grid);CHECK(size.width==200&&size.height==100);
    cui__arrange(&grid,(cui_rect){0,0,330,100});CHECK(c.frame.y==50&&b.frame.x==110);
    grid.kind=CUI_SPLIT;grid.axis=CUI_HORIZONTAL;grid.value=0.25;grid.last=&b;b.next=NULL;
    size=cui__measure(&grid);cui__arrange(&grid,(cui_rect){0,0,500,100});
    CHECK(a.frame.width>=100&&b.frame.width>=200&&b.frame.x==a.frame.width+10);
    cui_widget stack={0}, base={0}, overlay={0};
    stack.kind=CUI_STACK;stack.first=&base;base.next=&overlay;
    base.kind=overlay.kind=CUI_BUTTON;base.minimum=(cui_size){300,200};
    overlay.minimum=(cui_size){600,400};overlay.layer_alignment=CUI_LAYER_CENTER;
    overlay.layer_width=200;overlay.layer_height=100;overlay.layer_margin=10;
    size=cui__measure(&stack);CHECK(size.width==300&&size.height==200);
    cui__arrange(&stack,(cui_rect){20,30,500,400});
    CHECK(base.frame.width==500&&base.frame.height==400);
    CHECK(overlay.frame.x==170&&overlay.frame.y==180&&overlay.frame.width==200);
    overlay.layer_alignment=CUI_LAYER_BOTTOM_RIGHT;
    cui__arrange(&stack,(cui_rect){20,30,500,400});
    CHECK(overlay.frame.x==310&&overlay.frame.y==320);
    cui__arrange(&stack,(cui_rect){0,0,90,80});
    CHECK(overlay.frame.width==70&&overlay.frame.height==60);
    overlay.hidden=1;size=cui__measure(&stack);CHECK(size.width==300);
    puts("layout: nested boxes, expansion, padding and minimum sizes passed");
    return 0;
}
