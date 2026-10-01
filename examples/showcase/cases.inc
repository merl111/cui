/* showcase: canvas */
static void canvas_action(cui_widget *sender,const cui_canvas_event *event,void *data)
{
    if(event->kind==CUI_CANVAS_ACTIVATE)clicked(sender,data);
}
static void show_canvas(cui_widget *root)
{
    cui_surface *surface=cui_surface_create(520,260,cui_window_scale(window));
    cui_widget *canvas=cui_canvas(root);
    cui_set_min_size(canvas,520,260);
    const cui_draw_command scene[]={
        {.op=CUI_DRAW_GRADIENT,.p={0,0,520,260},.color=0x273957ff,.color2=0x754333ff},
        {.op=CUI_DRAW_ELLIPSE,.p={190,-30,280,270},.color=0xfb7749cc},
        {.op=CUI_DRAW_SHADOW,.p={40,48,440,180,22,16},.color=0x00000088},
        {.op=CUI_DRAW_MATERIAL,.p={40,40,440,180,22,16},.color=0x162234bb},
        {.op=CUI_DRAW_TEXT,.p={64,64,390,25,600},.text="Make something your own.",.color=0xffffffff},
        {.op=CUI_DRAW_LAYER,.p={.8}},
        {.op=CUI_DRAW_RECT,.p={64,128,198,52,14},.color=0x559cffff},
        {.op=CUI_DRAW_TEXT,.p={86,143,162,17,500},.text="Try this custom action",.color=0xffffffff},
        {.op=CUI_DRAW_END_LAYER}
    };
    if(!surface||!canvas||!cui_surface_render(surface,scene,sizeof(scene)/sizeof(scene[0]))){cui_surface_release(surface);return;}
    cui_canvas_set_surface(canvas,surface);
    cui_surface_release(surface); /* canvas owns its own reference */
    const cui_canvas_region region={1,64,128,198,52,"Try custom action",1};
    cui_canvas_set_regions(canvas,&region,1);
    cui_canvas_on_event(canvas,canvas_action,NULL);
}
/* endshowcase */

/* showcase: button */
static void show_button(cui_widget *root)
{
    cui_widget *button = cui_button(root, "Create workspace");
    cui_set_role(button, CUI_ROLE_PRIMARY);
    cui_on_action(button, clicked, NULL);
}
/* endshowcase */

/* showcase: label */
static void show_label(cui_widget *root)
{
    cui_set_role(cui_label(root, "A place for your best work."), CUI_ROLE_TITLE);
    cui_set_role(cui_label(root, "System fonts. Thoughtful defaults."), CUI_ROLE_CAPTION);
    cui_label(root, "Hello, world · Bonjour · 世界");
}
/* endshowcase */

/* showcase: entry */
static void show_entry(cui_widget *root)
{
    cui_widget *field = cui_entry(root, "Studio North");
    cui_set_min_size(field, 0, 40);
}
/* endshowcase */

/* showcase: password */
static void show_password(cui_widget *root)
{
    cui_widget *field = cui_password(root, "not-a-real-password");
    cui_set_min_size(field, 0, 40);
}
/* endshowcase */

/* showcase: search-field */
static void show_search_field(cui_widget *root)
{
    cui_widget *field = cui_search(root, "Search your workspace…");
    cui_set_min_size(field, 0, 40);
}
/* endshowcase */

/* showcase: textarea */
static void show_textarea(cui_widget *root)
{
    cui_widget *field = cui_textarea(root, "Great software starts with a small idea.\nMake a little room for it.");
    cui_set_min_size(field, 0, 150);
}
/* endshowcase */

/* showcase: code */
static void show_code(cui_widget *root)
{
    cui_widget *field = cui_code(root, "cui_widget *button = cui_button(root, \"Hello\");\ncui_set_role(button, CUI_ROLE_PRIMARY);");
    cui_set_min_size(field, 0, 150);
}
/* endshowcase */

/* showcase: checkbox */
static void show_checkbox(cui_widget *root)
{
    cui_widget *control = cui_checkbox(root, "Keep me updated", 1);
    cui_on_action(control, clicked, NULL);
}
/* endshowcase */

/* showcase: toggle */
static void show_toggle(cui_widget *root)
{
    cui_widget *control = cui_toggle(root, "Compact view", 1);
    cui_on_action(control, clicked, NULL);
}
/* endshowcase */

/* showcase: switch */
static void show_switch(cui_widget *root)
{
    cui_widget *control = cui_switch(root, "Enable notifications", 1);
    cui_on_action(control, clicked, NULL);
}
/* endshowcase */

/* showcase: radio */
static void show_radio(cui_widget *root)
{
    cui_widget *group = cui_box(root, CUI_VERTICAL, 10);
    cui_radio(group, "Follow system appearance", 1);
    cui_radio(group, "Always light", 0);
    cui_radio(group, "Always dark", 0);
}
/* endshowcase */

/* showcase: select */
static void show_select(cui_widget *root)
{
    const char *items[] = {"Design", "Engineering", "Research"};
    cui_widget *items_view = cui_select(root, items, 3);
    cui_set_selected(items_view, 1);
}
/* endshowcase */

/* showcase: list */
static void show_list(cui_widget *root)
{
    const char *items[] = {"Design", "Engineering", "Research"};
    cui_widget *items_view = cui_list(root, items, 3);
    cui_set_selected(items_view, 1);
}
/* endshowcase */

/* showcase: table */
static void show_table(cui_widget *root)
{
    const char *headers[] = {"Project", "Stage", "Tasks"};
    const char *cells[] = {"Atlas", "In progress", "12", "Studio", "Ready", "8", "Field notes", "Review", "24"};
    cui_widget *table = cui_table(root, headers, 3);
    cui_table_set_rows(table, cells, 3);
    cui_table_set_multiple(table, 1);
    cui_table_set_editable(table, 0, 1);
    cui_table_select_row(table, 1, 1);
}
/* endshowcase */

