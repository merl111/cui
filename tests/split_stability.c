#include "cui.h"
#include "cui_layouts.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static cui_app *app;
static cui_window *window;
static cui_widget *left, *right, *side, *inspector;
static int stage, initial, restored;
static void verify(void *data) {
    (void)data;
    int width, height;
    assert(cui_widget_get_size(side, &width, &height));
    assert(fabs(cui_split_get_position(left) - .23) < .001);
    assert(fabs(cui_split_get_position(right) - .73) < .001);
    switch(stage++) {
    case 0: initial=width; assert(width>250); cui_set_visible(inspector,0); break;
    case 1: assert(abs(width-initial)<=2); cui_set_visible(inspector,1); break;
    case 2: assert(abs(width-initial)<=2); cui_window_set_size(window,1100,800); break;
    case 3: assert(width<initial); restored=width; cui_set_visible(inspector,0); break;
    case 4: assert(abs(width-restored)<=2); cui_set_visible(inspector,1); break;
    case 5: assert(abs(width-restored)<=2); cui_app_quit(app); break;
    }
}
int main(void) {
    app=cui_app_create(); assert(app);
    window=cui_window_create(app,"Nested split stability",1440,900);
    cui_widget *root=cui_window_root(window); cui_box_set_padding(root,12);
    left=cui_split(root,CUI_HORIZONTAL,.23); side=cui_split_pane(left,0);
    cui_set_min_size(side,200,1); cui_label(side,"Rooms");
    right=cui_split(cui_split_pane(left,1),CUI_HORIZONTAL,.73);
    cui_set_min_size(cui_split_pane(right,0),380,1); cui_label(cui_split_pane(right,0),"Messages");
    inspector=cui_split_pane(right,1); cui_set_min_size(inspector,220,1); cui_label(inspector,"Inspector");
    cui_window_show(window); cui_every(app,200,verify,NULL); cui_app_run(app); cui_app_destroy(app);
    puts("split stability: initial ratios, nested visibility and resize passed");
}
