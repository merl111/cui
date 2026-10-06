#include "cui_winui.h"
using namespace cui::winui;
namespace {
controls::Grid row_content(cui_widget *w, size_t row) {
    controls::Grid grid;
    auto s = static_cast<cui_table_state *>(w->payload);
    uint64_t generation = state(w).table_generation;
    for (size_t column = 0; column < w->columns; ++column) {
        controls::ColumnDefinition def;
        def.Width({150, xaml::GridUnitType::Pixel});
        grid.ColumnDefinitions().Append(def);
        const char *text = w->items[row * w->columns + column];
        xaml::FrameworkElement cell{nullptr};
        if (s->editable[column]) {
            controls::TextBox editor;
            editor.Text(wide(text));
            xaml::Automation::AutomationProperties::SetName(editor, wide(w->headers[column]));
            auto commit = [w, row, column, generation](auto const &sender, auto const &) {
                protect(w->window->app, [&] {
                    if (w->updating || w->window->app->destroying ||
                        state(w).table_generation != generation)
                        return;
                    auto model = static_cast<cui_table_state *>(w->payload);
                    if (row >= model->rows || column >= w->columns)
                        return;
                    auto text = winrt::to_string(sender.template as<controls::TextBox>().Text());
                    if (text != w->items[row * w->columns + column])
                        cui__table_edit(w, row, column, text.c_str());
                });
            };
            editor.LostFocus(commit);
            editor.KeyDown([w, row, column, generation,
                            commit](auto const &sender, xaml::Input::KeyRoutedEventArgs const &e) {
                protect(w->window->app, [&] {
                    if (e.Key() == winrt::Windows::System::VirtualKey::Enter) {
                        commit(sender, e);
                        e.Handled(true);
                    } else if (e.Key() == winrt::Windows::System::VirtualKey::Escape &&
                               state(w).table_generation == generation) {
                        auto model = static_cast<cui_table_state *>(w->payload);
                        if (row < model->rows && column < w->columns)
                            sender.template as<controls::TextBox>().Text(
                                wide(w->items[row * w->columns + column]));
                        e.Handled(true);
                    }
                });
            });
            cell = editor;
        } else {
            controls::TextBlock label;
            label.Text(wide(text));
            label.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
            label.VerticalAlignment(xaml::VerticalAlignment::Center);
            cell = label;
        }
        cell.Margin({8, 4, 8, 4});
        controls::Grid::SetColumn(cell, int(column));
        grid.Children().Append(cell);
    }
    return grid;
}
void selection(cui_widget *w) {
    if (w->updating || w->window->app->destroying)
        return;
    auto s = static_cast<cui_table_state *>(w->payload);
    if (!s)
        return;
    std::fill_n(s->selected, s->rows, 0);
    for (auto item : state(w).rows.SelectedItems()) {
        auto index = winrt::unbox_value<uint32_t>(item);
        if (index < s->rows)
            s->selected[index] = 1;
    }
    cui__table_selection_changed(w);
}
} // namespace
void cui::winui::update_table(cui_widget *w) {
    auto &view = state(w);
    auto s = static_cast<cui_table_state *>(w->payload);
    if (!s)
        return;
    ++view.table_generation;
    if (!view.table_connected) {
        view.rows.SelectionChanged(
            [w](auto const &, auto const &) { protect(w->window->app, [&] { selection(w); }); });
        view.rows.ContainerContentChanging(
            [w](auto const &, controls::ContainerContentChangingEventArgs const &e) {
                protect(w->window->app, [&] {
                    if (e.InRecycleQueue())
                        return;
                    auto s = static_cast<cui_table_state *>(w->payload);
                    auto index = e.ItemIndex();
                    if (s && index >= 0 && size_t(index) < s->rows) {
                        e.ItemContainer().Content(row_content(w, size_t(index)));
                        e.Handled(true);
                    }
                });
            });
        view.table_connected = true;
    }
    view.rows.Items().Clear();
    view.headers.Children().Clear();
    view.rows.SelectionMode(s->multiple ? controls::ListViewSelectionMode::Multiple
                                        : controls::ListViewSelectionMode::Single);
    for (size_t c = 0; c < w->columns; ++c) {
        controls::Button header;
        header.Width(150);
        header.HorizontalContentAlignment(xaml::HorizontalAlignment::Left);
        auto label = wide(w->headers[c]);
        if (s->sort_column == int(c))
            label = label + (s->descending ? L" ▼" : L" ▲");
        header.Content(winrt::box_value(label));
        header.Click([w, c](auto const &, auto const &) {
            protect(w->window->app, [&] {
                auto s = static_cast<cui_table_state *>(w->payload);
                cui__table_sort(w, c, s->sort_column == int(c) ? !s->descending : 0);
            });
        });
        view.headers.Children().Append(header);
    }
    view.headers.Visibility(view.headers_visible ? xaml::Visibility::Visible
                                                 : xaml::Visibility::Collapsed);
    for (size_t r = 0; r < s->rows; ++r)
        view.rows.Items().Append(winrt::box_value(uint32_t(r)));
}
extern "C" void cui__backend_table_headers(cui_widget *w, int visible) {
    protect(w->window->app, [&] {
        state(w).headers_visible = visible != 0;
        state(w).headers.Visibility(visible ? xaml::Visibility::Visible
                                            : xaml::Visibility::Collapsed);
    });
}
extern "C" void cui__backend_table_selection(cui_widget *w) {
    protect(w->window->app, [&] {
        auto s = static_cast<cui_table_state *>(w->payload);
        auto list = state(w).rows;
        list.SelectedItems().Clear();
        for (size_t r = 0; r < s->rows; ++r)
            if (s->selected[r])
                list.SelectedItems().Append(list.Items().GetAt(uint32_t(r)));
    });
}
extern "C" void cui__backend_table_reveal(cui_widget *w, size_t row) {
    protect(w->window->app, [&] {
        auto list = state(w).rows;
        if (row < list.Items().Size())
            list.ScrollIntoView(list.Items().GetAt(uint32_t(row)));
    });
}
extern "C" void cui__backend_table_cell(cui_widget *w, size_t row, size_t) {
    protect(w->window->app, [&] {
        auto list = state(w).rows;
        auto container = list.ContainerFromIndex(int(row)).try_as<controls::ListViewItem>();
        if (container)
            container.Content(row_content(w, row));
    });
}