/* showcase: slider */
static void show_slider(cui_widget *root)
{
    cui_label(root, "Preview intensity");
    cui_slider(root, 0.64);
}
/* endshowcase */

/* showcase: progress */
static void show_progress(cui_widget *root)
{
    cui_label(root, "Preparing your workspace · 64%");
    cui_progress(root, 0.64);
}
/* endshowcase */

/* showcase: spinner */
static void show_spinner(cui_widget *root)
{
    cui_spinner(root);
    cui_label(root, "Connecting to your workspace…");
}
/* endshowcase */

/* showcase: separator */
static void show_separator(cui_widget *root)
{
    cui_label(root, "Workspace preferences");
    cui_separator(root);
    cui_label(root, "Account preferences");
}
/* endshowcase */

/* showcase: badge */
static void show_badge(cui_widget *root)
{
    cui_badge(root, "Ready to publish", CUI_ROLE_SUCCESS);
    cui_badge(root, "Review requested", CUI_ROLE_WARNING);
    cui_badge(root, "Connection lost", CUI_ROLE_DANGER);
}
/* endshowcase */

/* showcase: chart */
static void show_chart(cui_widget *root)
{
    const double values[] = {12, 18, 15, 27, 24, 37, 42, 38, 52};
    cui_widget *chart = cui_chart(root, values, 9);
    cui_set_min_size(chart, 0, 220);
}
/* endshowcase */

/* showcase: image */
static void show_image(cui_widget *root)
{
    unsigned char pixels[96 * 64 * 4];
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 96; ++x) {
            size_t i = (size_t)(y * 96 + x) * 4;
            pixels[i] = (unsigned char)(38 + x);
            pixels[i + 1] = (unsigned char)(110 + y);
            pixels[i + 2] = 128; pixels[i + 3] = 255;
        }
    }
    cui_widget *image = cui_image(root);
    cui_image_set_rgba(image, pixels, 96, 64);
    cui_set_min_size(image, 0, 220);
}
/* endshowcase */

/* showcase: box */
static void show_box(cui_widget *root)
{
    cui_widget *row = cui_box(root, CUI_HORIZONTAL, 12);
    cui_widget *left = cui_box(row, CUI_VERTICAL, 8);
    cui_widget *right = cui_box(row, CUI_VERTICAL, 8);
    cui_expand(left, 1); cui_expand(right, 1);
    cui_set_role(left, CUI_ROLE_CARD); cui_set_role(right, CUI_ROLE_CARD);
    cui_box_set_padding(left, 20); cui_box_set_padding(right, 20);
    cui_label(left, "Made to fit"); cui_label(right, "Room to grow");
}
/* endshowcase */

/* showcase: stack */
static void show_stack(cui_widget *root)
{
    cui_widget *stack = cui_stack(root);
    cui_set_min_size(stack, 480, 280);
    cui_widget *base = cui_stack_layer(stack, CUI_LAYER_FILL, 0, 0, 0);
    cui_label(base, "Application content remains in the same window.");
    cui_button(base, "Underlying action");
    cui_widget *card = cui_stack_layer(stack, CUI_LAYER_CENTER, 320, 160, 16);
    const cui_widget_style style = {0xf1f5ffff, 0x192536ff, 0xc4cfe0ff, 16, 1, 20};
    cui_set_style(card, &style);
    cui_label(card, "A reusable overlay layer");
    cui_label(card, "Compose ordinary C controls inside it.");
    cui_on_action(cui_button(card, "Continue"), clicked, NULL);
}
/* endshowcase */

/* showcase: grid */
static void show_grid(cui_widget *root)
{
    cui_widget *grid = cui_grid(root, 2, 14);
    cui_entry(cui_grid_cell(grid, 0, 0, 1, 1), "First name");
    cui_entry(cui_grid_cell(grid, 0, 1, 1, 1), "Last name");
    cui_entry(cui_grid_cell(grid, 1, 0, 1, 2), "hello@example.com");
}
/* endshowcase */

/* showcase: wrap */
static void show_wrap(cui_widget *root)
{
    cui_widget *wrap = cui_wrap(root, 10);
    cui_button(wrap, "Product design"); cui_button(wrap, "Engineering");
    cui_button(wrap, "Research"); cui_button(wrap, "Documentation");
}
/* endshowcase */

/* showcase: split */
static void show_split(cui_widget *root)
{
    cui_widget *split = cui_split(root, CUI_HORIZONTAL, 0.35);
    cui_label(cui_split_pane(split, 0), "Project files");
    cui_textarea(cui_split_pane(split, 1), "Your document starts here.");
    cui_set_min_size(split, 0, 180);
}
/* endshowcase */

/* showcase: tabs */
static void show_tabs(cui_widget *root)
{
    cui_widget *tabs = cui_tabs(root);
    cui_label(cui_tab_add(tabs, "Overview"), "A clear view of your workspace.");
    cui_label(cui_tab_add(tabs, "Activity"), "Everything that happened recently.");
    cui_label(cui_tab_add(tabs, "Settings"), "Make yourself at home.");
}
/* endshowcase */

/* showcase: disclosure */
static void show_disclosure(cui_widget *root)
{
    cui_widget *details = cui_disclosure(root, "Advanced preferences", 1);
    cui_checkbox(cui_disclosure_content(details), "Keep a local working copy", 1);
}
/* endshowcase */

/* showcase: tree */
static void show_tree(cui_widget *root)
{
    const cui_tree_item items[] = {{1, 0, "Workspace", 1}, {2, 1, "Design", 1}, {3, 2, "Components", 0}, {4, 1, "Engineering", 0}};
    cui_widget *tree = cui_tree(root, items, 4);
    cui_tree_select(tree, 3);
}
/* endshowcase */

