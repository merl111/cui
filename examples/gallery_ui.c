#include "gallery_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static cui_widget *caption(cui_widget *parent, const char *text)
{
    cui_widget *w = cui_label(parent, text); cui_set_role(w, CUI_ROLE_CAPTION); return w;
}
static cui_widget *section(cui_widget *parent, const char *title, const char *subtitle)
{
    cui_widget *box = cui_box(parent, CUI_VERTICAL, 12);
    cui_box_set_padding(box, 20); cui_set_role(box, CUI_ROLE_CARD); cui_expand(box, 1);
    cui_widget *heading = cui_label(box, title); cui_set_role(heading, CUI_ROLE_HEADING);
    if (subtitle) caption(box, subtitle);
    return box;
}
static void theme_changed(cui_widget *sender, void *data)
{
    gallery_demo *demo = (gallery_demo *)data;
    int selected = cui_get_selected(sender);
    if (selected >= 0) cui_app_set_theme(demo->app, (cui_theme)selected);
}
static void value_changed(cui_widget *sender, void *data)
{
    gallery_demo *demo = (gallery_demo *)data;
    char text[64]; double value = cui_get_value(sender);
    cui_set_value(demo->progress, value);
    snprintf(text, sizeof(text), "Progress  %.0f%%", value * 100);
    cui_set_text(demo->meter, text);
    cui_set_text(demo->status, "Slider updated the progress indicator.");
}
static void selection_changed(cui_widget *sender, void *data)
{
    gallery_demo *demo = (gallery_demo *)data;
    char text[80];
    snprintf(text, sizeof(text), "Selected row %d. Native selection, shared C API.", cui_get_selected(sender) + 1);
    cui_set_text(demo->status, text);
}
static int contains(const char *text, const char *query)
{
    size_t i, j, n = strlen(query);
    for (i = 0; text[i] || !n; ++i) {
        for (j = 0; j < n && text[i + j] && tolower((unsigned char)text[i + j]) == tolower((unsigned char)query[j]); ++j) {}
        if (j == n) return 1;
        if (!text[i]) break;
    }
    return 0;
}
static const char *records[] = {
    "Design system", "In progress", "Maya", "Native controls", "Completed", "Alex",
    "Accessibility review", "To do", "Sam", "Language bindings", "In progress", "Jordan",
    "Release checklist", "To do", "Alex"
};
static void filter_changed(cui_widget *sender, void *data)
{
    gallery_demo *demo = (gallery_demo *)data;
    const char *cells[15]; char query[256]; size_t row, count = 0;
    cui_get_text(sender, query, sizeof(query));
    for (row = 0; row < 5; ++row)
        if (contains(records[row * 3], query) || contains(records[row * 3 + 1], query)) {
            cells[count++] = records[row * 3]; cells[count++] = records[row * 3 + 1]; cells[count++] = records[row * 3 + 2];
        }
    cui_table_set_rows(demo->table, cells, count / 3);
    cui_set_text(demo->status, count ? "Showing matching records." : "No matching records. Try a different search.");
}
static void approved(cui_widget *sender, void *data)
{
    gallery_demo *demo = (gallery_demo *)data;
    cui_set_text(demo->approval_status, "Approved for this demo");
    cui_set_role(demo->approval_status, CUI_ROLE_SUCCESS);
    cui_set_enabled(sender, 0);
    cui_set_text(demo->status, "Approval recorded locally. No external action was performed.");
}
static void run_task(cui_widget *sender, void *data)
{
    gallery_demo *demo = (gallery_demo *)data;
    cui_set_text(demo->task_badge, "Completed"); cui_set_role(demo->task_badge, CUI_ROLE_SUCCESS);
    cui_set_visible(demo->spinner, 0); cui_set_enabled(sender, 0);
    cui_set_text(demo->status, "Demo task completed.");
}
static void send_message(cui_widget *sender, void *data)
{
    gallery_demo *demo = (gallery_demo *)data;
    size_t length = cui_get_text(demo->composer, NULL, 0);
    char *text;
    (void)sender;
    if (!length) { cui_set_text(demo->status, "Write a message first."); return; }
    text = (char *)malloc(length + 1);
    if (!text) return;
    cui_get_text(demo->composer, text, length + 1);
    cui_set_text(demo->reply, text); cui_set_text(demo->composer, "");
    cui_set_text(demo->status, "Message echoed locally. No AI service or network request.");
    free(text);
}
static void controls_page(gallery_demo *demo, cui_widget *page)
{
    const char *options[] = {"Personal workspace", "Team workspace", "Shared workspace"};
    cui_widget *row = cui_box(page, CUI_HORIZONTAL, 16);
    cui_widget *inputs = section(row, "Inputs", "Native editing, selection and keyboard behavior.");
    cui_label(inputs, "Display name"); cui_entry(inputs, "Alex Morgan");
    cui_label(inputs, "Password"); demo->password = cui_password(inputs, "correct-horse");
    cui_set_tooltip(demo->password, "A native password field; text is masked.");
    cui_label(inputs, "Workspace type"); demo->select = cui_select(inputs, options, 3);
    cui_set_selected(demo->select, 0);
    cui_search(inputs, "Search anything...");
    cui_widget *selection = section(row, "Choices", "Small controls with clear, predictable states.");
    demo->toggle_switch = cui_switch(selection, "Desktop notifications", 1);
    cui_checkbox(selection, "Include archived projects", 0);
    cui_separator(selection);
    cui_widget *group = cui_box(selection, CUI_VERTICAL, 4);
    demo->radios[0] = cui_radio(group, "Compact density", 0);
    demo->radios[1] = cui_radio(group, "Comfortable density", 1);
    demo->radios[2] = cui_radio(group, "Spacious density", 0);
    demo->toggle = cui_toggle(selection, "Pin this workspace", 0);
    cui_widget *feedback = section(page, "Feedback & status", "Drag the slider. The progress bar and percentage stay in sync.");
    cui_widget *badges = cui_box(feedback, CUI_HORIZONTAL, 8);
    cui_badge(badges, "Ready", CUI_ROLE_SUCCESS); cui_badge(badges, "Needs review", CUI_ROLE_WARNING);
    cui_badge(badges, "Failed", CUI_ROLE_DANGER); cui_badge(badges, "In progress", CUI_ROLE_BODY);
    demo->meter = caption(feedback, "Progress  64%");
    demo->slider = cui_slider(feedback, 0.64); demo->progress = cui_progress(feedback, 0.64);
    cui_on_action(demo->slider, value_changed, demo);
}
static void data_page(gallery_demo *demo, cui_widget *page)
{
    const char *headers[] = {"Project", "Status", "Owner"};
    const char *items[] = {"Overview", "My tasks", "Activity", "Saved views", "Settings"};
    cui_widget *data = section(page, "Records", "Search names or statuses. Select a row to inspect its index.");
    demo->filter = cui_search(data, "Filter projects or status...");
    demo->table = cui_table(data, headers, 3);
    cui_table_set_rows(demo->table, records, 5); cui_set_min_size(demo->table, 600, 230);
    cui_on_action(demo->filter, filter_changed, demo); cui_on_action(demo->table, selection_changed, demo);
    cui_widget *row = cui_box(page, CUI_HORIZONTAL, 16);
    cui_widget *navigation = section(row, "Navigation list", "Single selection with native keyboard support.");
    demo->list = cui_list(navigation, items, 5); cui_set_selected(demo->list, 0);
    cui_on_action(demo->list, selection_changed, demo);
    cui_widget *details = section(row, "Disclosure", "Extra context, available when you need it.");
    demo->disclosure = cui_disclosure(details, "Show implementation details", 0);
    cui_widget *body = cui_disclosure_content(demo->disclosure);
    cui_label(body, "C API. Native widgets. No bundled runtime.");
    cui_badge(body, "Reusable composition", CUI_ROLE_SUCCESS);
}
static void patterns_page(gallery_demo *demo, cui_widget *page)
{
    cui_widget *row = cui_box(page, CUI_HORIZONTAL, 16);
    cui_widget *approval = section(row, "Approval", "Let people make a clear choice before proceeding.");
    demo->approval_status = cui_badge(approval, "Waiting for approval", CUI_ROLE_WARNING);
    cui_label(approval, "Publish the reviewed changes?");
    cui_widget *options = cui_box(approval, CUI_VERTICAL, 4);
    cui_radio(options, "Keep as a draft", 1); cui_radio(options, "Ready for review", 0);
    demo->approve = cui_button(approval, "Approve demo"); cui_set_role(demo->approve, CUI_ROLE_PRIMARY);
    cui_on_action(demo->approve, approved, demo);
    cui_widget *task = section(row, "Task activity", "Compact status, supporting detail and actions.");
    demo->task_badge = cui_badge(task, "In progress", CUI_ROLE_BODY);
    cui_label(task, "Validate the component library");
    demo->spinner = cui_spinner(task);
    caption(task, "Keyboard paths and appearance checks");
    demo->task_run = cui_button(task, "Complete demo task"); cui_on_action(demo->task_run, run_task, demo);
    cui_widget *chat = section(page, "Composer", "A local interaction example built from standard controls.");
    demo->reply = cui_textarea(chat, "Your message will appear here."); cui_set_enabled(demo->reply, 0);
    cui_set_min_size(demo->reply, 0, 80);
    demo->composer = cui_textarea(chat, "Help me plan the next release."); cui_set_min_size(demo->composer, 0, 90);
    demo->send = cui_button(chat, "Send locally"); cui_set_role(demo->send, CUI_ROLE_PRIMARY);
    cui_on_action(demo->send, send_message, demo);
}
static void code_page(gallery_demo *demo, cui_widget *page)
{
    cui_widget *code = section(page, "Code surface", "Selectable, read-only text in the platform's monospace font.");
    demo->code = cui_code(code, "cui_widget *slider = cui_slider(parent, 0.64);\ncui_on_action(slider, changed, progress);\n\nstatic void changed(cui_widget *sender, void *data)\n{\n    cui_set_value(data, cui_get_value(sender));\n}\n");
    cui_set_min_size(demo->code, 680, 240);
    cui_widget *states = section(page, "Control states", "Disabled controls remain visible and cannot be activated.");
    cui_widget *row = cui_box(states, CUI_HORIZONTAL, 10);
    cui_button(row, "Secondary"); cui_widget *primary = cui_button(row, "Primary"); cui_set_role(primary, CUI_ROLE_PRIMARY);
    cui_widget *disabled = cui_button(row, "Unavailable"); cui_set_enabled(disabled, 0);
    cui_widget *entry = cui_entry(states, "A disabled text field"); cui_set_enabled(entry, 0);
    cui_checkbox(states, "An ordinary checkbox", 1);
    caption(states, "Hover, press Tab, and use Space or the arrow keys to explore.");
}

