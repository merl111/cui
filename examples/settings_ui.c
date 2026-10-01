#include "settings_ui.h"
#include <string.h>

static cui_widget *label(cui_widget *parent, const char *text, cui_role role)
{
    cui_widget *widget = cui_label(parent, text);
    cui_set_role(widget, role);
    return widget;
}

static cui_widget *card(cui_widget *parent)
{
    cui_widget *box = cui_box(parent, CUI_VERTICAL, 14);
    cui_box_set_padding(box, 20);
    cui_set_role(box, CUI_ROLE_CARD);
    return box;
}

static void choose_theme(cui_widget *sender, void *userdata)
{
    settings_demo *demo = (settings_demo *)userdata;
    cui_theme theme = sender == demo->dark ? CUI_THEME_DARK :
                      sender == demo->light ? CUI_THEME_LIGHT : CUI_THEME_SYSTEM;
    cui_app_set_theme(demo->app, theme);
    cui_set_role(demo->system, sender == demo->system ? CUI_ROLE_PRIMARY : CUI_ROLE_BODY);
    cui_set_role(demo->light, sender == demo->light ? CUI_ROLE_PRIMARY : CUI_ROLE_BODY);
    cui_set_role(demo->dark, sender == demo->dark ? CUI_ROLE_PRIMARY : CUI_ROLE_BODY);
}

static void changed(cui_widget *sender, void *userdata)
{
    settings_demo *demo = (settings_demo *)userdata;
    (void)sender;
    cui_set_text(demo->status, "You have unsaved changes.");
    cui_set_enabled(demo->save, 1);
}

static void save(cui_widget *sender, void *userdata)
{
    settings_demo *demo = (settings_demo *)userdata;
    (void)sender;
    /* A UI example: deliberately no filesystem or persistence side effects. */
    cui_set_text(demo->status, "Changes applied for this session.");
    cui_set_enabled(demo->save, 0);
}

int settings_demo_create(settings_demo *demo, cui_app *app)
{
    cui_widget *root, *intro, *profile, *field, *row, *appearance, *space, *footer;
    memset(demo, 0, sizeof(*demo));
    demo->app = app;
    demo->window = cui_window_create(app, "Workspace settings", 640, 700);
    if (!demo->window) return 0;
    root = cui_window_root(demo->window);
    cui_box_set_padding(root, 32);

    intro = cui_box(root, CUI_VERTICAL, 8);
    label(intro, "WORKSPACE  /  PREFERENCES", CUI_ROLE_CAPTION);
    label(intro, "Your workspace", CUI_ROLE_TITLE);
    label(intro, "A quieter place to focus. Make it yours.", CUI_ROLE_CAPTION);
    cui_box_set_padding(intro, 4);

    profile = card(root);
    label(profile, "Personal details", CUI_ROLE_HEADING);
    field = cui_box(profile, CUI_VERTICAL, 7);
    label(field, "Display name", CUI_ROLE_BODY);
    demo->name = cui_entry(field, "Alex Morgan");
    field = cui_box(profile, CUI_VERTICAL, 7);
    label(field, "Workspace", CUI_ROLE_BODY);
    demo->workspace = cui_entry(field, "Studio North");
    demo->notifications = cui_checkbox(profile, "Keep me in the loop with desktop notifications", 1);

    appearance = card(root);
    label(appearance, "Appearance", CUI_ROLE_HEADING);
    label(appearance, "Choose a theme, or follow your system.", CUI_ROLE_CAPTION);
    row = cui_box(appearance, CUI_HORIZONTAL, 10);
    demo->system = cui_button(row, "System");
    demo->light = cui_button(row, "Light");
    demo->dark = cui_button(row, "Dark");
    cui_expand(demo->system, 1);
    cui_expand(demo->light, 1);
    cui_expand(demo->dark, 1);
    cui_set_role(demo->system, CUI_ROLE_PRIMARY);
    cui_on_action(demo->system, choose_theme, demo);
    cui_on_action(demo->light, choose_theme, demo);
    cui_on_action(demo->dark, choose_theme, demo);

    space = cui_box(root, CUI_VERTICAL, 0);
    cui_expand(space, 1);
    footer = cui_box(root, CUI_HORIZONTAL, 16);
    demo->status = label(footer, "Everything is up to date.", CUI_ROLE_CAPTION);
    cui_expand(demo->status, 1);
    demo->save = cui_button(footer, "Save changes");
    cui_set_role(demo->save, CUI_ROLE_PRIMARY);
    cui_on_action(demo->save, save, demo);
    cui_on_action(demo->name, changed, demo);
    cui_on_action(demo->workspace, changed, demo);
    cui_on_action(demo->notifications, changed, demo);
    return demo->save && demo->name && demo->workspace && demo->notifications &&
           demo->system && demo->light && demo->dark && !cui_app_error(app)[0];
}