/* showcase: breadcrumbs */
static void show_breadcrumbs(cui_widget *root)
{
    const cui_breadcrumb_item path[] = {{1, "Workspace"}, {2, "Design system"}, {3, "Components"}};
    cui_breadcrumbs(root, path, 3);
}
/* endshowcase */

/* showcase: number */
static void show_number(cui_widget *root)
{
    cui_label(root, "Spacing in logical units");
    cui_number(root, 12.5, 0, 100, 0.5, 1);
}
/* endshowcase */

/* showcase: date */
static void show_date(cui_widget *root)
{
    cui_label(root, "Project start");
    cui_date(root, (cui_date_value){2026, 9, 30});
}
/* endshowcase */

/* showcase: time */
static void show_time(cui_widget *root)
{
    cui_label(root, "Daily review");
    cui_time_input(root, (cui_time_value){9, 30, 0});
}
/* endshowcase */

/* showcase: field */
static void show_field(cui_widget *root)
{
    cui_widget *field = cui_field(root, "Workspace name", "", "Choose a name your team will recognize.");
    cui_field_set_error(field, "Enter a workspace name to continue.");
}
/* endshowcase */

/* showcase: autocomplete */
static void show_autocomplete(cui_widget *root)
{
    const cui_choice choices[] = {{1, "Open workspace", "Return to your recent work", "project", 0}, {2, "Create document", "Start something new", "new", 0}, {3, "Export", "Available after saving", "download", 1}};
    cui_widget *picker = cui_picker(root, CUI_AUTOCOMPLETE, "Find an action…");
    cui_picker_set_items(picker, choices, 3);
    cui_picker_open(picker, NULL);
}
/* endshowcase */

/* showcase: command-palette */
static void show_command_palette(cui_widget *root)
{
    const cui_choice choices[] = {{1, "Open workspace", "Return to your recent work", "project", 0}, {2, "Create document", "Start something new", "new", 0}, {3, "Export", "Available after saving", "download", 1}};
    cui_widget *picker = cui_picker(root, CUI_COMMAND_PALETTE, "Find an action…");
    cui_picker_set_items(picker, choices, 3);
    cui_picker_open(picker, NULL);
}
/* endshowcase */

/* showcase: tokens */
static void show_tokens(cui_widget *root)
{
    const cui_choice choices[] = {{1, "Open workspace", "Return to your recent work", "project", 0}, {2, "Create document", "Start something new", "new", 0}, {3, "Export", "Available after saving", "download", 1}};
    cui_widget *tokens = cui_tokens(root, "Add an action…", 4);
    cui_tokens_set_items(tokens, choices, 3);
    const cui_item_id selected[] = {1, 2};
    cui_tokens_set_selected(tokens, selected, 2);
}
/* endshowcase */

/* showcase: banner */
static void show_banner(cui_widget *root)
{
    cui_widget *feedback = cui_feedback(root, CUI_BANNER);
    cui_feedback_show(feedback, "Your workspace is up to date", "Everything is saved on this device.", CUI_ROLE_SUCCESS, "View activity", 0);
    cui_on_action(feedback, clicked, NULL);
}
/* endshowcase */

/* showcase: toast */
static void show_toast(cui_widget *root)
{
    cui_widget *feedback = cui_feedback(root, CUI_TOAST);
    cui_feedback_show(feedback, "Document archived", "You can bring it back at any time.", CUI_ROLE_SUBTLE, "Undo", 0);
    cui_on_action(feedback, clicked, NULL);
}
/* endshowcase */

/* showcase: empty-state */
static void show_empty_state(cui_widget *root)
{
    cui_widget *feedback = cui_feedback(root, CUI_EMPTY_STATE);
    cui_feedback_show(feedback, "A little room for something new", "Create your first document to get started.", CUI_ROLE_SUBTLE, "Create document", 0);
    cui_on_action(feedback, clicked, NULL);
}
/* endshowcase */

/* showcase: error-state */
static void show_error_state(cui_widget *root)
{
    cui_widget *feedback = cui_feedback(root, CUI_ERROR_STATE);
    cui_feedback_show(feedback, "Couldn’t connect", "Your local work is safe. Try connecting again.", CUI_ROLE_DANGER, "Try again", 0);
    cui_on_action(feedback, clicked, NULL);
}
/* endshowcase */

/* showcase: file-dialog */
static void show_file_dialog(cui_widget *root)
{
    cui_widget *button = cui_button(root, "Open file dialogs");
    cui_on_action(button, open_dialog, (void *)(uintptr_t)CUI_DIALOG_OPEN);
}
/* endshowcase */

/* showcase: alert */
static void show_alert(cui_widget *root)
{
    cui_widget *button = cui_button(root, "Open alert");
    cui_on_action(button, open_dialog, (void *)(uintptr_t)CUI_DIALOG_ALERT);
}
/* endshowcase */

/* showcase: color-dialog */
static void show_color_dialog(cui_widget *root)
{
    cui_widget *button = cui_button(root, "Open color picker");
    cui_on_action(button, open_dialog, (void *)(uintptr_t)CUI_DIALOG_COLOR);
}
/* endshowcase */

/* showcase: font-dialog */
static void show_font_dialog(cui_widget *root)
{
    cui_widget *button = cui_button(root, "Open font picker");
    cui_on_action(button, open_dialog, (void *)(uintptr_t)CUI_DIALOG_FONT);
}
/* endshowcase */

