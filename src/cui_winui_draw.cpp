#include "cui_winui.h"
#include <cstring>
#include <robuffer.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.ViewManagement.h>
using namespace cui::winui;
media::Imaging::WriteableBitmap cui::winui::bitmap(const uint32_t *pixels, int width, int height) {
    media::Imaging::WriteableBitmap result(width, height);
    auto access = result.PixelBuffer().as<::Windows::Storage::Streams::IBufferByteAccess>();
    uint8_t *bytes = nullptr;
    winrt::check_hresult(access->Buffer(&bytes));
    std::memcpy(bytes, pixels, size_t(width) * height * 4);
    result.Invalidate();
    return result;
}
namespace {
struct fit {
    double scale = 1, x = 0, y = 0;
};
fit canvas_fit(cui_widget *w) {
    auto &v = state(w);
    auto e = v.element;
    double width = e.ActualWidth(), height = e.ActualHeight();
    if (width <= 0)
        width = w->frame.width;
    if (height <= 0)
        height = w->frame.height;
    if (v.logical_width <= 0 || v.logical_height <= 0)
        return {};
    double scale = std::min(width / v.logical_width, height / v.logical_height);
    return {scale, (width - v.logical_width * scale) / 2, (height - v.logical_height * scale) / 2};
}
void pointer(cui_widget *w, xaml::Input::PointerRoutedEventArgs const &event,
             cui_canvas_event_kind kind) {
    protect(w->window->app, [&] {
        auto e = state(w).element;
        auto p = event.GetCurrentPoint(e);
        auto f = canvas_fit(w);
        if (f.scale <= 0)
            return;
        auto pos = p.Position();
        double dx = 0, dy = 0;
        if (kind == CUI_CANVAS_SCROLL) {
            auto props = p.Properties();
            if (props.IsHorizontalMouseWheel())
                dx = -props.MouseWheelDelta() / 120.;
            else
                dy = -props.MouseWheelDelta() / 120.;
        }
        if (kind == CUI_CANVAS_PRESS && !p.Properties().IsLeftButtonPressed())
            return;
        if (kind == CUI_CANVAS_PRESS)
            e.CapturePointer(event.Pointer());
        if (kind == CUI_CANVAS_RELEASE)
            e.ReleasePointerCapture(event.Pointer());
        cui__canvas_event(w, kind, (pos.X - f.x) / f.scale, (pos.Y - f.y) / f.scale, dx, dy,
                          modifiers());
        if (kind != CUI_CANVAS_MOVE)
            event.Handled(true);
    });
}
} // namespace
extern "C" int cui__canvas_attach(cui_widget *w) {
    return protect(w->window->app, 0, [&] {
        auto e = state(w).element;
        e.as<controls::Grid>().Background(brush(0));
        e.KeyDown([w](auto const &, xaml::Input::KeyRoutedEventArgs const &event) {
            if (event.Key() == winrt::Windows::System::VirtualKey::C &&
                (modifiers() & CUI_MOD_PRIMARY) && cui__canvas_copy(w)) event.Handled(true);
        });
        e.PointerPressed([w](auto const &, auto const &v) { pointer(w, v, CUI_CANVAS_PRESS); });
        e.PointerReleased([w](auto const &, auto const &v) { pointer(w, v, CUI_CANVAS_RELEASE); });
        e.PointerMoved([w](auto const &, auto const &v) { pointer(w, v, CUI_CANVAS_MOVE); });
        e.PointerWheelChanged(
            [w](auto const &, auto const &v) { pointer(w, v, CUI_CANVAS_SCROLL); });
        e.PointerExited([w](auto const &, auto const &) {
            cui__canvas_event(w, CUI_CANVAS_MOVE, -1, -1, 0, 0, modifiers());
        });
        e.RightTapped([w](auto const &, xaml::Input::RightTappedRoutedEventArgs const &v) {
            protect(w->window->app, [&] {
                auto p = v.GetPosition(state(w).element);
                auto f = canvas_fit(w);
                if (f.scale > 0)
                    cui__canvas_event(w, CUI_CANVAS_CONTEXT, (p.X - f.x) / f.scale,
                                      (p.Y - f.y) / f.scale, 0, 0, modifiers());
                v.Handled(true);
            });
        });
        e.SizeChanged([w](auto const &, auto const &) {
            protect(w->window->app, [&] { update_regions(w); });
        });
        return 1;
    });
}
void cui::winui::update_regions(cui_widget *w) {
    auto s = cui__canvas_state(w);
    if (!s)
        return;
    auto &v = state(w);
    auto f = canvas_fit(w);
    bool enabled = true;
    for (auto p = w; p; p = p->parent)
        enabled = enabled && p->enabled;
    for (size_t i = 0; i < s->count && i < v.region_buttons.size(); ++i) {
        auto b = v.region_buttons[i];
        auto r = s->regions[i];
        controls::Canvas::SetLeft(b, f.x + r.x * f.scale);
        controls::Canvas::SetTop(b, f.y + r.y * f.scale);
        b.Width(r.width * f.scale);
        b.Height(r.height * f.scale);
        b.IsEnabled(r.enabled != 0 && enabled);
        b.UseSystemFocusVisuals(!w->window->app->hide_focus);
    }
}
static controls::Control region_control(cui_widget *w, const cui_canvas_region &r,
                                        const std::vector<controls::Control> &old,
                                        const std::vector<unsigned> &ids) {
    for (size_t j = 0; j < ids.size(); ++j)
        if (ids[j] == r.id &&
            bool(old[j].try_as<controls::TextBox>()) == (r.role == CUI_CANVAS_TEXT)) {
            return old[j];
        }
    controls::Control b{nullptr};
    if (r.role == CUI_CANVAS_TEXT) {
        controls::TextBox text;
        text.IsReadOnly(true);
        text.TextWrapping(xaml::TextWrapping::Wrap);
        text.Opacity(0);
        b = text;
    } else {
        auto button = xaml::Markup::XamlReader::Load(
                          L"<Button "
                          L"xmlns='http://schemas.microsoft.com/winfx/2006/xaml/"
                          L"presentation' "
                          L"UseSystemFocusVisuals='True'><Button.Template><ControlTemplate "
                          L"TargetType='Button'><Border Background='Transparent'/>"
                          L"</ControlTemplate></Button.Template></Button>")
                          .as<controls::Button>();
        button.Click(
            [w, id = r.id](auto const &, auto const &) { cui_canvas_activate_region(w, id); });
        b = button;
    }
    b.IsHitTestVisible(false);
    b.IsTabStop(true);
    b.GotFocus([w, id = r.id](auto const &, auto const &) { cui__canvas_focus(w, id); });
    return b;
}
extern "C" void cui__canvas_regions(cui_widget *w) {
    protect(w->window->app, [&] {
        auto s = cui__canvas_state(w);
        auto &v = state(w);
        auto old = v.region_buttons;
        auto ids = v.region_ids;
        std::vector<controls::Control> next;
        std::vector<unsigned> next_ids;
        unsigned focused = 0;
        for (size_t i = 0; i < old.size(); ++i)
            if (old[i].FocusState() != xaml::FocusState::Unfocused)
                focused = ids[i];
        for (size_t i = 0; i < s->count; ++i) {
            auto r = s->regions[i];
            auto b = region_control(w, r, old, ids);
            if (auto text = b.try_as<controls::TextBox>()) {
                auto value = wide(r.label);
                if (text.Text() != value)
                    text.Text(value);
            }
            xaml::Automation::AutomationProperties::SetName(
                b, r.role == CUI_CANVAS_TEXT ? hstring{} : wide(r.label));
            next.push_back(b);
            next_ids.push_back(r.id);
        }
        auto children = v.regions.Children();
        for (uint32_t i = children.Size(); i > 0; --i) {
            auto child = children.GetAt(i - 1);
            if (std::find(next.begin(), next.end(), child) == next.end())
                children.RemoveAt(i - 1);
        }
        for (uint32_t i = 0; i < next.size(); ++i) {
            uint32_t position = 0;
            if (children.IndexOf(next[i], position)) {
                if (position == i)
                    continue;
                children.RemoveAt(position);
            }
            children.InsertAt(i, next[i]);
        }
        v.region_buttons = std::move(next);
        v.region_ids = std::move(next_ids);
        update_regions(w);
        if (focused)
            for (size_t i = 0; i < v.region_ids.size(); ++i)
                if (v.region_ids[i] == focused &&
                    v.region_buttons[i].FocusState() == xaml::FocusState::Unfocused)
                    v.region_buttons[i].Focus(xaml::FocusState::Programmatic);
    });
}
extern "C" int cui__canvas_present(cui_widget *w, cui_surface *surface, double opacity) {
    return protect(w->window->app, 0, [&] {
        auto image = bitmap(surface->pixels, surface->width, surface->height);
        auto &v = state(w);
        v.image.Source(image);
        v.image.Opacity(opacity);
        v.logical_width = surface->width / surface->scale;
        v.logical_height = surface->height / surface->scale;
        w->image_width = surface->width;
        w->image_height = surface->height;
        update_regions(w);
        return 1;
    });
}
extern "C" int cui__native_opacity(cui_widget *w, double value) {
    return protect(w->window->app, 0, [&] {
        state(w).element.Opacity(value);
        return 1;
    });
}
void cui::winui::update_icon(cui_widget *w) {
    auto &s = state(w);
    if (!w->icon) {
        if (w->kind == CUI_ICON)
            s.image.Source(nullptr);
        else if (auto c = s.element.try_as<controls::ContentControl>())
            c.Content(winrt::box_value(s.text));
        return;
    }
    int side = w->icon_size ? w->icon_size : 20;
    int pixels = int(std::ceil(side * cui_window_scale(w->window)));
    auto color = winrt::Windows::UI::ViewManagement::UISettings().GetColorValue(
        winrt::Windows::UI::ViewManagement::UIColorType::Foreground);
    if (!winrt::Windows::UI::ViewManagement::AccessibilitySettings().HighContrast()) {
        auto channel = uint8_t(cui_app_resolved_theme(w->window->app) == CUI_THEME_DARK ? 255 : 24);
        color = {255, channel, channel, channel};
    }
    unsigned rgba =
        (unsigned(color.R) << 24) | (unsigned(color.G) << 16) | (unsigned(color.B) << 8) | color.A;
    auto data = std::unique_ptr<uint32_t, decltype(&std::free)>(
        cui__draw_asset(w->icon, pixels, pixels, rgba), &std::free);
    if (!data)
        throw std::bad_alloc();
    controls::Image icon;
    icon.Source(bitmap(data.get(), pixels, pixels));
    icon.Width(side);
    icon.Height(side);
    icon.Stretch(media::Stretch::Uniform);
    if (w->kind == CUI_ICON) {
        s.image.Source(icon.Source());
        return;
    }
    controls::StackPanel content;
    content.Orientation(controls::Orientation::Horizontal);
    content.Spacing(8);
    controls::TextBlock text;
    text.Text(s.text);
    text.VerticalAlignment(xaml::VerticalAlignment::Center);
    if (!w->icon_trailing)
        content.Children().Append(icon);
    if (!w->icon_only)
        content.Children().Append(text);
    if (w->icon_trailing)
        content.Children().Append(icon);
    s.element.as<controls::ContentControl>().Content(content);
    if (w->icon_only)
        xaml::Automation::AutomationProperties::SetName(s.element, s.text);
}
extern "C" void cui__backend_icon(cui_widget *w) {
    protect(w->window->app, [&] { update_icon(w); });
}
extern "C" void cui__backend_media(cui_widget *w) {
    protect(w->window->app, [&] {
        auto &s = state(w);
        if (w->kind == CUI_CHART) {
            s.children.Children().Clear();
            if (w->series_count < 2)
                return;
            auto bounds = std::minmax_element(w->series, w->series + w->series_count);
            double range = *bounds.second - *bounds.first;
            double width = std::max(1.f, w->frame.width), height = std::max(1.f, w->frame.height);
            xaml::Shapes::Polyline line;
            line.Stroke(resource(L"AccentFillColorDefaultBrush").as<media::Brush>());
            line.StrokeThickness(2);
            for (size_t i = 0; i < w->series_count; ++i)
                line.Points().Append(
                    {float(i * width / (w->series_count - 1)),
                     float(height -
                           (range ? (w->series[i] - *bounds.first) / range : .5) * height)});
            s.children.Children().Append(line);
            return;
        }
        if (!w->pixels || !s.image)
            return;
        std::vector<uint32_t> pixels(size_t(w->image_width) * w->image_height);
        for (size_t i = 0; i < pixels.size(); ++i) {
            auto p = w->pixels + i * 4;
            unsigned a = p[3];
            pixels[i] =
                (a << 24) | (p[0] * a / 255 << 16) | (p[1] * a / 255 << 8) | (p[2] * a / 255);
        }
        s.image.Source(bitmap(pixels.data(), w->image_width, w->image_height));
    });
}
