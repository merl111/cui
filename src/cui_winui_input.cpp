#include "cui_winui.h"
using namespace cui::winui;
namespace {
cui_key key_value(winrt::Windows::System::VirtualKey key) {
    using K = winrt::Windows::System::VirtualKey;
    switch (key) {
    case K::Enter:
        return CUI_KEY_ENTER;
    case K::Escape:
        return CUI_KEY_ESCAPE;
    case K::Back:
        return CUI_KEY_BACKSPACE;
    case K::Tab:
        return CUI_KEY_TAB;
    case K::Up:
        return CUI_KEY_UP;
    case K::Down:
        return CUI_KEY_DOWN;
    case K::Home:
        return CUI_KEY_HOME;
    case K::End:
        return CUI_KEY_END;
    case K::PageUp:
        return CUI_KEY_PAGE_UP;
    case K::PageDown:
        return CUI_KEY_PAGE_DOWN;
    default:
        return static_cast<cui_key>(0);
    }
}
} // namespace
void cui::winui::connect_input(cui_widget *w) {
    auto e = state(w).element;
    e.PreviewKeyDown([w](auto const &, xaml::Input::KeyRoutedEventArgs const &event) {
        protect(w->window->app, [&] {
            auto key = key_value(event.Key());
            if (key && cui__key(w, key, modifiers()))
                event.Handled(true);
        });
    });
}
extern "C" int cui__backend_keys(cui_widget *) {
    return 1;
}
extern "C" int cui__backend_hover(const cui_widget *w) {
    return protect(w->window->app, 0, [&] {
        POINT p{};
        if (!GetCursorPos(&p))
            return 0;
        HWND host = static_cast<HWND>(w->window->native), hit = WindowFromPoint(p);
        if (hit != host && !IsChild(host, hit))
            return 0;
        ScreenToClient(host, &p);
        auto e = state(w).element;
        auto point = e.TransformToVisual(state(w->window).root).TransformPoint({0, 0});
        double x = p.x / w->window->scale - point.X, y = p.y / w->window->scale - point.Y;
        return int(x >= 0 && y >= 0 && x < e.ActualWidth() && y < e.ActualHeight());
    });
}
extern "C" int cui_focus(cui_widget *w) {
    if (!w || !w->native)
        return 0;
    for (auto p = w; p; p = p->parent)
        if (p->hidden || !p->enabled)
            return 0;
    return protect(w->window->app, 0, [&] {
        auto &s = state(w);
        auto e = s.element;
        if (auto c = e.try_as<controls::Control>())
            return int(c.Focus(xaml::FocusState::Programmatic));
        if (s.time)
            return int(s.time.Focus(xaml::FocusState::Programmatic));
        if (s.rows)
            return int(s.rows.Focus(xaml::FocusState::Programmatic));
        if (s.splitter)
            return int(s.splitter.Focus(xaml::FocusState::Programmatic));
        if (s.regions) {
            // Restore the canvas action that owned focus before an app modal.
            if (auto canvas = cui__canvas_state(w))
                for (size_t i = 0; i < s.region_ids.size(); ++i)
                    if (s.region_ids[i] == canvas->focus && s.region_buttons[i].IsEnabled())
                        return int(s.region_buttons[i].Focus(xaml::FocusState::Programmatic));
            for (auto child : s.regions.Children())
                if (auto c = child.try_as<controls::Control>())
                    if (c.IsEnabled())
                        return int(c.Focus(xaml::FocusState::Programmatic));
        }
        return 0;
    });
}
extern "C" int cui_has_focus(const cui_widget *w) {
    if (!w || !w->native)
        return 0;
    return protect(w->window->app, 0, [&] {
        auto element = state(w).element;
        auto root = element.XamlRoot();
        if (!root)
            return 0;
        auto focused =
            xaml::Input::FocusManager::GetFocusedElement(root).try_as<xaml::DependencyObject>();
        while (focused) {
            if (focused == element)
                return 1;
            focused = media::VisualTreeHelper::GetParent(focused);
        }
        return 0;
    });
}
extern "C" void cui_accessibility(cui_widget *w, const char *label, const char *description) {
    if (!w || !w->native)
        return;
    protect(w->window->app, [&] {
        auto e = state(w).element;
        xaml::Automation::AutomationProperties::SetName(e, wide(label));
        xaml::Automation::AutomationProperties::SetHelpText(e, wide(description));
    });
}
extern "C" void cui__backend_announce(cui_widget *w, const char *text, int urgent) {
    protect(w->window->app, [&] {
        auto e = state(w).element;
        auto peer = xaml::Automation::Peers::FrameworkElementAutomationPeer::FromElement(e);
        if (!peer)
            peer = xaml::Automation::Peers::FrameworkElementAutomationPeer::CreatePeerForElement(e);
        if (peer)
            peer.RaiseNotificationEvent(
                xaml::Automation::Peers::AutomationNotificationKind::Other,
                urgent
                    ? xaml::Automation::Peers::AutomationNotificationProcessing::ImportantMostRecent
                    : xaml::Automation::Peers::AutomationNotificationProcessing::MostRecent,
                wide(text), L"CUI.Status");
    });
}
extern "C" void cui_set_read_only(cui_widget *w, int value) {
    if (!w || !w->native)
        return;
    protect(w->window->app, [&] {
        if (auto box = state(w).element.try_as<controls::TextBox>()) {
            w->read_only = !!value;
            box.IsReadOnly(value != 0);
        } else if (w->kind == CUI_PASSWORD)
            w->read_only = !!value;
    });
}
extern "C" void cui_undo(cui_widget *w) {
    if (w && w->native)
        protect(w->window->app, [&] {
            if (auto b = state(w).element.try_as<controls::TextBox>())
                if (!b.IsReadOnly() && b.CanUndo())
                    b.Undo();
        });
}
extern "C" void cui_redo(cui_widget *w) {
    if (w && w->native)
        protect(w->window->app, [&] {
            if (auto b = state(w).element.try_as<controls::TextBox>())
                if (!b.IsReadOnly() && b.CanRedo())
                    b.Redo();
        });
}
extern "C" int cui__backend_insert_text(cui_widget *w, const char *text) {
    return protect(w->window->app, 0, [&] {
        auto b = state(w).element.try_as<controls::TextBox>();
        if (!b) return 0;
        int start = b.SelectionStart();
        auto value = winrt::to_hstring(text);
        b.SelectedText(value);
        b.Select(start + static_cast<int>(value.size()), 0);
        return 1;
    });
}
extern "C" size_t cui_get_selected_text(const cui_widget *w, char *buffer, size_t capacity) {
    if (w && (w->kind == CUI_CANVAS || w->kind == CUI_BOX)) return cui__chat_selected_text(w, buffer, capacity);
    if (!w || !w->native)
        return cui__copy_text("", buffer, capacity);
    return protect(w->window->app, size_t(0), [&] {
        auto b = state(w).element.try_as<controls::TextBox>();
        return cui__copy_text(b ? winrt::to_string(b.SelectedText()).c_str() : "", buffer,
                              capacity);
    });
}