/* showcase: toolbar */
static void show_toolbar(cui_widget *root)
{
    cui_command *commands[] = {cui_command_create(app, "New document", 'N', CUI_MOD_PRIMARY, command_clicked, NULL), cui_command_create(app, "Save", 'S', CUI_MOD_PRIMARY, command_clicked, NULL)};
    cui_toolbar(root, commands, 2);
}
/* endshowcase */

/* showcase: menu */
static void show_menu(cui_widget *root)
{
    cui_command *command = cui_command_create(app, "New document", 'N', CUI_MOD_PRIMARY, command_clicked, NULL);
    cui_menu *menu = cui_menu_create(app);
    cui_menu_add(menu, command);
    cui_menu_add_separator(menu);
    cui_widget *button = cui_button(root, "Workspace actions");
    cui_on_action(button, popup_menu, menu);
}
/* endshowcase */

/* showcase: pattern-loading-state */
static void show_pattern_loading_state(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_LOADING, "Loading State");
    cui_on_action(pattern, pattern_clicked, NULL);
}
/* endshowcase */

/* showcase: pattern-thinking */
static void show_pattern_thinking(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_THINKING, "Thinking");
    cui_on_action(pattern, pattern_clicked, NULL);
    cui_set_text(cui_pattern_part(pattern, CUI_PART_BODY), "1. Read the project notes\n2. Compare the available options\n3. Prepare a recommendation\n\nApplication-supplied status trace.");
}
/* endshowcase */

/* showcase: pattern-streaming-text */
static void show_pattern_streaming_text(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_STREAMING, "Streaming Text");
    cui_on_action(pattern, pattern_clicked, NULL);
    cui_stream_append(pattern, "A native interface can feel considered, clear, and quietly capable.");
}
/* endshowcase */

/* showcase: pattern-approval-card */
static void show_pattern_approval_card(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_APPROVAL, "Approval Card");
    cui_on_action(pattern, pattern_clicked, NULL);
}
/* endshowcase */

/* showcase: pattern-tool-chips */
static void show_pattern_tool_chips(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_TOOL_CHIPS, "Tool Chips");
    cui_on_action(pattern, pattern_clicked, NULL);
    const cui_pattern_item items[] = {
        {1, "Search workspace", "Check labels, contrast and spacing at 150% text scale.", "Workspace · Today", "In progress", .5, CUI_ROLE_SUBTLE, CUI_ITEM_EXPANDED},
        {2, "Release notes", "Changes are ready for review.", "Updated just now", "Ready", 1, CUI_ROLE_SUCCESS, CUI_ITEM_COMPLETE}
    };
    cui_pattern_set_items(pattern, items, 2);
}
/* endshowcase */

/* showcase: pattern-task-rows */
static void show_pattern_task_rows(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_TASK_ROWS, "Task Rows");
    cui_on_action(pattern, pattern_clicked, NULL);
    const cui_pattern_item items[] = {
        {1, "Review typography", "Check labels, contrast and spacing at 150% text scale.", "Workspace · Today", "In progress", .5, CUI_ROLE_SUBTLE, CUI_ITEM_EXPANDED},
        {2, "Release notes", "Changes are ready for review.", "Updated just now", "Ready", 1, CUI_ROLE_SUCCESS, CUI_ITEM_COMPLETE}
    };
    cui_pattern_set_items(pattern, items, 2);
}
/* endshowcase */

/* showcase: chat-rooms */
static void show_chat_rooms(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_ROOMS, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 440, 300);
    cui_on_action(chat_preview, chat_clicked, NULL);
    const cui_chat_room items[] = {
        {.id=1,.group="PINNED",.title="#design-crit",.detail="Weekly critique · Bring WIP, not finals",.trailing="09:34",.avatar_color=0x64d7faff,.unread=4,.flags=CUI_CHAT_SQUARE|CUI_CHAT_HIGHLIGHT},
        {.id=2,.group="ROOMS",.title="#engineering",.detail="Builds and on-call handoffs",.trailing="08:15",.avatar_color=0x57d8faff,.flags=CUI_CHAT_SQUARE},
        {.id=3,.group="PEOPLE",.title="Ana Ribeiro",.detail="Can you look at the call tile?",.trailing="09:40",.avatar_color=0xcdb2ffff,.flags=CUI_CHAT_ONLINE}
    };
    cui_chat_set_rooms(chat_preview,items,sizeof(items)/sizeof(*items));
    cui_chat_select(chat_preview,1);
}
/* endshowcase */

/* showcase: chat-timeline */
static void show_chat_timeline(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_TIMELINE, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 440, 350);
    cui_on_action(chat_preview, chat_clicked, NULL);
    const cui_chat_span spans[]={{"A reusable conversation component.","",CUI_CHAT_BODY}};
    const cui_chat_detail files[]={{.id=8,.text="layout.fig",.detail="4.2 MB"}};
    const cui_chat_detail reactions[]={{.text="🙌",.count=2,.flags=CUI_CHAT_MINE},{.text="💯",.count=3}};
    const cui_chat_detail options[]={{.text="Speaker focus",.count=4},{.text="Grid",.count=6},{.text="Spotlight + filmstrip",.count=2}};
    const cui_chat_message item={.id=42,.author="Ana Ribeiro",.time="09:12",.avatar_color=0xcdb2ffff,
        .spans=spans,.span_count=1,.selected_option=-1,
        .attachments=files,.attachment_count=0,
        .reactions=reactions,.reaction_count=2,
        .poll_question="Default call layout?",.options=options,.option_count=0,
        .reply_id=0,.reply_author="Kai Tanaka",.reply_text="Composer states",
        .thread_count=0,.thread_preview="Ben: I will pick up the call tile"};
    cui_chat_set_messages(chat_preview,&item,1);
}
/* endshowcase */

