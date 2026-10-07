#include "cui_winui.h"
#include <MddBootstrap.h>
#include <cstdlib>
#include <cstring>
#include <dwmapi.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
using namespace cui::winui;
namespace {
constexpr wchar_t window_class[] = L"CUI.WinUI3.Window";
struct xaml_app : xaml::ApplicationT<xaml_app, xaml::Markup::IXamlMetadataProvider> {
    xaml::XamlTypeInfo::XamlControlsXamlMetaDataProvider provider;
    xaml::Hosting::WindowsXamlManager manager{nullptr};
    xaml_app() {
        manager = xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
    }
    void OnLaunched(xaml::LaunchActivatedEventArgs const &) {
        Resources().MergedDictionaries().Append(controls::XamlControlsResources());
    }
    xaml::Markup::IXamlType GetXamlType(hstring const &name) {
        return provider.GetXamlType(name);
    }
    xaml::Markup::IXamlType GetXamlType(winrt::Windows::UI::Xaml::Interop::TypeName const &type) {
        return provider.GetXamlType(type);
    }
    winrt::com_array<xaml::Markup::XmlnsDefinition> GetXmlnsDefinitions() {
        return provider.GetXmlnsDefinitions();
    }
};
struct timer_state {
    cui_timer *owner = nullptr;
    winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer timer{nullptr};
};
struct app_state {
    bool apartment = false, bootstrap = false;
    winrt::Microsoft::UI::Dispatching::DispatcherQueueController queue{nullptr};
    winrt::com_ptr<xaml_app> application;
    HMODULE windowing = nullptr;
    BOOL(WINAPI *pretranslate)(const MSG *) = nullptr;
    ~app_state() {
        // Queue shutdown dispatches XAML cleanup before releasing the runtime.
        try {
            if (application && application->manager)
                application->manager.Close();
        } catch (...) {
        }
        try {
            if (queue)
                queue.ShutdownQueue();
        } catch (...) {
        }
        application = nullptr;
        queue = nullptr;
        winrt::clear_factory_cache();
        if (windowing)
            FreeLibrary(windowing);
        if (bootstrap)
            MddBootstrapShutdown();
        if (apartment)
            winrt::uninit_apartment();
    }
};
void navigate_focus(cui_window *w, xaml::Hosting::XamlSourceFocusNavigationReason reason) {
    if (!w->content || state(w).navigating_focus)
        return;
    auto &navigating = state(w).navigating_focus;
    navigating = true;
    struct reset_on_exit {
        bool &value;
        ~reset_on_exit() { value = false; }
    } reset{navigating};
    state(w).island.NavigateFocus(xaml::Hosting::XamlSourceFocusNavigationRequest(reason));
}
void size_island(cui_window *w) {
    if (!w->content)
        return;
    RECT r{};
    GetClientRect(static_cast<HWND>(w->native), &r);
    state(w).island.SiteBridge().MoveAndResize({0, 0, int(r.right), int(r.bottom)});
    cui__backend_refresh(w);
}
LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto w = reinterpret_cast<cui_window *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        w = static_cast<cui_window *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
        w->native = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(w));
    }
    if (!w)
        return DefWindowProcW(hwnd, msg, wp, lp);
    return protect(w->app, LRESULT(0), [&]() -> LRESULT {
        switch (msg) {
        case WM_CLOSE:
            cui_window_close(w);
            return 0;
        case WM_SIZE:
            size_island(w);
            return 0;
        case WM_DPICHANGED: {
            w->scale = float(HIWORD(wp)) / 96.f;
            auto r = reinterpret_cast<RECT *>(lp);
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            size_island(w);
            cui__backend_theme(w->app);
            return 0;
        }
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            if (w->content)
                cui__backend_theme(w->app);
            break;
        case WM_SETFOCUS:
            if (w->content)
                navigate_focus(w,
                    GetKeyState(VK_SHIFT) < 0
                        ? xaml::Hosting::XamlSourceFocusNavigationReason::Last
                        : xaml::Hosting::XamlSourceFocusNavigationReason::First);
            return 0;
        case WM_ACTIVATE:
            if (w->popup && LOWORD(wp) == WA_INACTIVE)
                cui_window_close(w);
            break;
        case WM_MOVE:
            for (auto p = w->app->windows; p; p = p->next)
                if (p->anchor_parent == w && p->visible)
                    cui__backend_window_anchor(p);
            break;
        case WM_ERASEBKGND:
            return 1;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    });
}
} // namespace
extern "C" int cui__backend_init(cui_app *app) {
    return protect(app, 0, [&] {
        auto s = std::make_unique<app_state>();
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        s->apartment = true;
        PACKAGE_VERSION minimum{};
        winrt::check_hresult(MddBootstrapInitialize2(
            0x00010008, L"", minimum, MddBootstrapInitializeOptions_OnPackageIdentity_NOOP));
        s->bootstrap = true;
        s->queue =
            winrt::Microsoft::UI::Dispatching::DispatcherQueueController::CreateOnCurrentThread();
        s->application = winrt::make_self<xaml_app>();
        // Application resources are also needed before the caller enters cui_app_run.
        if (!s->application->Resources().MergedDictionaries().Size())
            s->application->Resources().MergedDictionaries().Append(
                controls::XamlControlsResources());
        s->windowing = LoadLibraryExW(L"Microsoft.UI.Windowing.Core.dll", nullptr, 0);
        if (!s->windowing)
            winrt::throw_last_error();
        s->pretranslate = reinterpret_cast<decltype(s->pretranslate)>(
            GetProcAddress(s->windowing, "ContentPreTranslateMessage"));
        if (!s->pretranslate)
            winrt::throw_last_error();
        WNDCLASSEXW cls{};
        cls.cbSize = sizeof(cls);
        cls.lpfnWndProc = window_proc;
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        // Optional application icon embedded in the consumer executable.
        // LR_SHARED keeps ownership with the module for the class lifetime.
        cls.hIcon = static_cast<HICON>(LoadImageW(cls.hInstance, MAKEINTRESOURCEW(1),
            IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED));
        cls.hIconSm = static_cast<HICON>(LoadImageW(cls.hInstance, MAKEINTRESOURCEW(1),
            IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
        cls.lpszClassName = window_class;
        if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            winrt::throw_last_error();
        app->native = s.release();
        return 1;
    });
}
extern "C" void cui__backend_shutdown(cui_app *app) {
    cui__win_icons_shutdown();
    protect(app, [&] {
        delete static_cast<app_state *>(app->native);
        app->native = nullptr;
    });
    UnregisterClassW(window_class, GetModuleHandleW(nullptr));
}
extern "C" void cui__backend_run(cui_app *app) {
    auto s = static_cast<app_state *>(app->native);
    MSG msg{};
    while (app->running) {
        int status = GetMessageW(&msg, nullptr, 0, 0);
        if (status <= 0) {
            if (status < 0)
                app->error = "GetMessage failed";
            break;
        }
        if (s->pretranslate(&msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}
extern "C" void cui__backend_quit(cui_app *) {}
extern "C" int cui__backend_window_create(cui_window *w, const char *title) {
    int result = protect(w->app, 0, [&] {
        auto s = std::make_unique<window_state>();
        HWND hwnd =
            CreateWindowExW(0, window_class, wide(title).c_str(),
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                            w->width, w->height, nullptr, nullptr, GetModuleHandleW(nullptr), w);
        if (!hwnd)
            winrt::throw_last_error();
        w->scale = float(GetDpiForWindow(hwnd)) / 96.f;
        s->island = xaml::Hosting::DesktopWindowXamlSource();
        s->island.Initialize(winrt::Microsoft::UI::WindowId{reinterpret_cast<uint64_t>(hwnd)});
        s->root = xaml::Markup::XamlReader::Load(
                      L"<Grid xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' "
                      L"Background='{ThemeResource ApplicationPageBackgroundThemeBrush}' />")
                      .as<controls::Grid>();
        s->document = controls::Canvas();
        s->scroll = controls::ScrollViewer();
        controls::RowDefinition menu_row;
        menu_row.Height({1, xaml::GridUnitType::Auto});
        controls::RowDefinition body_row;
        body_row.Height({1, xaml::GridUnitType::Star});
        s->root.RowDefinitions().Append(menu_row);
        s->root.RowDefinitions().Append(body_row);
        controls::Grid::SetRow(s->scroll, 1);
        s->scroll.Content(s->document);
        s->root.Children().Append(s->scroll);
        s->island.Content(s->root);
        s->island.TakeFocusRequested([w](auto const &, auto const &args) {
            protect(w->app, [&] {
                // Wrap at the island boundary. An empty island can synchronously
                // raise this event again, so navigate_focus guards re-entry.
                using reason = xaml::Hosting::XamlSourceFocusNavigationReason;
                navigate_focus(w, args.Request().Reason() == reason::Last ||
                    args.Request().Reason() == reason::Previous ? reason::Last : reason::First);
            });
        });
        s->root.KeyDown([w](auto const &, xaml::Input::KeyRoutedEventArgs const &e) {
            protect(w->app, [&] {
                if (w->popup && e.Key() == winrt::Windows::System::VirtualKey::Escape) {
                    cui_window_close(w);
                    e.Handled(true);
                } else if (cui__desktop_key(w, static_cast<unsigned>(e.Key()),
                                            modifiers() & ~CUI_MOD_PRIMARY))
                    e.Handled(true);
            });
        });
        s->scroll.ViewChanged([w](auto const &sender, auto const &) {
            auto scroll = sender.template as<controls::ScrollViewer>();
            w->scroll_x = float(scroll.HorizontalOffset());
            w->scroll_y = float(scroll.VerticalOffset());
        });
        s->root.SizeChanged([w](auto const &, auto const &) { cui__backend_refresh(w); });
        w->content = s.release();
        cui__backend_scrollable(w);
        update_theme(w);
        return cui__backend_window_size(w);
    });
    if (!result)
        cui__backend_window_destroy(w);
    return result;
}
extern "C" void cui__backend_window_destroy(cui_window *w) {
    protect(w->app, [&] {
        if (w->content) {
            delete static_cast<window_state *>(w->content);
            w->content = nullptr;
        }
        if (w->native)
            DestroyWindow(static_cast<HWND>(w->native));
        w->native = nullptr;
    });
}
extern "C" void cui__backend_window_show(cui_window *w) {
    protect(w->app, [&] {
        update_theme(w);
        ShowWindow(static_cast<HWND>(w->native), SW_SHOW);
        size_island(w);
    });
}
extern "C" void cui__backend_window_hide(cui_window *w) {
    ShowWindow(static_cast<HWND>(w->native), SW_HIDE);
}
extern "C" void cui__backend_refresh(cui_window *w) {
    if (!w->root || !w->content || w->laying_out || w->app->destroying)
        return;
    protect(w->app, [&] {
        RECT r{};
        GetClientRect(static_cast<HWND>(w->native), &r);
        auto &s = state(w);
        double menu_height = s.menu ? s.menu.ActualHeight() : 0;
        cui__layout(w, float(r.right) / w->scale,
                    std::max(0.f, float(r.bottom) / w->scale - float(menu_height)));
        s.document.Width(w->document_width);
        s.document.Height(w->document_height);
    });
}
extern "C" void cui__backend_scrollable(cui_window *w) {
    protect(w->app, [&] {
        auto scroll = state(w).scroll;
        auto visibility = w->scrollable ? controls::ScrollBarVisibility::Auto
                                        : controls::ScrollBarVisibility::Disabled;
        scroll.HorizontalScrollBarVisibility(visibility);
        scroll.VerticalScrollBarVisibility(visibility);
        scroll.HorizontalScrollMode(w->scrollable ? controls::ScrollMode::Enabled
                                                  : controls::ScrollMode::Disabled);
        scroll.VerticalScrollMode(w->scrollable ? controls::ScrollMode::Enabled
                                                : controls::ScrollMode::Disabled);
    });
}
void cui::winui::update_theme(cui_window *w) {
    auto &s = state(w);
    auto theme = w->app->theme;
    s.root.RequestedTheme(theme == CUI_THEME_SYSTEM ? xaml::ElementTheme::Default
                          : theme == CUI_THEME_DARK ? xaml::ElementTheme::Dark
                                                    : xaml::ElementTheme::Light);
    for (auto const &widget : s.widgets) {
        widget->element.UseSystemFocusVisuals(!w->app->hide_focus);
        if (widget->rows)
            widget->rows.UseSystemFocusVisuals(!w->app->hide_focus);
        if (widget->time)
            widget->time.UseSystemFocusVisuals(!w->app->hide_focus);
        if (widget->seconds)
            widget->seconds.UseSystemFocusVisuals(!w->app->hide_focus);
        if (widget->splitter)
            widget->splitter.UseSystemFocusVisuals(!w->app->hide_focus);
        for (auto const &button : widget->region_buttons)
            button.UseSystemFocusVisuals(!w->app->hide_focus);
    }
    BOOL dark = cui_app_resolved_theme(w->app) == CUI_THEME_DARK;
    DwmSetWindowAttribute(static_cast<HWND>(w->native), 20, &dark, sizeof(dark));
    int rounded = w->decorated ? 2 : 1;
    DwmSetWindowAttribute(static_cast<HWND>(w->native), 33, &rounded, sizeof(rounded));
}
extern "C" cui_theme cui__backend_resolved_theme(cui_app *app) {
    return protect(app, CUI_THEME_LIGHT, [&] {
        auto c = winrt::Windows::UI::ViewManagement::UISettings().GetColorValue(
            winrt::Windows::UI::ViewManagement::UIColorType::Background);
        return (int(c.R) + c.G + c.B) < 384 ? CUI_THEME_DARK : CUI_THEME_LIGHT;
    });
}
extern "C" void cui__backend_theme(cui_app *app) {
    protect(app, [&] {
        for (auto w = app->windows; w; w = w->next) {
            update_theme(w);
            cui__font_tree(w->root);
            for (auto const &widget : state(w).widgets)
                if (widget->owner && widget->owner->icon)
                    update_icon(widget->owner);
        }
    });
}
extern "C" int cui__backend_widget_create(cui_widget *w, const char *text) {
    return protect(w->window->app, 0, [&] {
        auto s = std::make_unique<widget_state>();
        s->text = wide(text);
        s->owner = w;
        create_control(w, *s);
        auto &widgets = state(w->window).widgets;
        widgets.reserve(widgets.size() + 1);
        auto parent = w->parent ? state(w->parent).children : state(w->window).document;
        parent.Children().Append(s->element);
        w->native = s.get();
        try {
            connect_input(w);
            s->element.UseSystemFocusVisuals(!w->window->app->hide_focus);
        } catch (...) {
            uint32_t index = 0;
            if (parent.Children().IndexOf(s->element, index))
                parent.Children().RemoveAt(index);
            w->native = nullptr;
            throw;
        }
        widgets.push_back(std::move(s));
        return 1;
    });
}
extern "C" void cui__backend_place(cui_widget *w) {
    protect(w->window->app, [&] {
        auto &s = state(w);
        auto r = w->frame;
        if (w->parent) {
            r.x -= w->parent->frame.x;
            r.y -= w->parent->frame.y;
        }
        controls::Canvas::SetLeft(s.element, r.x);
        controls::Canvas::SetTop(s.element, r.y);
        s.element.Width(std::max(0.f, r.width));
        s.element.Height(std::max(0.f, r.height));
        if (s.splitter && w->first) {
            auto a = w->first->frame;
            controls::Canvas::SetLeft(s.splitter,
                                      w->axis == CUI_HORIZONTAL ? a.x + a.width - w->frame.x : 0);
            controls::Canvas::SetTop(s.splitter,
                                     w->axis == CUI_VERTICAL ? a.y + a.height - w->frame.y : 0);
            s.splitter.Width(w->axis == CUI_HORIZONTAL ? w->gap : r.width);
            s.splitter.Height(w->axis == CUI_VERTICAL ? w->gap : r.height);
        }
        if (w->kind == CUI_CANVAS)
            update_regions(w);
        if (w->kind == CUI_CHART)
            cui__backend_media(w);
    });
}
extern "C" cui_size cui__backend_measure(cui_widget *w) {
    return protect(w->window->app, cui_size{0, 0}, [&] {
        auto e = state(w).element;
        double width = e.Width(), height = e.Height();
        e.Width(NAN);
        e.Height(NAN);
        e.Measure({INFINITY, INFINITY});
        auto d = e.DesiredSize();
        e.Width(width);
        e.Height(height);
        cui_size result{d.Width, d.Height};
        switch (w->kind) {
        case CUI_ENTRY:
        case CUI_SEARCH:
        case CUI_PASSWORD:
        case CUI_NUMBER:
            result.width = std::max(200.f, result.width);
            break;
        case CUI_TEXTAREA:
        case CUI_CODE:
            result = {240, float(w->min_height ? w->min_height : 120)};
            break;
        case CUI_TABLE:
            result = {360, 180};
            break;
        case CUI_TREE:
        case CUI_LIST:
            result = {240, 180};
            break;
        case CUI_IMAGE:
        case CUI_CANVAS:
        case CUI_CHART:
            result = {260, 160};
            break;
        case CUI_ICON:
            result = {float(w->icon_size ? w->icon_size : 20),
                      float(w->icon_size ? w->icon_size : 20)};
            break;
        case CUI_SELECT:
            result.width = std::max(160.f, result.width);
            break;
        case CUI_SLIDER:
            result.width = std::max(160.f, result.width);
            break;
        case CUI_PROGRESS:
            result = {120, 8};
            break;
        case CUI_SPINNER:
            result = {24, 24};
            break;
        case CUI_SEPARATOR:
            result = {1, 1};
            break;
        default:
            break;
        }
        return result;
    });
}
extern "C" void cui__backend_expand(cui_widget *) {}
extern "C" void cui__backend_padding(cui_widget *) {}
extern "C" void cui__backend_container(cui_widget *) {}
extern "C" void cui__backend_grid_cell(cui_widget *) {}
extern "C" void cui__backend_min_size(cui_widget *) {}
extern "C" void cui__backend_split_position(cui_widget *w) {
    cui__backend_refresh(w->window);
}
extern "C" void cui__backend_font_free(cui_widget *) {} // Window owns XAML objects.
extern "C" double cui_window_scale(const cui_window *w) {
    return w ? w->scale : 1;
}
extern "C" double cui_time(void) {
    return double(GetTickCount64()) / 1000.;
}
extern "C" int cui__backend_timer(cui_timer *t) {
    return protect(t->app, 0, [&] {
        auto holder =
            std::make_unique<std::shared_ptr<timer_state>>(std::make_shared<timer_state>());
        auto state = *holder;
        state->owner = t;
        state->timer =
            winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread().CreateTimer();
        state->timer.Interval(std::chrono::milliseconds(t->interval));
        state->timer.IsRepeating(true);
        state->timer.Tick([weak = std::weak_ptr<timer_state>(state)](auto const &, auto const &) {
            auto state = weak.lock();
            auto t = state ? state->owner : nullptr;
            if (t && t->active && !t->app->destroying)
                protect(t->app, [&] { t->task(t->userdata); });
        });
        state->timer.Start();
        t->native = holder.release();
        return 1;
    });
}
extern "C" void cui__backend_timer_stop(cui_timer *t) {
    protect(t->app, [&] {
        auto holder = std::unique_ptr<std::shared_ptr<timer_state>>(
            static_cast<std::shared_ptr<timer_state> *>(t->native));
        t->native = nullptr;
        if (holder) {
            (*holder)->owner = nullptr;
            (*holder)->timer.Stop();
        }
    });
}