static void choose_pattern(cui_widget *sender, void *data)
{
    gallery_demo *demo = (gallery_demo *)data;
    int selected = cui_get_selected(sender);
    for (int i = 0; i < CUI_PATTERN_COUNT; ++i) cui_set_visible(demo->patterns[i], i == selected);
}
static void stream_sample(void *data)
{
    gallery_demo *demo = (gallery_demo *)data;
    const char *chunks[] = {"Native controls ", "keep text crisp ", "and keyboard interaction familiar.\n", "This is a local streaming demonstration."};
    if (demo->stream_step < 4) cui_stream_append(demo->patterns[CUI_STREAMING], chunks[demo->stream_step++]);
    else { cui_timer_stop(demo->stream_timer); cui_pattern_set_busy(demo->patterns[CUI_STREAMING], 0); }
}
static void pattern_action(cui_widget *sender, void *data)
{
    gallery_demo *demo = (gallery_demo *)data;
    cui_event event = cui_pattern_event(sender);
    cui_widget *status = cui_pattern_part(sender, CUI_PART_STATUS);
    if (sender == demo->patterns[CUI_CHAT] && event == CUI_EVENT_SUBMIT) {
        char text[2048]; cui_get_text(cui_pattern_part(sender, CUI_PART_INPUT), text, sizeof(text));
        static cui_item_id message_id=100;
        const cui_pattern_item message={message_id++,"You",text,"Just now","",0,CUI_ROLE_SUBTLE,0};
        if(*text&&cui_pattern_upsert_item(sender,&message))cui_set_text(cui_pattern_part(sender,CUI_PART_INPUT),"");
        cui_set_text(status, "Local message · no AI service connected");
    } else if(event==CUI_EVENT_CANCEL&&cui_pattern_item_event_id(sender)) {
        cui_pattern_remove_item(sender,cui_pattern_item_event_id(sender));
    } else if (sender == demo->patterns[CUI_LOADING]) {
        cui_pattern_set_busy(sender, 0); cui_set_text(status, "Cancelled");
    } else if (sender == demo->patterns[CUI_STREAMING] && event == CUI_EVENT_CANCEL) {
        cui_timer_stop(demo->stream_timer); cui_pattern_set_busy(sender, 0);
    } else if (sender == demo->patterns[CUI_AGENT_SCREEN]) {
        cui_window *preview = cui_window_create(demo->app, "Example frame · local preview", 700, 460);
        cui_label(cui_window_root(preview), "Application-supplied frame · no screen capture running");
        cui_widget *image = cui_image(cui_window_root(preview));
        unsigned char pixels[64*40*4];
        for (int i = 0; i < 64*40; ++i) { pixels[i*4] = 40; pixels[i*4+1] = (unsigned char)(70+i%64); pixels[i*4+2] = 150; pixels[i*4+3] = 255; }
        cui_image_set_rgba(image, pixels, 64, 40); cui_window_show(preview);
    } else if (sender == demo->patterns[CUI_SELECTION_ACTIONS]) {
        char text[512]; cui_get_selected_text(cui_pattern_part(sender, CUI_PART_BODY), text, sizeof(text));
        cui_set_text(status, *text ? text : "Select some text first; transformation is an application callback.");
    } else if (event == CUI_EVENT_DICTATE) cui_set_text(status, "Dictation requested · connect your speech service");
    else if (event == CUI_EVENT_SUBMIT) cui_set_text(status, "Received locally · application callback fired");
}
static void reference_page(gallery_demo *demo, cui_widget *page)
{
    const char *titles[CUI_PATTERN_COUNT];
    for (int i = 0; i < CUI_PATTERN_COUNT; ++i) titles[i] = cui_pattern_name((cui_pattern)i);
    caption(page, "All 21 Beautiful UI families · native compositions · local sample data");
    demo->pattern_picker = cui_select(page, titles, CUI_PATTERN_COUNT);
    cui_set_selected(demo->pattern_picker, 0); cui_on_action(demo->pattern_picker, choose_pattern, demo);
    const char *cells[] = {"Design system", "Active", "Shared tokens and components", "Release notes", "Draft", "Changes ready for review", "Accessibility", "Active", "Keyboard and readable typography"};
    for (int i = 0; i < CUI_PATTERN_COUNT; ++i) {
        cui_widget *pattern = cui_pattern_create(page, (cui_pattern)i, NULL);
        demo->patterns[i] = pattern; cui_set_visible(pattern, i == 0); cui_on_action(pattern, pattern_action, demo);
        if (i >= CUI_CONTEXT && i <= CUI_SEARCH_PANEL) cui_pattern_set_records(pattern, cells, 3);
    }
    const cui_pattern_item items[]={{1,"Review typography","Check labels at 150% text scale.","Workspace · Today","In progress",.5,CUI_ROLE_SUBTLE,CUI_ITEM_EXPANDED},{2,"Release notes","Changes ready for review.","Updated today","Ready",1,CUI_ROLE_SUCCESS,CUI_ITEM_COMPLETE}};
    const cui_pattern collections[]={CUI_TOOL_CHIPS,CUI_TASK_ROWS,CUI_CHAT,CUI_CONTEXT};
    for(size_t i=0;i<4;++i)cui_pattern_set_items(demo->patterns[collections[i]],items,2);
    const double values[]={12,18,15,24,32};
    const cui_insight_series series[]={{1,"Weekly requests","Requests per day",values,NULL,5},{2,"Response time","Milliseconds",values,NULL,5}};
    cui_insights_set_series(demo->patterns[CUI_INSIGHTS],series,2);
    const cui_tree_item navigation[]={{1,0,"Workspace",1},{2,1,"Design system",1},{3,2,"Tokens",0},{4,2,"Components",0},{5,1,"Release notes",0},{6,1,"Accessibility",0}};
    const char *details[]={"Shared workspace","Shared tokens and components","Colors, typography and spacing","Reusable native controls","Changes ready for review","Keyboard and readable typography"};
    cui_sidebar_set_items(demo->patterns[CUI_SIDEBAR],navigation,details,6);
    cui_set_text(cui_pattern_part(demo->patterns[CUI_CODE_BLOCK], CUI_PART_BODY), "// Native UI through a stable C ABI\ncui_widget *card = cui_pattern_create(root, CUI_APPROVAL, \"Review changes\");\ncui_on_action(card, approved, context);\n");
    cui_set_text(cui_pattern_part(demo->patterns[CUI_THINKING], CUI_PART_BODY), "1. Read the supplied context\n2. Compare available options\n3. Prepare a recommendation\n\nSample trace supplied by the application.");
    cui_set_text(cui_pattern_part(demo->patterns[CUI_SELECTION_ACTIONS], CUI_PART_BODY), "Beautiful native applications should feel familiar, remain readable on a 4K display, and work naturally with the keyboard. Select a phrase and choose an action.");
    unsigned char pixels[64*40*4];
    for (int i = 0; i < 64*40; ++i) { pixels[i*4] = 40; pixels[i*4+1] = (unsigned char)(70+i%64); pixels[i*4+2] = 150; pixels[i*4+3] = 255; }
    cui_image_set_rgba(cui_pattern_part(demo->patterns[CUI_AGENT_SCREEN], CUI_PART_PREVIEW), pixels, 64, 40);
    cui_pattern_set_busy(demo->patterns[CUI_LOADING], 1);
    demo->stream_timer = cui_every(demo->app, 450, stream_sample, demo);
}
static void text_scale_changed(cui_widget *sender, void *data)
{
    gallery_demo *demo = (gallery_demo *)data;
    const double scales[] = {1, 1.25, 1.5, 2}; int selected = cui_get_selected(sender);
    if (selected >= 0 && selected < 4) cui_app_set_text_scale(demo->app, scales[selected]);
}
static void typography_page(gallery_demo *demo, cui_widget *page)
{
    caption(page, "Native text shaping · independent display and text scaling");
    const char *scales[] = {"Text 100%", "Text 125%", "Text 150%", "Text 200%"};
    cui_widget *scale = cui_select(page, scales, 4); demo->text_scale_select = scale; cui_set_selected(scale, 0); cui_on_action(scale, text_scale_changed, demo);
    demo->font_sample = cui_label(page, "Crisp type at every resolution."); cui_set_font(demo->font_sample, NULL, 24, 600);
    cui_widget *body = cui_label(page, "System font · regular 13 pt · live layout measurements"); cui_set_font(body, NULL, 13, 400);
    cui_widget *large = cui_label(page, "Readable. Familiar. Native."); cui_set_font(large, NULL, 32, 700);
    cui_code(page, "UTF-8: Café · München · Ελληνικά · 日本語\nMixed scripts use the platform's installed font fallback.\nNo bitmap font atlas or bundled font files.");
    cui_entry(page, "Try editing with your keyboard or input method…");
    caption(page, "Use your desktop display settings for monitor scale; the selector changes text size only.");
}
int gallery_demo_create(gallery_demo *demo, cui_app *app)
{
    const char *themes[] = {"System appearance", "Light appearance", "Dark appearance"};
    memset(demo, 0, sizeof(*demo)); demo->app = app;
    demo->window = cui_window_create(app, "CUI - Component gallery", 1080, 900);
    if (!demo->window) return 0;
    cui_window_set_scrollable(demo->window, 1);
    cui_widget *root = cui_window_root(demo->window); cui_box_set_padding(root, 28);
    cui_widget *header = cui_box(root, CUI_HORIZONTAL, 24);
    cui_widget *intro = cui_box(header, CUI_VERTICAL, 6); cui_expand(intro, 1);
    caption(intro, "CUI  /  COMPONENT LIBRARY");
    cui_widget *title = cui_label(intro, "Native UI. Beautiful details."); cui_set_role(title, CUI_ROLE_TITLE);
    caption(intro, "Native controls, thoughtful details.");
    cui_widget *theme = cui_select(header, themes, 3); cui_set_selected(theme, 0);
    cui_on_action(theme, theme_changed, demo);
    demo->tabs = cui_tabs(root); cui_expand(demo->tabs, 1);
    controls_page(demo, cui_tab_add(demo->tabs, "Controls"));
    data_page(demo, cui_tab_add(demo->tabs, "Data"));
    patterns_page(demo, cui_tab_add(demo->tabs, "Activity"));
    code_page(demo, cui_tab_add(demo->tabs, "Code"));
    reference_page(demo, cui_tab_add(demo->tabs, "Components"));
    typography_page(demo, cui_tab_add(demo->tabs, "Type"));
    cui_separator(root);
    demo->status = caption(root, "Explore the controls. Every example runs through the public C API.");
    return demo->status && demo->table && demo->slider && demo->send && !cui_app_error(app)[0];
}