/* showcase: chat-composer */
static void show_chat_composer(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_COMPOSER, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 440, 160);
    cui_on_action(chat_preview, chat_clicked, NULL);
    cui_set_placeholder(cui_chat_part(chat_preview,0),"Message your team…");
    cui_chat_compose_context(chat_preview,42,"Ana Ribeiro","Original message",0);
}
/* endshowcase */

/* showcase: chat-workspace */
static void show_chat_workspace(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_WORKSPACE, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 440, 350);
    cui_on_action(chat_preview, chat_clicked, NULL);
    cui_chat_workspace_layout(chat_preview, 3);
    for (unsigned pane=0; pane<3; ++pane) {
        cui_widget *content=cui_chat_part(chat_preview,pane);
        cui_box_set_padding(content,16);
        cui_label(content,pane==0?"Main conversation":pane==1?"Direct message":"Engineering");
        cui_entry(content,"Each pane retains its own draft");
    }
}
/* endshowcase */

/* showcase: chat-header */
static void show_chat_header(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_HEADER, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 440, 72);
    cui_on_action(chat_preview, chat_clicked, NULL);
    const cui_chat_room items[] = {
        {.id=1,.group="PINNED",.title="#design-crit",.detail="Weekly critique · Bring WIP, not finals",.avatar_color=0x64d7faff,.unread=4,.flags=CUI_CHAT_SQUARE|CUI_CHAT_HIGHLIGHT}
    };
    cui_chat_set_rooms(chat_preview,items,sizeof(items)/sizeof(*items));
    const cui_chat_command commands[] = {
        {.id=401,.label="Search conversation",.symbol=CUI_SYMBOL_SEARCH,.action=CUI_CHAT_MORE},
        {.id=402,.label="Show details",.symbol=CUI_SYMBOL_PANEL,.action=CUI_CHAT_MORE,.flags=CUI_CHAT_MINE}
    };
    cui_chat_set_commands(chat_preview,commands,2);
    cui_chat_select(chat_preview,1);
}
/* endshowcase */

/* showcase: chat-spaces */
static void show_chat_spaces(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_SPACES, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 440, 160);
    cui_on_action(chat_preview, chat_clicked, NULL);
    const cui_chat_room items[] = {
        {.id=1,.title="Home",.symbol=CUI_SYMBOL_HOME},
        {.id=2,.title="Design team",.avatar_color=0xffac8aff,.unread=12},
        {.id=3,.title="Community",.avatar_color=0x72dfb7ff,.unread=3}
    };
    cui_chat_set_rooms(chat_preview,items,sizeof(items)/sizeof(*items));
    cui_chat_select(chat_preview,1);
}
/* endshowcase */

/* showcase: chat-inspector */
static void show_chat_inspector(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_INSPECTOR, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 440, 350);
    cui_on_action(chat_preview, chat_clicked, NULL);
    const cui_chat_room items[] = {
        {.id=1,.group="PINNED",.title="#design-crit",.detail="Weekly critique · Bring WIP, not finals",.trailing="09:34",.avatar_color=0x64d7faff,.unread=4,.flags=CUI_CHAT_SQUARE|CUI_CHAT_HIGHLIGHT},
        {.id=20,.title="Members",.detail="14"},
        {.id=21,.title="Encryption",.detail="E2EE"},
        {.id=22,.title="Notifications",.detail="Mentions & keywords"}
    };
    cui_chat_set_rooms(chat_preview,items,sizeof(items)/sizeof(*items));
    cui_chat_select(chat_preview,1);
}
/* endshowcase */

/* showcase: chat-message */
static void show_chat_message(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_MESSAGE, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 440, 200);
    cui_on_action(chat_preview, chat_clicked, NULL);
    const cui_chat_span body[] = {{"A reusable conversation component.", "", CUI_CHAT_BODY}};
    const cui_chat_message item = {.id=42, .author="Ana Ribeiro", .time="09:12",
        .avatar_color=0xcdb2ffff, .author_color=0x714595ff,
        .spans=body, .span_count=1, .selected_option=-1};
    cui_chat_set_messages(chat_preview, &item, 1);
}
/* endshowcase */

/* showcase: attachment-card */
static void show_attachment_card(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_ATTACHMENT_CARD, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 440, 160);
    cui_on_action(chat_preview, chat_clicked, NULL);
    const cui_chat_detail files[] = {{.id=8, .text="layout.fig", .detail="4.2 MB"}};
    const cui_chat_message item = {.id=42, .attachments=files, .attachment_count=1,
        .selected_option=-1};
    cui_chat_set_messages(chat_preview, &item, 1);
}
/* endshowcase */

/* showcase: reaction-strip */
static void show_reaction_strip(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_REACTION_STRIP, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 440, 160);
    cui_on_action(chat_preview, chat_clicked, NULL);
    const cui_chat_detail reactions[] = {{.text="🙌", .count=2, .flags=CUI_CHAT_MINE},
        {.text="💯", .count=3}};
    const cui_chat_message item = {.id=42, .reactions=reactions, .reaction_count=2,
        .selected_option=-1};
    cui_chat_set_messages(chat_preview, &item, 1);
}
/* endshowcase */

/* showcase: poll-card */
static void show_poll_card(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_POLL_CARD, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 440, 260);
    cui_on_action(chat_preview, chat_clicked, NULL);
    const cui_chat_detail options[] = {{.text="Speaker focus", .count=4},
        {.text="Grid", .count=6}, {.text="Spotlight + filmstrip", .count=2}};
    const cui_chat_message item = {.id=42, .poll_question="Default call layout?",
        .options=options, .option_count=3, .selected_option=-1};
    cui_chat_set_messages(chat_preview, &item, 1);
}
/* endshowcase */

