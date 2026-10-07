#pragma once
// Only this private backend sees C++/WinRT. The exported ABI remains C.
#define NOMINMAX
#include <windows.h>
#undef GetCurrentTime
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.NumberFormatting.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Text.h>
extern "C" {
#include "cui_desktop_internal.h"
#include "cui_draw_internal.h"
#include "cui_inputs_internal.h"
#include "cui_navigation_internal.h"
#include "cui_tables_internal.h"
wchar_t *cui__win32_wide(const char *text);
void cui__win_icons_shutdown(void);
}
namespace cui::winui {
namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = xaml::Controls;
namespace media = xaml::Media;
namespace foundation = winrt::Windows::Foundation;
using foundation::IInspectable;
using winrt::hstring;

struct widget_state {
    cui_widget *owner = nullptr;
    xaml::FrameworkElement element{nullptr};
    controls::Canvas children{nullptr};
    controls::Border border{nullptr};
    controls::Image image{nullptr};
    controls::Canvas regions{nullptr};
    controls::ListView rows{nullptr};
    controls::StackPanel headers{nullptr};
    controls::TimePicker time{nullptr};
    controls::NumberBox seconds{nullptr};
    controls::Primitives::Thumb splitter{nullptr};
    std::vector<controls::TreeViewNode> nodes;
    hstring text;
    bool headers_visible = true;
    bool table_connected = false;
    uint64_t table_generation = 0;
    double logical_width = 0, logical_height = 0;
    std::vector<controls::Control> region_buttons;
    std::vector<unsigned> region_ids;
};
struct window_state {
    bool navigating_focus = false;
    xaml::Hosting::DesktopWindowXamlSource island{nullptr};
    controls::Grid root{nullptr};
    controls::Canvas document{nullptr};
    controls::ScrollViewer scroll{nullptr};
    controls::MenuBar menu{nullptr};
    std::vector<std::unique_ptr<widget_state>> widgets;
    ~window_state() {
        // Also close the island when window construction fails partway through.
        if (island) {
            try {
                island.Content(nullptr);
                island.Close();
            } catch (...) {
                OutputDebugStringW(L"CUI: XAML island cleanup failed\n");
            }
        }
    }
};
inline widget_state &state(const cui_widget *w) {
    return *static_cast<widget_state *>(w->native);
}
inline window_state &state(const cui_window *w) {
    return *static_cast<window_state *>(w->content);
}
inline hstring wide(const char *s) {
    return winrt::to_hstring(s ? s : "");
}
// No C++ exceptions may cross the C ABI or a native event callback.
template <class F, class R> R protect(cui_app *app, R fallback, F &&f) noexcept {
    try {
        return f();
    } catch (winrt::hresult_error const &error) {
        OutputDebugStringW(error.message().c_str());
        if (app)
            app->error = "WinUI operation failed (see debugger output)";
        return fallback;
    } catch (...) {
        if (app)
            app->error = "WinUI operation failed";
        return fallback;
    }
}
template <class F> void protect(cui_app *app, F &&f) noexcept {
    protect(app, 0, [&] {
        f();
        return 1;
    });
}
inline media::SolidColorBrush brush(unsigned rgba) {
    return media::SolidColorBrush(winrt::Windows::UI::Color{
        static_cast<uint8_t>(rgba), static_cast<uint8_t>(rgba >> 24),
        static_cast<uint8_t>(rgba >> 16), static_cast<uint8_t>(rgba >> 8)});
}
inline IInspectable resource(const wchar_t *key) {
    return xaml::Application::Current().Resources().Lookup(winrt::box_value(key));
}
inline unsigned modifiers() {
    unsigned m = 0;
    if (GetKeyState(VK_SHIFT) < 0)
        m |= CUI_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) < 0)
        m |= CUI_MOD_CONTROL | CUI_MOD_PRIMARY;
    if (GetKeyState(VK_MENU) < 0)
        m |= CUI_MOD_ALT;
    return m;
}
void connect_input(cui_widget *w);
void create_control(cui_widget *w, widget_state &s);
void update_icon(cui_widget *w);
void update_regions(cui_widget *w);
void update_table(cui_widget *w);
void update_theme(cui_window *w);
media::Imaging::WriteableBitmap bitmap(const uint32_t *pixels, int width, int height);
} // namespace cui::winui
