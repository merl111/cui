#include "cui_winui.h"
using namespace cui::winui;
namespace {
void changed(cui_widget *w) {
    if (!w->updating && !w->window->app->destroying)
        cui__emit(w);
}
controls::TextBox text_box(cui_widget *w, hstring const &text) {
    controls::TextBox box;
    box.Text(text);
    box.AcceptsReturn(w->kind == CUI_TEXTAREA || w->kind == CUI_CODE);
    box.TextWrapping(w->kind == CUI_TEXTAREA ? xaml::TextWrapping::Wrap
                                             : xaml::TextWrapping::NoWrap);
    box.IsReadOnly(w->kind == CUI_CODE);
    box.TextChanging([w](auto const &, auto const &) { changed(w); });
    box.TextCompositionStarted([w](auto const &, auto const &) { w->composing = 1; });
    box.TextCompositionEnded([w](auto const &, auto const &) { w->composing = 0; });
    return box;
}
void create_container(cui_widget *w, widget_state &s) {
    s.children = controls::Canvas();
    s.border = xaml::Markup::XamlReader::Load(
                   L"<Border xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' />")
                   .as<controls::Border>();
    s.border.Child(s.children);
    s.element = s.border;
    if (w->kind != CUI_SPLIT)
        return;
    s.splitter = controls::Primitives::Thumb();
    s.children.Children().Append(s.splitter);
    controls::Canvas::SetZIndex(s.splitter, 1);
    xaml::Automation::AutomationProperties::SetName(s.splitter, L"Resize panes");
    s.splitter.IsTabStop(true);
    s.splitter.DragDelta([w](auto const &, controls::Primitives::DragDeltaEventArgs const &e) {
        protect(w->window->app, [&] {
            double size = w->axis == CUI_HORIZONTAL ? w->frame.width : w->frame.height;
            double delta = w->axis == CUI_HORIZONTAL ? e.HorizontalChange() : e.VerticalChange();
            if (size > 0) {
                w->value = std::clamp(w->value + delta / size, 0., 1.);
                cui__backend_refresh(w->window);
                changed(w);
            }
        });
    });
    s.splitter.KeyDown([w](auto const &, xaml::Input::KeyRoutedEventArgs const &e) {
        using K = winrt::Windows::System::VirtualKey;
        double delta = e.Key() == K::Left || e.Key() == K::Up      ? -.02
                       : e.Key() == K::Right || e.Key() == K::Down ? .02
                                                                   : 0;
        if (delta) {
            w->value = std::clamp(w->value + delta, 0., 1.);
            cui__backend_refresh(w->window);
            changed(w);
            e.Handled(true);
        }
    });
}
void create_tree(cui_widget *w, widget_state &s) {
    controls::TreeView tree;
    tree.ItemInvoked([w](auto const &, controls::TreeViewItemInvokedEventArgs const &e) {
        protect(w->window->app, [&] {
            if (w->updating)
                return;
            auto node = e.InvokedItem().try_as<controls::TreeViewNode>();
            auto &nodes = state(w).nodes;
            auto m = static_cast<cui_tree_model *>(w->payload);
            for (size_t i = 0; m && i < nodes.size(); ++i)
                if (nodes[i] == node) {
                    cui__tree_event(w, CUI_TREE_ACTIVATE, m->nodes[i].id);
                    break;
                }
        });
    });
    tree.SelectionChanged([w](auto const &sender, auto const &) {
        protect(w->window->app, [&] {
            if (w->updating)
                return;
            auto node = sender.SelectedNode();
            auto m = static_cast<cui_tree_model *>(w->payload);
            for (size_t i = 0; m && i < state(w).nodes.size(); ++i)
                if (state(w).nodes[i] == node) {
                    cui__tree_event(w, CUI_TREE_SELECTION, m->nodes[i].id);
                    break;
                }
        });
    });
    auto expanded = [w](controls::TreeViewNode const &node, bool expanded) {
        protect(w->window->app, [&] {
            if (w->updating)
                return;
            auto m = static_cast<cui_tree_model *>(w->payload);
            for (size_t i = 0; m && i < state(w).nodes.size(); ++i)
                if (state(w).nodes[i] == node) {
                    m->nodes[i].expanded = expanded;
                    cui__tree_event(w, expanded ? CUI_TREE_EXPAND : CUI_TREE_COLLAPSE,
                                    m->nodes[i].id);
                    break;
                }
        });
    };
    tree.Expanding([expanded](auto const &, auto const &e) { expanded(e.Node(), true); });
    tree.Collapsed([expanded](auto const &, auto const &e) { expanded(e.Node(), false); });
    s.element = tree;
}
void create_time(cui_widget *w, widget_state &s) {
    controls::StackPanel panel;
    panel.Orientation(controls::Orientation::Horizontal);
    panel.Spacing(8);
    s.time = controls::TimePicker();
    s.seconds = controls::NumberBox();
    s.seconds.Minimum(0);
    s.seconds.Maximum(59);
    s.seconds.SmallChange(1);
    s.seconds.Width(90);
    s.seconds.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
    xaml::Automation::AutomationProperties::SetName(s.seconds, L"Seconds");
    s.time.TimeChanged([w](auto const &sender, auto const &) {
        protect(w->window->app, [&] {
            if (w->updating || !w->payload)
                return;
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(
                               sender.template as<controls::TimePicker>().Time())
                               .count();
            if (minutes >= 0)
                cui__time_user(w, {int(minutes / 60), int(minutes % 60), cui_time_get(w).second});
        });
    });
    s.seconds.ValueChanged([w](auto const &sender, auto const &) {
        protect(w->window->app, [&] {
            if (w->updating || !w->payload || !std::isfinite(sender.Value()))
                return;
            auto time = cui_time_get(w);
            time.second = int(std::round(sender.Value()));
            cui__time_user(w, time);
        });
    });
    panel.Children().Append(s.time);
    panel.Children().Append(s.seconds);
    s.element = panel;
}
} // namespace
void cui::winui::create_control(cui_widget *w, widget_state &s) {
    if (cui__container(w)) {
        create_container(w, s);
        return;
    }
    switch (w->kind) {
    case CUI_LABEL:
    case CUI_BADGE: {
        controls::TextBlock t;
        t.Text(s.text);
        t.VerticalAlignment(xaml::VerticalAlignment::Center);
        if (w->kind == CUI_BADGE) {
            s.border = controls::Border();
            s.border.Child(t);
            s.border.Padding({10, 4, 10, 4});
            s.border.CornerRadius({8, 8, 8, 8});
            s.element = s.border;
        } else
            s.element = t;
        break;
    }
    case CUI_BUTTON: {
        controls::Button b;
        b.Content(winrt::box_value(s.text));
        b.Click([w](auto const &, auto const &) { changed(w); });
        s.element = b;
        break;
    }
    case CUI_TOGGLE:
    case CUI_CHECKBOX:
    case CUI_RADIO: {
        controls::Primitives::ToggleButton b{nullptr};
        if (w->kind == CUI_TOGGLE)
            b = controls::Primitives::ToggleButton();
        else if (w->kind == CUI_RADIO)
            b = controls::RadioButton();
        else
            b = controls::CheckBox();
        b.Content(winrt::box_value(s.text));
        b.Click([w](auto const &, auto const &) {
            if (w->updating)
                return;
            if (w->kind == CUI_RADIO)
                cui_set_checked(w, 1);
            changed(w);
        });
        s.element = b;
        break;
    }
    case CUI_SWITCH: {
        controls::ToggleSwitch b;
        b.Header(winrt::box_value(s.text));
        b.Toggled([w](auto const &, auto const &) { changed(w); });
        s.element = b;
        break;
    }
    case CUI_ENTRY:
    case CUI_SEARCH:
    case CUI_TEXTAREA:
    case CUI_CODE:
        s.element = text_box(w, s.text);
        break;
    case CUI_PASSWORD: {
        controls::PasswordBox b;
        b.Password(s.text);
        b.PasswordChanged([w](auto const &sender, auto const &) {
            protect(w->window->app, [&] {
                if (w->updating)
                    return;
                auto &v = state(w);
                if (w->read_only) {
                    ++w->updating;
                    sender.template as<controls::PasswordBox>().Password(v.text);
                    --w->updating;
                } else {
                    v.text = sender.template as<controls::PasswordBox>().Password();
                    changed(w);
                }
            });
        });
        s.element = b;
        break;
    }
    case CUI_SELECT: {
        controls::ComboBox b;
        b.SelectionChanged([w](auto const &, auto const &) { changed(w); });
        s.element = b;
        break;
    }
    case CUI_LIST: {
        controls::ListView b;
        b.SelectionChanged([w](auto const &, auto const &) { changed(w); });
        s.element = b;
        break;
    }
    case CUI_TABLE: {
        controls::Grid grid;
        controls::RowDefinition h;
        h.Height({1, xaml::GridUnitType::Auto});
        grid.RowDefinitions().Append(h);
        controls::RowDefinition r;
        r.Height({1, xaml::GridUnitType::Star});
        grid.RowDefinitions().Append(r);
        s.headers = controls::StackPanel();
        s.headers.Orientation(controls::Orientation::Horizontal);
        s.rows = controls::ListView();
        controls::Grid::SetRow(s.rows, 1);
        grid.Children().Append(s.headers);
        grid.Children().Append(s.rows);
        s.element = grid;
        break;
    }
    case CUI_TREE:
        create_tree(w, s);
        break;
    case CUI_SLIDER: {
        controls::Slider b;
        b.Minimum(0);
        b.Maximum(1);
        b.StepFrequency(.001);
        b.ValueChanged([w](auto const &, auto const &) { changed(w); });
        s.element = b;
        break;
    }
    case CUI_PROGRESS: {
        controls::ProgressBar b;
        b.Minimum(0);
        b.Maximum(1);
        s.element = b;
        break;
    }
    case CUI_SPINNER: {
        controls::ProgressRing b;
        b.IsActive(true);
        s.element = b;
        break;
    }
    case CUI_NUMBER: {
        controls::NumberBox b;
        b.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
        b.ValueChanged([w](auto const &sender, auto const &) {
            protect(w->window->app, [&] {
                if (!w->updating && std::isfinite(sender.Value()))
                    cui__number_user(w, sender.Value());
            });
        });
        s.element = b;
        break;
    }
    case CUI_DATE: {
        controls::CalendarDatePicker b;
        b.CalendarIdentifier(L"GregorianCalendar");
        winrt::Windows::Globalization::Calendar bounds;
        bounds.ChangeCalendarSystem(L"GregorianCalendar");
        bounds.Day(1);
        bounds.Month(1);
        bounds.Year(1601);
        b.MinDate(bounds.GetDateTime());
        bounds.Year(9999);
        bounds.Month(12);
        bounds.Day(31);
        b.MaxDate(bounds.GetDateTime());
        b.DateChanged([w](auto const &sender, auto const &) {
            protect(w->window->app, [&] {
                if (w->updating || !sender.Date())
                    return;
                winrt::Windows::Globalization::Calendar c;
                c.ChangeCalendarSystem(L"GregorianCalendar");
                c.SetDateTime(sender.Date().Value());
                cui__date_user(w, {c.Year(), c.Month(), c.Day()});
            });
        });
        s.element = b;
        break;
    }
    case CUI_TIME_INPUT:
        create_time(w, s);
        break;
    case CUI_SEPARATOR:
        s.element =
            xaml::Markup::XamlReader::Load(
                L"<Border xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' "
                L"Background='{ThemeResource DividerStrokeColorDefaultBrush}' />")
                .as<controls::Border>();
        break;
    case CUI_ICON:
    case CUI_IMAGE: {
        s.image = controls::Image();
        s.image.Stretch(media::Stretch::Uniform);
        s.element = s.image;
        break;
    }
    case CUI_CANVAS: {
        controls::Grid g;
        s.image = controls::Image();
        s.image.Stretch(media::Stretch::Uniform);
        s.regions = controls::Canvas();
        g.Children().Append(s.image);
        g.Children().Append(s.regions);
        s.element = g;
        break;
    }
    case CUI_CHART:
        s.children = controls::Canvas();
        s.element = s.children;
        break;
    default:
        throw winrt::hresult_invalid_argument();
    }
}
extern "C" void cui__backend_set_text(cui_widget *w, const char *text) {
    protect(w->window->app, [&] {
        auto &s = state(w);
        s.text = wide(text);
        auto e = s.element;
        if (auto b = e.try_as<controls::TextBox>())
            b.Text(s.text);
        else if (auto b = e.try_as<controls::PasswordBox>())
            b.Password(s.text);
        else if (auto b = e.try_as<controls::TextBlock>())
            b.Text(s.text);
        else if (auto b = e.try_as<controls::ToggleSwitch>())
            b.Header(winrt::box_value(s.text));
        else if (auto b = e.try_as<controls::ContentControl>())
            b.Content(winrt::box_value(s.text));
        else if (w->kind == CUI_BADGE)
            s.border.Child().as<controls::TextBlock>().Text(s.text);
        if (w->icon)
            update_icon(w);
    });
}
extern "C" size_t cui__backend_get_text(const cui_widget *w, char *buffer, size_t capacity) {
    return protect(w->window->app, size_t(0), [&] {
        auto &s = state(w);
        hstring value = s.text;
        if (auto b = s.element.try_as<controls::TextBox>())
            value = b.Text();
        else if (auto b = s.element.try_as<controls::PasswordBox>())
            value = b.Password();
        return cui__copy_text(winrt::to_string(value).c_str(), buffer, capacity);
    });
}
extern "C" void cui__backend_set_checked(cui_widget *w, int checked) {
    protect(w->window->app, [&] {
        auto e = state(w).element;
        if (auto b = e.try_as<controls::ToggleSwitch>())
            b.IsOn(checked != 0);
        else if (auto b = e.try_as<controls::Primitives::ToggleButton>())
            b.IsChecked(winrt::box_value(checked != 0).as<foundation::IReference<bool>>());
    });
}
extern "C" int cui__backend_get_checked(const cui_widget *w) {
    return protect(w->window->app, 0, [&] {
        auto e = state(w).element;
        if (auto b = e.try_as<controls::ToggleSwitch>())
            return int(b.IsOn());
        if (auto b = e.try_as<controls::Primitives::ToggleButton>())
            return int(b.IsChecked() && b.IsChecked().Value());
        return 0;
    });
}
extern "C" void cui__backend_set_enabled(cui_widget *w, int enabled) {
    protect(w->window->app, [&] {
        auto &s = state(w);
        auto e = s.element;
        e.IsHitTestVisible(enabled != 0);
        if (auto c = e.try_as<controls::Control>())
            c.IsEnabled(enabled != 0);
        if (s.rows)
            s.rows.IsEnabled(enabled != 0);
        if (s.splitter)
            s.splitter.IsEnabled(enabled != 0);
        if (s.time)
            s.time.IsEnabled(enabled != 0);
        if (s.seconds)
            s.seconds.IsEnabled(enabled != 0);
        if (s.regions)
            update_regions(w);
    });
}
extern "C" void cui__backend_visible(cui_widget *w, int visible) {
    protect(w->window->app, [&] {
        state(w).element.Visibility(visible ? xaml::Visibility::Visible
                                            : xaml::Visibility::Collapsed);
    });
}
extern "C" void cui__backend_placeholder(cui_widget *w, const char *text) {
    protect(w->window->app, [&] {
        auto e = state(w).element;
        if (auto b = e.try_as<controls::TextBox>())
            b.PlaceholderText(wide(text));
        else if (auto b = e.try_as<controls::PasswordBox>())
            b.PlaceholderText(wide(text));
    });
}
extern "C" void cui__backend_tooltip(cui_widget *w, const char *text) {
    protect(w->window->app, [&] {
        controls::ToolTipService::SetToolTip(state(w).element, winrt::box_value(wide(text)));
    });
}
extern "C" void cui__backend_items(cui_widget *w) {
    protect(w->window->app, [&] {
        if (w->kind == CUI_TABLE) {
            update_table(w);
            return;
        }
        auto view = state(w).element.as<controls::ItemsControl>();
        view.Items().Clear();
        for (size_t i = 0; i < w->item_count; ++i)
            view.Items().Append(winrt::box_value(wide(w->items[i])));
    });
}
extern "C" void cui__backend_set_selected(cui_widget *w, int index) {
    protect(w->window->app, [&] {
        if (w->kind == CUI_TABLE)
            state(w).rows.SelectedIndex(index);
        else
            state(w).element.as<controls::Primitives::Selector>().SelectedIndex(index);
    });
}
extern "C" int cui__backend_get_selected(const cui_widget *w) {
    return protect(w->window->app, -1, [&] {
        return w->kind == CUI_TABLE
                   ? state(w).rows.SelectedIndex()
                   : state(w).element.as<controls::Primitives::Selector>().SelectedIndex();
    });
}
extern "C" void cui__backend_set_value(cui_widget *w, double value) {
    protect(w->window->app,
            [&] { state(w).element.as<controls::Primitives::RangeBase>().Value(value); });
}
extern "C" double cui__backend_get_value(const cui_widget *w) {
    return protect(w->window->app, 0.,
                   [&] { return state(w).element.as<controls::Primitives::RangeBase>().Value(); });
}
extern "C" void cui__backend_font(cui_widget *w) {
    protect(w->window->app, [&] {
        const char *family = nullptr;
        double points = 0;
        int weight = 0;
        cui__font_resolve(w, &family, &points, &weight);
        auto e = state(w).element;
        auto font = family ? media::FontFamily(wide(family))
                    : w->kind == CUI_CODE
                        ? media::FontFamily(L"Consolas")
                        : resource(L"ContentControlThemeFontFamily").as<media::FontFamily>();
        auto style = cui__font_italic(w) ? winrt::Windows::UI::Text::FontStyle::Italic
                                         : winrt::Windows::UI::Text::FontStyle::Normal;
        double base = w->role == CUI_ROLE_TITLE     ? 28
                      : w->role == CUI_ROLE_HEADING ? 20
                      : w->role == CUI_ROLE_CAPTION ? 12
                                                    : 14;
        double size = (points ? points * 96. / 72. : base) * w->window->app->text_scale;
        if (!weight)
            weight = (w->role == CUI_ROLE_TITLE || w->role == CUI_ROLE_HEADING) ? 600 : 400;
        winrt::Windows::UI::Text::FontWeight fw{static_cast<uint16_t>(weight)};
        if (auto c = e.try_as<controls::Control>()) {
            c.FontFamily(font);
            c.FontSize(size);
            c.FontWeight(fw);
            c.FontStyle(style);
        }
        if (state(w).rows) {
            auto c = state(w).rows;
            c.FontFamily(font);
            c.FontSize(size);
            c.FontWeight(fw);
            c.FontStyle(style);
        }
        if (state(w).time) {
            auto c = state(w).time;
            c.FontFamily(font);
            c.FontSize(size);
            c.FontWeight(fw);
            c.FontStyle(style);
        }
        if (state(w).seconds) {
            auto c = state(w).seconds;
            c.FontFamily(font);
            c.FontSize(size);
            c.FontWeight(fw);
            c.FontStyle(style);
        }
        {
            auto t = e.try_as<controls::TextBlock>();
            if (w->kind == CUI_BADGE)
                t = state(w).border.Child().as<controls::TextBlock>();
            if (t) {
                t.FontFamily(font);
                t.FontSize(size);
                t.FontWeight(fw);
                t.FontStyle(style);
            }
        }
    });
}
extern "C" void cui__backend_role(cui_widget *w) {
    cui__backend_font(w);
    protect(w->window->app, [&] {
        auto &s = state(w);
        if (auto button = s.element.try_as<controls::Button>()) {
            if (w->role == CUI_ROLE_PRIMARY)
                button.Style(resource(L"AccentButtonStyle").as<xaml::Style>());
            else
                button.ClearValue(xaml::FrameworkElement::StyleProperty());
        }
        if (s.border &&
            (w->role == CUI_ROLE_CARD || w->role == CUI_ROLE_PANEL || w->kind == CUI_BADGE)) {
            s.border.Style(
                xaml::Markup::XamlReader::Load(
                    L"<Style xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' "
                    L"TargetType='Border'><Setter Property='Background' Value='{ThemeResource "
                    L"CardBackgroundFillColorDefaultBrush}'/><Setter Property='BorderBrush' "
                    L"Value='{ThemeResource CardStrokeColorDefaultBrush}'/><Setter "
                    L"Property='BorderThickness' Value='1'/><Setter Property='CornerRadius' "
                    L"Value='8'/></Style>")
                    .as<xaml::Style>());
        } else if (s.border)
            s.border.ClearValue(xaml::FrameworkElement::StyleProperty());
        if (w->styled)
            cui__backend_style(w);
        if (w->icon)
            update_icon(w);
    });
}
extern "C" void cui__backend_style(cui_widget *w) {
    protect(w->window->app, [&] {
        auto &s = state(w);
        if (!w->styled) {
            if (auto c = s.element.try_as<controls::Control>()) {
                c.ClearValue(controls::Control::BackgroundProperty());
                c.ClearValue(controls::Control::ForegroundProperty());
                c.ClearValue(controls::Control::BorderBrushProperty());
                c.ClearValue(controls::Control::BorderThicknessProperty());
                c.ClearValue(controls::Control::CornerRadiusProperty());
            } else if (s.border) {
                s.border.ClearValue(controls::Border::BackgroundProperty());
                s.border.ClearValue(controls::Border::BorderBrushProperty());
                s.border.ClearValue(controls::Border::BorderThicknessProperty());
                s.border.ClearValue(controls::Border::CornerRadiusProperty());
            } else if (auto t = s.element.try_as<controls::TextBlock>())
                t.ClearValue(controls::TextBlock::ForegroundProperty());
            cui__backend_role(w);
            return;
        }
        auto st = w->style;
        if (auto c = s.element.try_as<controls::Control>()) {
            c.Background(brush(st.background));
            c.Foreground(brush(st.foreground));
            c.BorderBrush(brush(st.border));
            c.BorderThickness({st.border_width, st.border_width, st.border_width, st.border_width});
            c.CornerRadius({st.radius, st.radius, st.radius, st.radius});
        } else if (s.border) {
            s.border.Background(brush(st.background));
            s.border.BorderBrush(brush(st.border));
            s.border.BorderThickness(
                {st.border_width, st.border_width, st.border_width, st.border_width});
            s.border.CornerRadius({st.radius, st.radius, st.radius, st.radius});
        } else if (auto t = s.element.try_as<controls::TextBlock>())
            t.Foreground(brush(st.foreground));
    });
}
extern "C" void cui__backend_tree_items(cui_widget *w) {
    protect(w->window->app, [&] {
        auto m = static_cast<cui_tree_model *>(w->payload);
        auto &s = state(w);
        auto tree = s.element.as<controls::TreeView>();
        tree.RootNodes().Clear();
        s.nodes.clear();
        if (!m)
            return;
        s.nodes.reserve(m->count);
        for (size_t i = 0; i < m->count; ++i) {
            controls::TreeViewNode n;
            n.Content(winrt::box_value(wide(m->nodes[i].text)));
            n.IsExpanded(m->nodes[i].expanded != 0);
            s.nodes.push_back(n);
        }
        for (size_t i = 0; i < m->count; ++i) {
            auto p = m->nodes[i].parent;
            if (p < 0)
                tree.RootNodes().Append(s.nodes[i]);
            else
                s.nodes.at(p).Children().Append(s.nodes[i]);
        }
    });
}
extern "C" void cui__backend_tree_select(cui_widget *w, cui_tree_node *node) {
    protect(w->window->app, [&] {
        auto m = static_cast<cui_tree_model *>(w->payload);
        auto &s = state(w);
        s.element.as<controls::TreeView>().SelectedNode(node ? s.nodes.at(size_t(node - m->nodes))
                                                             : nullptr);
    });
}
extern "C" void cui__backend_tree_expand(cui_widget *w, cui_tree_node *node) {
    protect(w->window->app, [&] {
        auto m = static_cast<cui_tree_model *>(w->payload);
        state(w).nodes.at(size_t(node - m->nodes)).IsExpanded(node->expanded != 0);
    });
}
extern "C" void cui__backend_number(cui_widget *w) {
    protect(w->window->app, [&] {
        auto n = static_cast<cui_number_state *>(w->payload);
        auto box = state(w).element.as<controls::NumberBox>();
        winrt::Windows::Globalization::NumberFormatting::DecimalFormatter format;
        format.FractionDigits(n->digits);
        format.IntegerDigits(1);
        box.NumberFormatter(format);
        box.Minimum(n->minimum);
        box.Maximum(n->maximum);
        box.SmallChange(n->step);
        box.LargeChange(n->step * 10);
        box.Value(w->value);
    });
}
extern "C" void cui__backend_invalid(cui_widget *w, int invalid) {
    protect(w->window->app, [&] {
        xaml::Automation::AutomationProperties::SetItemStatus(state(w).element,
                                                              invalid ? L"Invalid" : L"");
    });
}
extern "C" void cui__backend_datetime(cui_widget *w) {
    protect(w->window->app, [&] {
        auto s = static_cast<cui_datetime_state *>(w->payload);
        if (w->kind == CUI_DATE) {
            winrt::Windows::Globalization::Calendar c;
            c.ChangeCalendarSystem(L"GregorianCalendar");
            c.Day(1);
            c.Year(s->date.year);
            c.Month(s->date.month);
            c.Day(s->date.day);
            c.Hour(12);
            c.Minute(0);
            c.Second(0);
            auto date = state(w).element.as<controls::CalendarDatePicker>();
            date.Date(winrt::box_value(c.GetDateTime())
                          .as<foundation::IReference<foundation::DateTime>>());
        } else {
            state(w).time.Time(std::chrono::minutes(s->time.hour * 60 + s->time.minute));
            state(w).seconds.Value(s->time.second);
        }
    });
}