/* showcase: reply-preview */
static void show_reply_preview(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_REPLY_PREVIEW, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 440, 160);
    cui_on_action(chat_preview, chat_clicked, NULL);
    const cui_chat_message item = {.id=42, .reply_id=7, .reply_author="Kai Tanaka",
        .reply_text="Composer states", .selected_option=-1};
    cui_chat_set_messages(chat_preview, &item, 1);
}
/* endshowcase */

/* showcase: thread-summary */
static void show_thread_summary(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_THREAD_SUMMARY, CUI_CHAT_NEBULA);
    cui_set_min_size(chat_preview, 440, 160);
    cui_on_action(chat_preview, chat_clicked, NULL);
    const cui_chat_room participants[] = {
        {.id=1, .title="Kai Tanaka", .avatar_color=0x00c9bfff},
        {.id=2, .title="Noor Haddad", .avatar_color=0x64c9faff},
        {.id=3, .title="Ben Okafor", .avatar_color=0xdeb0ffff}};
    const cui_chat_message item = {.id=42, .thread_count=3,
        .thread_preview="Ben: I will pick up the call tile",
        .thread_participants=participants, .thread_participant_count=3,
        .selected_option=-1};
    cui_chat_set_messages(chat_preview, &item, 1);
}
/* endshowcase */

/* showcase: avatar */
static void show_avatar(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_AVATAR, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 440, 160);
    cui_on_action(chat_preview, chat_clicked, NULL);
    const cui_chat_room items[] = {
        {.id=1,.title="Ana Ribeiro",.avatar_color=0xcdb2ffff,.flags=CUI_CHAT_ONLINE}
    };
    cui_chat_set_rooms(chat_preview,items,sizeof(items)/sizeof(*items));
    cui_chat_select(chat_preview,1);
}
/* endshowcase */

/* showcase: native-chat */
static void show_native_chat(cui_widget *root)
{
    chat_preview = cui_chat_create(root, CUI_CHAT_TIMELINE, CUI_CHAT_DAYLIGHT);
    cui_set_min_size(chat_preview, 400, 230);
    const cui_chat_span spans[] = {{"Shared C components, available in every binding.", "", CUI_CHAT_BODY}};
    const cui_chat_message messages[] = {{.id=1, .author="Ana Ribeiro", .time="09:12", .avatar_color=0xcbb5ffff, .spans=spans, .span_count=1, .selected_option=-1, .thread_count=3}};
    cui_chat_set_messages(chat_preview, messages, 1);
    cui_widget *composer = cui_chat_create(root, CUI_CHAT_COMPOSER, CUI_CHAT_DAYLIGHT);
    cui_set_placeholder(cui_chat_part(composer, 0), "Message #design-crit");
}
/* endshowcase */

/* showcase: pattern-chat */
static void show_pattern_chat(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_CHAT, "Chat");
    cui_on_action(pattern, pattern_clicked, (void *)(uintptr_t)CUI_CHAT);
    const cui_pattern_item items[] = {
        {1, "Assistant", "Start with a clear structure and a few useful controls.", "Workspace · Today", "In progress", .5, CUI_ROLE_SUBTLE, CUI_ITEM_EXPANDED},
        {2, "Release notes", "Changes are ready for review.", "Updated just now", "Ready", 1, CUI_ROLE_SUCCESS, CUI_ITEM_COMPLETE}
    };
    cui_pattern_set_items(pattern, items, 1);
    cui_set_placeholder(cui_pattern_part(pattern, CUI_PART_INPUT), "Write a message…");
}
/* endshowcase */

/* showcase: pattern-prompt-bar */
static void show_pattern_prompt_bar(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_PROMPT_BAR, "Prompt Bar");
    cui_on_action(pattern, pattern_clicked, NULL);
}
/* endshowcase */

/* showcase: pattern-recommendation-card */
static void show_pattern_recommendation_card(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_RECOMMENDATION, "Recommendation Card");
    cui_on_action(pattern, pattern_clicked, NULL);
}
/* endshowcase */

/* showcase: pattern-context-cards */
static void show_pattern_context_cards(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_CONTEXT, "Context Cards");
    cui_on_action(pattern, pattern_clicked, NULL);
    const cui_pattern_item items[] = {
        {1, "Design system", "Check labels, contrast and spacing at 150% text scale.", "Workspace · Today", "In progress", .5, CUI_ROLE_SUBTLE, CUI_ITEM_EXPANDED},
        {2, "Release notes", "Changes are ready for review.", "Updated just now", "Ready", 1, CUI_ROLE_SUCCESS, CUI_ITEM_COMPLETE}
    };
    cui_pattern_set_items(pattern, items, 2);
}
/* endshowcase */

/* showcase: pattern-diff-table */
static void show_pattern_diff_table(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_DIFF_TABLE, "Diff Table");
    cui_on_action(pattern, pattern_clicked, NULL);
    const char *cells[] = {"Title", "Untitled", "Workspace", "Spacing", "8", "12", "Theme", "Light", "System"};
    cui_pattern_set_records(pattern, cells, 3);
}
/* endshowcase */

/* showcase: pattern-records-table */
static void show_pattern_records_table(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_RECORDS_TABLE, "Records Table");
    cui_on_action(pattern, pattern_clicked, NULL);
    const char *cells[] = {"Design system", "Active", "Shared components", "Release notes", "Draft", "Ready for review", "Accessibility", "Active", "Keyboard and typography"};
    cui_pattern_set_records(pattern, cells, 3);
}
/* endshowcase */

/* showcase: pattern-filter-table */
static void show_pattern_filter_table(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_FILTER_TABLE, "Filter Table");
    cui_on_action(pattern, pattern_clicked, NULL);
    const char *cells[] = {"Design system", "Active", "Shared components", "Release notes", "Draft", "Ready for review", "Accessibility", "Active", "Keyboard and typography"};
    cui_pattern_set_records(pattern, cells, 3);
    const cui_record_filter filters[]={{1,CUI_FILTER_EQUALS,"Active"},{0,CUI_FILTER_NOT_EQUALS,"Archive"}};
    cui_pattern_set_filters(pattern, filters, 2, 1);
}
/* endshowcase */

