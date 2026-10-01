#include "cui_internal.h"

static float maximum(float a, float b) { return a > b ? a : b; }

cui_size cui__measure(cui_widget *widget)
{
    cui_size size = {0, 0};
    cui_widget *child;
    int count = 0;
    if (widget->hidden) { widget->minimum = size; return size; }
    if (widget->kind == CUI_GRID || widget->kind == CUI_WRAP || widget->kind == CUI_SPLIT) size = cui__layout_measure(widget);
    else if (!cui__container(widget)) size = cui__backend_measure(widget);
    else {
        for (child = widget->first; child; child = child->next) {
            if (child->hidden) continue;
            cui_size item = cui__measure(child);
            float gap = count++ ? (float)widget->gap : 0;
            if (widget->axis == CUI_HORIZONTAL) {
                size.width += item.width + gap;
                size.height = maximum(size.height, item.height);
            } else {
                size.height += item.height + gap;
                size.width = maximum(size.width, item.width);
            }
        }
        size.width += 2.0f * (float)widget->padding;
        size.height += 2.0f * (float)widget->padding;
    }
    size.width = maximum(size.width, (float)widget->min_width);
    size.height = maximum(size.height, (float)widget->min_height);
    widget->minimum = size;
    return size;
}

void cui__arrange(cui_widget *widget, cui_rect rect)
{
    cui_widget *child;
    cui_rect inner = rect;
    int expanding = 0;
    float extra, cursor;
    if (widget->hidden) return;
    widget->frame = rect;
    if (widget->kind == CUI_GRID || widget->kind == CUI_WRAP || widget->kind == CUI_SPLIT) { cui__layout_arrange(widget, rect); return; }
    if (!cui__container(widget)) { cui__backend_place(widget); return; }
    inner.x += (float)widget->padding;
    inner.y += (float)widget->padding;
    inner.width = maximum(0, inner.width - 2.0f * (float)widget->padding);
    inner.height = maximum(0, inner.height - 2.0f * (float)widget->padding);
    for (child = widget->first; child; child = child->next)
        if (!child->hidden) expanding += !!child->expand;
    extra = widget->axis == CUI_HORIZONTAL ? rect.width - widget->minimum.width
                                           : rect.height - widget->minimum.height;
    extra = expanding ? maximum(0, extra) / (float)expanding : 0;
    cursor = widget->axis == CUI_HORIZONTAL ? inner.x : inner.y;
    for (child = widget->first; child; child = child->next) {
        if (child->hidden) continue;
        cui_rect item = inner;
        if (widget->axis == CUI_HORIZONTAL) {
            item.x = cursor;
            item.width = child->minimum.width + (child->expand ? extra : 0);
            item.height = maximum(item.height, child->minimum.height);
            cursor += item.width + (float)widget->gap;
        } else {
            item.y = cursor;
            item.height = child->minimum.height + (child->expand ? extra : 0);
            item.width = maximum(item.width, child->minimum.width);
            cursor += item.height + (float)widget->gap;
        }
        cui__arrange(child, item);
    }
}

void cui__layout(cui_window *window, float width, float height)
{
    cui_rect rect = {0, 0, width, height};
    if (!window->root || window->laying_out) return;
    window->laying_out = 1;
    cui__measure(window->root);
    if (window->scrollable) {
        rect.width = maximum(rect.width, window->root->minimum.width);
        rect.height = maximum(rect.height, window->root->minimum.height);
    }
    window->document_width = rect.width;
    window->document_height = rect.height;
    cui__arrange(window->root, rect);
    cui__measure(window->root);
    if (window->scrollable) rect.height = maximum(height, window->root->minimum.height);
    window->document_height = rect.height;
    cui__arrange(window->root, rect);
    window->laying_out = 0;
}
