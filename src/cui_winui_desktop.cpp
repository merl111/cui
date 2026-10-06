#include "cui_winui.h"
#include <set>
#include <shobjidl.h>
using namespace cui::winui;
namespace {
using DialogOperation = foundation::IAsyncOperation<controls::ContentDialogResult>;
struct dialog_state : std::enable_shared_from_this<dialog_state> {
    cui_dialog *owner = nullptr;
    controls::ContentDialog dialog;
    DialogOperation operation{nullptr};
    controls::ColorPicker color{nullptr};
    controls::ComboBox family{nullptr};
    controls::NumberBox size{nullptr};
    controls::ComboBox weight{nullptr};
    controls::CheckBox italic{nullptr};
};
int CALLBACK font_name(const LOGFONTW *font, const TEXTMETRICW *, DWORD, LPARAM context) {
    if (font->lfFaceName[0] != L'@')
        reinterpret_cast<std::set<std::wstring> *>(context)->insert(font->lfFaceName);
    return 1;
}
controls::StackPanel font_picker(cui_dialog *d, dialog_state &s) {
    controls::StackPanel panel;
    panel.Spacing(12);
    s.family = controls::ComboBox();
    s.family.IsEditable(true);
    s.family.Header(winrt::box_value(L"Font family"));
    std::set<std::wstring> names;
    LOGFONTW filter{};
    filter.lfCharSet = DEFAULT_CHARSET;
    HDC dc = GetDC(nullptr);
    EnumFontFamiliesExW(dc, &filter, font_name, reinterpret_cast<LPARAM>(&names), 0);
    ReleaseDC(nullptr, dc);
    for (auto const &name : names)
        s.family.Items().Append(winrt::box_value(hstring(name)));
    s.family.Text(wide(d->font.family));
    s.size = controls::NumberBox();
    s.size.Header(winrt::box_value(L"Size (points)"));
    s.size.Minimum(6);
    s.size.Maximum(200);
    s.size.Value(d->font.points);
    s.weight = controls::ComboBox();
    s.weight.Header(winrt::box_value(L"Weight"));
    for (int i = 100; i <= 900; i += 100)
        s.weight.Items().Append(winrt::box_value(winrt::to_hstring(i)));
    s.weight.SelectedIndex(std::clamp(d->font.weight / 100 - 1, 0, 8));
    s.italic = controls::CheckBox();
    s.italic.Content(winrt::box_value(L"Italic"));
    s.italic.IsChecked(winrt::box_value(d->font.italic != 0).as<foundation::IReference<bool>>());
    panel.Children().Append(s.family);
    panel.Children().Append(s.size);
    panel.Children().Append(s.weight);
    panel.Children().Append(s.italic);
    return panel;
}
void finish_dialog(std::shared_ptr<dialog_state> const &s, foundation::AsyncStatus status) {
    auto d = s->owner;
    if (!d)
        return;
    cui_dialog_result result = CUI_DIALOG_FAILED;
    try {
        if (status == foundation::AsyncStatus::Canceled || d->cancelled)
            result = CUI_DIALOG_CANCELLED;
        else if (status == foundation::AsyncStatus::Completed) {
            result = s->operation.GetResults() == controls::ContentDialogResult::Primary
                         ? CUI_DIALOG_ACCEPTED
                         : CUI_DIALOG_CANCELLED;
            if (result == CUI_DIALOG_ACCEPTED && s->color) {
                auto c = s->color.Color();
                d->color = (unsigned(c.R) << 16) | (unsigned(c.G) << 8) | c.B;
            }
            if (result == CUI_DIALOG_ACCEPTED && s->family) {
                auto family = winrt::to_string(s->family.Text());
                if (family.size() >= sizeof(d->font.family) || !std::isfinite(s->size.Value()))
                    result = CUI_DIALOG_FAILED;
                else {
                    cui__copy_text(family.c_str(), d->font.family, sizeof(d->font.family));
                    d->font.points = s->size.Value();
                    d->font.weight = (s->weight.SelectedIndex() + 1) * 100;
                    d->font.italic = s->italic.IsChecked().Value();
                    if (!cui__font_valid(&d->font))
                        result = CUI_DIALOG_FAILED;
                }
            }
        }
    } catch (...) {
        result = CUI_DIALOG_FAILED;
    }
    d->native = nullptr;
    s->owner = nullptr;
    s->operation = nullptr;
    s->dialog = nullptr;
    cui__desktop_finish(d, result, "");
    cui__dialog_release(d);
}
controls::MenuFlyoutItemBase menu_item(cui_menu_item const &item) {
    if (item.submenu) {
        controls::MenuFlyoutSubItem sub;
        sub.Text(wide(item.label));
        for (size_t i = 0; i < item.submenu->count; ++i)
            sub.Items().Append(menu_item(item.submenu->items[i]));
        return sub;
    }
    if (!item.command)
        return controls::MenuFlyoutSeparator();
    auto c = item.command;
    if (c->checkable) {
        controls::ToggleMenuFlyoutItem entry;
        entry.Text(wide(c->label));
        entry.IsChecked(c->checked != 0);
        entry.IsEnabled(c->enabled != 0);
        entry.Click([c](auto const &, auto const &) { cui_command_invoke(c); });
        return entry;
    }
    controls::MenuFlyoutItem entry;
    entry.Text(wide(c->label));
    entry.IsEnabled(c->enabled != 0);
    entry.Click([c](auto const &, auto const &) { cui_command_invoke(c); });
    return entry;
}
controls::MenuFlyout flyout(cui_menu *m) {
    controls::MenuFlyout f;
    for (size_t i = 0; i < m->count; ++i)
        f.Items().Append(menu_item(m->items[i]));
    return f;
}
} // namespace
extern "C" void cui__desktop_open(cui_dialog *d) {
    if (d->kind <= CUI_DIALOG_FOLDER) {
        cui__file_open_native(d);
        return;
    }
    int opened = protect(d->parent->app, 0, [&] {
        auto s = std::make_shared<dialog_state>();
        s->dialog.XamlRoot(state(d->parent).root.XamlRoot());
        s->dialog.RequestedTheme(state(d->parent).root.RequestedTheme());
        s->dialog.Title(winrt::box_value(wide(d->title)));
        s->dialog.CloseButtonText(L"Cancel");
        s->dialog.PrimaryButtonText(d->kind == CUI_DIALOG_ALERT ? wide(d->accept) : L"OK");
        s->dialog.DefaultButton(controls::ContentDialogButton::Primary);
        if (d->kind == CUI_DIALOG_COLOR) {
            s->color = controls::ColorPicker();
            s->color.IsAlphaEnabled(false);
            s->color.Color(
                {255, uint8_t(d->color >> 16), uint8_t(d->color >> 8), uint8_t(d->color)});
            s->dialog.Content(s->color);
        } else if (d->kind == CUI_DIALOG_FONT)
            s->dialog.Content(font_picker(d, *s));
        else {
            controls::TextBlock text;
            text.Text(wide(d->message));
            text.TextWrapping(xaml::TextWrapping::Wrap);
            s->dialog.Content(text);
        }
        s->operation = s->dialog.ShowAsync();
        s->owner = d;
        d->native = s.get();
        cui__dialog_retain(d);
        try {
            s->operation.Completed(
                [s](auto const &, foundation::AsyncStatus status) { finish_dialog(s, status); });
        } catch (...) {
            d->native = nullptr;
            s->owner = nullptr;
            try {
                s->dialog.Hide();
            } catch (...) {
            }
            s->operation = nullptr;
            cui__dialog_release(d);
            throw;
        }
        return 1;
    });
    if (!opened)
        cui__desktop_finish(d, CUI_DIALOG_FAILED, "");
}
extern "C" void cui__desktop_cancel(cui_dialog *d) {
    if (!d->native)
        return;
    if (d->kind <= CUI_DIALOG_FOLDER) {
        static_cast<IFileDialog *>(d->native)->Close(HRESULT_FROM_WIN32(ERROR_CANCELLED));
        return;
    }
    protect(d->parent->app, [&] { static_cast<dialog_state *>(d->native)->dialog.Hide(); });
}
extern "C" void cui__picker_open(cui_dialog *d) {
    cui__desktop_open(d);
}
extern "C" void cui__picker_cancel(cui_dialog *d) {
    cui__desktop_cancel(d);
}
extern "C" void cui__desktop_dispose(cui_app *app) {
    for (auto d = app->dialogs; d; d = d->next) {
        if (!d->native || d->kind <= CUI_DIALOG_FOLDER)
            continue;
        protect(app, [&] {
            auto s = static_cast<dialog_state *>(d->native);
            s->operation.Completed(nullptr);
            s->dialog.Hide();
            d->native = nullptr;
            s->owner = nullptr;
            s->operation = nullptr;
            s->dialog = nullptr;
            cui__dialog_release(d);
        });
    }
}
extern "C" void cui__desktop_window(cui_window *) {}
// The shared command setters already refresh menu bars after model changes.
extern "C" void cui__desktop_command(cui_command *) {}
extern "C" void cui__desktop_menu(cui_window *w) {
    protect(w->app, [&] {
        auto &s = state(w);
        uint32_t index = 0;
        if (s.menu && s.root.Children().IndexOf(s.menu, index))
            s.root.Children().RemoveAt(index);
        s.menu = nullptr;
        if (w->menu) {
            controls::MenuBar bar;
            for (size_t i = 0; i < w->menu->count; ++i) {
                auto item = w->menu->items[i];
                controls::MenuBarItem entry;
                entry.Title(wide(item.label));
                for (size_t j = 0; item.submenu && j < item.submenu->count; ++j)
                    entry.Items().Append(menu_item(item.submenu->items[j]));
                bar.Items().Append(entry);
            }
            s.menu = bar;
            s.root.Children().Append(bar);
        }
        cui__backend_refresh(w);
    });
}
extern "C" int cui__desktop_popup_at(cui_menu *m, cui_widget *anchor, double x, double y, double,
                                     double height) {
    return protect(m->app, 0, [&] {
        auto menu = flyout(m);
        controls::Primitives::FlyoutShowOptions options;
        options.Position(winrt::box_value(foundation::Point{float(x), float(y + height)})
                             .as<foundation::IReference<foundation::Point>>());
        menu.ShowAt(state(anchor).element, options);
        return 1;
    });
}
extern "C" void cui_menu_popup(cui_menu *m, cui_widget *anchor) {
    if (m && anchor && m->app == anchor->window->app)
        cui__desktop_popup_at(m, anchor, 0, 0, anchor->frame.width, anchor->frame.height);
}
extern "C" int cui__desktop_key(cui_window *w, unsigned key, unsigned mods) {
    for (auto c = w->app->commands; c; c = c->next) {
        unsigned expected = (c->modifiers & ~CUI_MOD_PRIMARY) |
                            ((c->modifiers & CUI_MOD_PRIMARY) ? CUI_MOD_CONTROL : 0);
        if (c->key && c->key == key && expected == mods)
            return cui_command_invoke(c);
    }
    return 0;
}
extern "C" int cui__desktop_command_id(cui_window *w, unsigned id) {
    for (auto c = w->app->commands; c; c = c->next)
        if (c->id == id)
            return cui_command_invoke(c);
    return 0;
}