/* showcase: pattern-sidebar-nav */
static void show_pattern_sidebar_nav(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_SIDEBAR, "Sidebar Nav");
    cui_on_action(pattern, pattern_clicked, NULL);
    const cui_tree_item items[] = {{1, 0, "Workspace", 1}, {2, 1, "Design system", 1}, {3, 2, "Components", 0}, {4, 1, "Release notes", 0}};
    cui_sidebar_set_items(pattern, items, NULL, 4);
}
/* endshowcase */

/* showcase: pattern-search */
static void show_pattern_search(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_SEARCH_PANEL, "Search");
    cui_on_action(pattern, pattern_clicked, NULL);
    const char *cells[] = {"Design system", "Active", "Shared components", "Release notes", "Draft", "Ready for review", "Accessibility", "Active", "Keyboard and typography"};
    cui_pattern_set_records(pattern, cells, 3);
}
/* endshowcase */

/* showcase: pattern-flowchart */
static void show_pattern_flowchart(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_FLOWCHART, "Flowchart");
    cui_on_action(pattern, pattern_clicked, NULL);
    const char *triggers[] = {"When a document is created", "When a review is completed"};
    cui_set_items(cui_pattern_part(pattern, CUI_PART_CHOICE), triggers, 2);
    cui_set_selected(cui_pattern_part(pattern, CUI_PART_CHOICE), 0);
}
/* endshowcase */

/* showcase: pattern-insight-cards */
static void show_pattern_insight_cards(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_INSIGHTS, "Insight Cards");
    cui_on_action(pattern, pattern_clicked, NULL);
    const double requests[]={12,18,15,24,32}, latency[]={8,6,7,4,3};
    const char *labels[]={"Mon","Tue","Wed","Thu","Fri"};
    const cui_insight_series series[]={
        {1,"Weekly requests","Requests per day",requests,labels,5},
        {2,"Response time","Milliseconds per request",latency,labels,5}
    };
    cui_insights_set_series(pattern, series, 2);
    cui_insights_select(pattern, 1, 4);
}
/* endshowcase */

/* showcase: pattern-code-block */
static void show_pattern_code_block(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_CODE_BLOCK, "Code Block");
    cui_on_action(pattern, pattern_clicked, NULL);
    cui_set_text(cui_pattern_part(pattern, CUI_PART_BODY), "// Small API. Native controls.\ncui_widget *button = cui_button(root, \"Create workspace\");\ncui_set_role(button, CUI_ROLE_PRIMARY);");
}
/* endshowcase */

/* showcase: pattern-fine-tune-card */
static void show_pattern_fine_tune_card(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_FINE_TUNE, "Fine-tune Card");
    cui_on_action(pattern, pattern_clicked, NULL);
}
/* endshowcase */

/* showcase: pattern-selection-actions */
static void show_pattern_selection_actions(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_SELECTION_ACTIONS, "Selection Actions");
    cui_on_action(pattern, pattern_clicked, NULL);
    cui_set_text(cui_pattern_part(pattern, CUI_PART_BODY), "Good interfaces make room for people. Select a phrase to request a transformation from your application.");
}
/* endshowcase */

/* showcase: pattern-agent-screen */
static void show_pattern_agent_screen(cui_widget *root)
{
    cui_widget *pattern = cui_pattern_create(root, CUI_AGENT_SCREEN, "Agent Screen");
    cui_on_action(pattern, pattern_clicked, NULL);
    unsigned char pixels[64 * 40 * 4];
    for (size_t i = 0; i < 64 * 40; ++i) {
        pixels[i*4] = 35; pixels[i*4+1] = (unsigned char)(100+i%64);
        pixels[i*4+2] = 135; pixels[i*4+3] = 255;
    }
    cui_image_set_rgba(cui_pattern_part(pattern, CUI_PART_PREVIEW), pixels, 64, 40);
}
/* endshowcase */

/* showcase: icon */
static void show_icon(cui_widget *root)
{
    cui_label(root,"One asset. Multiple sizes. Native vector rendering.");
    cui_icon_command commands[]={
        {CUI_ICON_MOVE,{12,22},0,0},
        {CUI_ICON_CUBIC,{0,14,0,3,7,3},0,0},
        {CUI_ICON_CUBIC,{10,3,12,6,12,6},0,0},
        {CUI_ICON_CUBIC,{12,6,14,3,17,3},0,0},
        {CUI_ICON_CUBIC,{24,3,24,14,12,22},0,0},
        {CUI_ICON_CLOSE,{0},0,0},{CUI_ICON_FILL,{0},0xdd526eff,0}
    };
    cui_icon_asset *asset=cui_icon_vector(24,24,commands,sizeof(commands)/sizeof(*commands));
    cui_widget *row=cui_box(root,CUI_HORIZONTAL,28);
    cui_set_icon_size(cui_icon(row,asset),24);
    cui_set_icon_size(cui_icon(row,asset),48);
    cui_set_icon_size(cui_icon(row,asset),96);
    cui_icon_release(asset);
}
/* endshowcase */

