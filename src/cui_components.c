#include "cui_internal.h"

static void tab_clicked(cui_widget *sender, void *data)
{
    cui_widget *tabs = (cui_widget *)data;
    int previous = tabs->selected;
    cui_set_selected(tabs, sender->index);
    if (previous != tabs->selected) cui__emit(tabs);
}
cui_widget *cui_tabs(cui_widget *parent)
{
    cui_widget *tabs = cui_box(parent, CUI_VERTICAL, 16);
    if (!tabs) return NULL;
    tabs->component = CUI_COMPONENT_TABS;
    if (!cui_box(tabs, CUI_HORIZONTAL, 6)) return NULL;
    tabs->content = cui_box(tabs, CUI_VERTICAL, 0);
    cui_expand(tabs->content, 1);
    return tabs->content ? tabs : NULL;
}
cui_widget *cui_tab_add(cui_widget *tabs, const char *title)
{
    cui_widget *button, *page;
    int index;
    if (!tabs || tabs->component != CUI_COMPONENT_TABS) return NULL;
    index = tabs->content->last ? tabs->content->last->index + 1 : 0;
    button = cui_toggle(tabs->first, title, index == 0);
    page = cui_box(tabs->content, CUI_VERTICAL, 16);
    if (!button || !page) return NULL;
    button->index = page->index = index;
    cui_expand(button, 1);
    cui_on_action(button, tab_clicked, tabs);
    cui_set_visible(page, index == 0);
    if (index == 0) tabs->selected = 0;
    return page;
}
void cui__tabs_select(cui_widget *tabs, int index)
{
    cui_widget *page, *button;
    if (index < 0 || !tabs->content->last || index > tabs->content->last->index) return;
    tabs->selected = index;
    for (button = tabs->first->first; button; button = button->next)
        cui_set_checked(button, button->index == index);
    for (page = tabs->content->first; page; page = page->next)
        cui_set_visible(page, page->index == index);
}
static void disclosure_clicked(cui_widget *sender, void *data)
{
    cui_widget *w = (cui_widget *)data;
    cui_set_expanded(w, cui_get_checked(sender));
    cui__emit(w);
}
cui_widget *cui_disclosure(cui_widget *parent, const char *title, int expanded)
{
    cui_widget *w = cui_box(parent, CUI_VERTICAL, 10);
    cui_widget *button;
    if (!w) return NULL;
    w->component = CUI_COMPONENT_DISCLOSURE;
    button = cui_toggle(w, title, expanded);
    w->content = cui_box(w, CUI_VERTICAL, 8);
    if (!button || !w->content) return NULL;
    cui_on_action(button, disclosure_clicked, w);
    cui_set_visible(w->content, expanded);
    return w;
}
cui_widget *cui_disclosure_content(cui_widget *w)
{ return w && w->component == CUI_COMPONENT_DISCLOSURE ? w->content : NULL; }
void cui_set_expanded(cui_widget *w, int expanded)
{
    if (!cui_disclosure_content(w)) return;
    cui_set_checked(w->first, expanded);
    cui_set_visible(w->content, expanded);
}
int cui_get_expanded(const cui_widget *w)
{ return w && w->component == CUI_COMPONENT_DISCLOSURE ? !w->content->hidden : 0; }