/* showcase: icon-button */
static void show_icon_button(cui_widget *root)
{
    cui_widget *row=cui_box(root,CUI_HORIZONTAL,16);
    cui_icon_asset *previous=cui_icon_symbol(CUI_SYMBOL_PREVIOUS);
    cui_icon_asset *play=cui_icon_symbol(CUI_SYMBOL_PLAY);
    cui_icon_asset *next=cui_icon_symbol(CUI_SYMBOL_NEXT);
    cui_on_action(cui_icon_button(row,previous,"Previous track"),clicked,NULL);
    cui_widget *button=cui_icon_button(row,play,"Play track");
    cui_set_icon_size(button,28);cui_set_role(button,CUI_ROLE_PRIMARY);cui_on_action(button,clicked,NULL);
    cui_on_action(cui_icon_button(row,next,"Next track"),clicked,NULL);
    cui_widget *labeled=cui_button(root,"Start listening");cui_set_icon(labeled,play);cui_on_action(labeled,clicked,NULL);
    cui_icon_release(previous);cui_icon_release(play);cui_icon_release(next);
}
/* endshowcase */

static const showcase_case cases[] = {
    {"canvas", "Canvas & compositing", show_canvas},
    {"icon","Icon assets",show_icon},
    {"icon-button","Icon button",show_icon_button},
    {"button", "Button", show_button},
    {"label", "Label & typography", show_label},
    {"entry", "Text entry", show_entry},
    {"password", "Password", show_password},
    {"search-field", "Search field", show_search_field},
    {"textarea", "Text area", show_textarea},
    {"code", "Code surface", show_code},
    {"checkbox", "Checkbox", show_checkbox},
    {"toggle", "Toggle button", show_toggle},
    {"switch", "Switch", show_switch},
    {"radio", "Radio group", show_radio},
    {"select", "Dropdown", show_select},
    {"list", "List", show_list},
    {"table", "Data table", show_table},
    {"slider", "Slider", show_slider},
    {"progress", "Progress", show_progress},
    {"spinner", "Spinner", show_spinner},
    {"separator", "Separator", show_separator},
    {"badge", "Badge", show_badge},
    {"chart", "Line chart", show_chart},
    {"image", "RGBA image", show_image},
    {"box", "Rows & columns", show_box},
    {"stack", "Layered stack", show_stack},
    {"grid", "Grid", show_grid},
    {"wrap", "Wrapping row", show_wrap},
    {"split", "Split panes", show_split},
    {"tabs", "Tabs", show_tabs},
    {"disclosure", "Disclosure", show_disclosure},
    {"tree", "Tree", show_tree},
    {"breadcrumbs", "Breadcrumbs", show_breadcrumbs},
    {"number", "Numeric input", show_number},
    {"date", "Date input", show_date},
    {"time", "Time input", show_time},
    {"field", "Validation field", show_field},
    {"autocomplete", "Autocomplete", show_autocomplete},
    {"command-palette", "Command palette", show_command_palette},
    {"tokens", "Token field", show_tokens},
    {"banner", "Banner", show_banner},
    {"toast", "Toast region", show_toast},
    {"empty-state", "Empty state", show_empty_state},
    {"error-state", "Error state", show_error_state},
    {"file-dialog", "File dialogs", show_file_dialog},
    {"alert", "Alert", show_alert},
    {"color-dialog", "Color picker", show_color_dialog},
    {"font-dialog", "Font picker", show_font_dialog},
    {"toolbar", "Toolbar", show_toolbar},
    {"menu", "Menus & context menus", show_menu},
    {"pattern-loading-state", "Loading State", show_pattern_loading_state},
    {"pattern-thinking", "Thinking", show_pattern_thinking},
    {"pattern-streaming-text", "Streaming Text", show_pattern_streaming_text},
    {"pattern-approval-card", "Approval Card", show_pattern_approval_card},
    {"pattern-tool-chips", "Tool Chips", show_pattern_tool_chips},
    {"pattern-task-rows", "Task Rows", show_pattern_task_rows},
    {"chat-rooms", "Room navigation", show_chat_rooms},
    {"chat-timeline", "Message timeline", show_chat_timeline},
    {"chat-composer", "Message composer", show_chat_composer},
    {"chat-workspace", "Conversation workspace", show_chat_workspace},
    {"chat-header", "Conversation header", show_chat_header},
    {"chat-spaces", "Space navigation", show_chat_spaces},
    {"chat-inspector", "Context inspector", show_chat_inspector},
    {"chat-message", "Message bubble", show_chat_message},
    {"attachment-card", "Attachment card", show_attachment_card},
    {"reaction-strip", "Reaction strip", show_reaction_strip},
    {"poll-card", "Poll card", show_poll_card},
    {"reply-preview", "Reply preview", show_reply_preview},
    {"thread-summary", "Thread summary", show_thread_summary},
    {"avatar", "Avatar and presence", show_avatar},
    {"native-chat", "Native Chat", show_native_chat},
    {"pattern-chat", "Chat", show_pattern_chat},
    {"pattern-prompt-bar", "Prompt Bar", show_pattern_prompt_bar},
    {"pattern-recommendation-card", "Recommendation Card", show_pattern_recommendation_card},
    {"pattern-context-cards", "Context Cards", show_pattern_context_cards},
    {"pattern-diff-table", "Diff Table", show_pattern_diff_table},
    {"pattern-records-table", "Records Table", show_pattern_records_table},
    {"pattern-filter-table", "Filter Table", show_pattern_filter_table},
    {"pattern-sidebar-nav", "Sidebar Nav", show_pattern_sidebar_nav},
    {"pattern-search", "Search", show_pattern_search},
    {"pattern-flowchart", "Flowchart", show_pattern_flowchart},
    {"pattern-insight-cards", "Insight Cards", show_pattern_insight_cards},
    {"pattern-code-block", "Code Block", show_pattern_code_block},
    {"pattern-fine-tune-card", "Fine-tune Card", show_pattern_fine_tune_card},
    {"pattern-selection-actions", "Selection Actions", show_pattern_selection_actions},
    {"pattern-agent-screen", "Agent Screen", show_pattern_agent_screen},
};
