/* Reference-specific compositions of public CUI primitives. */
static void notice(const char *text) {
  if (!toast_layer)
    return;
  cui_set_text(toast_label, text);
  cui_set_visible(toast_layer, 1);
  toast_until = cui_time() + 2.8;
}
static void close_modal(void *data) {
  (void)data;
  modal_kind = 0;
  cui_set_enabled(base_layer, 1);
  cui_set_visible(scrim_layer, 0);
  cui_set_visible(verify_layer, 0);
  cui_set_visible(palette_layer, 0);
  cui_picker_close(palette);
  cui_command_set_enabled(escape_command, 0);
  cui_focus(cui_chat_part(composers[focused], 0));
}
static void scrim_event(cui_widget *w, const cui_canvas_event *e, void *data) {
  (void)w;
  (void)data;
  if (e->kind == CUI_CANVAS_ACTIVATE)
    close_modal(NULL);
}
static void modal_open(int kind) {
  modal_kind = kind;
  cui_set_enabled(base_layer, 0);
  cui_set_visible(scrim_layer, 1);
  cui_set_visible(verify_layer, kind == 1);
  cui_set_visible(palette_layer, kind == 2);
  cui_command_set_enabled(escape_command, 1);
  if (kind == 1)
    cui_focus(verify_accept);
}
static void verify_show(cui_widget *w, void *data) {
  (void)w;
  (void)data;
  if (session_verified)
    notice("This session is verified");
  else
    modal_open(1);
}
static void verify_answer(cui_widget *w, void *data) {
  (void)w;
  int accepted = data != NULL;
  close_modal(NULL);
  if (accepted) {
    session_verified = 1;
    cui_set_text(verify_button, "Session verified");
    style(verify_button, theme.surface, 18, theme.online, 1.5, 8);
  }
  notice(accepted ? "Session verified" : "Verification cancelled");
}
static void show_notice(cui_widget *w, void *data) {
  (void)w;
  notice((const char *)data);
}
static void palette_action(cui_widget *w, void *data) {
  (void)data;
  cui_picker_event event = cui_picker_last_event(w);
  if (event == CUI_PICKER_CANCEL) {
    close_modal(NULL);
    return;
  }
  if (event != CUI_PICKER_SELECT)
    return;
  cui_item_id id = cui_picker_selected(w);
  close_modal(NULL);
  if (id && id <= ROOM_COUNT) {
    sample_rooms[id - 1].room.unread = 0;
    /* Navigation uses the same stable room IDs as the palette. */
    open_room(focused, (unsigned)id - 1);
  } else if (id >= 100 && id <= 103 && workspace) {
    layout_click(NULL, (void *)(size_t)((unsigned)id - 99));
  } else if (id == 104) {
    for (size_t i = 0; i < ROOM_COUNT; ++i)
      sample_rooms[i].room.unread = 0;
    room_list();
    notice("All rooms marked read");
  } else if (id == 105)
    start_call();
  else if (id == 106)
    notice("Create room");
  else if (id == 107)
    notice("Verification request sent to your other sessions");
}
static void open_palette(void *data) {
  (void)data;
  if (appearance != CUI_CHAT_TILES) {
    notice("Search rooms, people and messages");
    return;
  }
  if (modal_kind == 2) {
    close_modal(NULL);
    return;
  }
  if (modal_kind)
    return;
  modal_open(2);
  cui_picker_set_query(palette, "");
  cui_picker_open(palette, NULL);
}
static void end_call(cui_widget *w, void *data) {
  (void)w;
  (void)data;
  call_active = 0;
  cui_set_visible(call_layer, 0);
  notice("Call ended");
}
static void call_toggle(cui_widget *w, void *data) {
  (void)data;
  int checked = cui_get_checked(w);
  cui_widget_style control = {checked ? theme.foreground : theme.soft,
                              checked ? theme.background : theme.foreground,
                              0,
                              22,
                              0,
                              12};
  cui_set_style(w, &control);
}
static void start_call(void) {
  if (appearance != CUI_CHAT_NEBULA) {
    notice("Starting an encrypted call…");
    return;
  }
  cui_set_text(call_title, sample_rooms[pane_room[focused]].room.title);
  call_started = cui_time();
  call_active = 1;
  cui_set_visible(call_layer, 1);
}
static cui_widget *center_label(cui_widget *parent, const char *text,
                                double size, int weight) {
  cui_widget *row = cui_box(parent, CUI_HORIZONTAL, 0);
  cui_widget *before = cui_box(row, CUI_HORIZONTAL, 0);
  cui_expand(before, 1);
  cui_widget *value = label(row, text, size, weight);
  cui_widget *after = cui_box(row, CUI_HORIZONTAL, 0);
  cui_expand(after, 1);
  return value;
}
static void build_overlays(void) {
  /* All transient content shares the same native window and C stack layout. */
  call_layer = cui_stack_layer(layers, CUI_LAYER_BOTTOM_RIGHT, 310, 0, 24);
  cui_widget *card = cui_box(call_layer, CUI_VERTICAL, 10);
  style(card, theme.rail, 16, theme.border, 1, 14);
  cui_widget *heading = cui_box(card, CUI_HORIZONTAL, 8);
  call_title = label(heading, "", 12.5, 650);
  cui_expand(call_title, 1);
  call_clock = label(heading, "00:00", 12, 400);
  cui_widget *grid = cui_grid(card, 2, 8);
  const char *names[] = {"Ana Ribeiro", "Kai Tanaka", "Ben Okafor", "You"};
  for (unsigned i = 0; i < 4; ++i) {
    cui_widget *cell = cui_grid_cell(grid, i / 2, i % 2, 1, 1);
    style(cell, avatar_color(names[i]), 10, 0, 0, 4);
    cui_set_min_size(cell, 120, 102);
    cui_widget *row = cui_box(cell, CUI_HORIZONTAL, 0);
    cui_widget *spacer = cui_box(row, CUI_HORIZONTAL, 0);
    cui_expand(spacer, 1);
    cui_widget *av = chat(row, CUI_CHAT_AVATAR);
    cui_expand(av, 0);
    cui_set_min_size(cui_chat_part(av, 0), 64, 64);
    cui_chat_theme tile_theme = theme;
    tile_theme.surface = avatar_color(names[i]);
    cui_chat_set_theme(av, &tile_theme);
    cui_chat_room person = {.id = i + 1,
                            .title = names[i],
                            .avatar_color = cui_chat_color(.95, 0, 0, .55)};
    cui_chat_set_rooms(av, &person, 1);
    spacer = cui_box(row, CUI_HORIZONTAL, 0);
    cui_expand(spacer, 1);
    cui_widget *name = label(cell, names[i], 11, 650);
    cui_widget_style ink = {
        avatar_color(names[i]), theme.background, 0, 0, 0, 0};
    cui_set_style(name, &ink);
  }
  cui_widget *controls = cui_box(card, CUI_HORIZONTAL, 8);
  cui_widget *mute = cui_toggle(controls, "Mute", 0);
  cui_widget *camera = cui_toggle(controls, "Camera", 0);
  cui_widget *share = cui_toggle(controls, "Share", 0);
  cui_widget *controls_list[] = {mute, camera, share};
  const cui_symbol symbols[] = {CUI_SYMBOL_MUTED, CUI_SYMBOL_VIDEO,
                                CUI_SYMBOL_PANEL};
  for (unsigned i = 0; i < 3; ++i) {
    artwork(controls_list[i], cui_icon_symbol(symbols[i]), 20);
    cui_set_icon_only(controls_list[i], 1);
    call_toggle(controls_list[i], NULL);
    cui_on_action(controls_list[i], call_toggle, NULL);
  }
  cui_widget *leave = button(controls, "Leave", end_call, NULL);
  artwork(leave, cui_icon_symbol(CUI_SYMBOL_PHONE), 20);
  cui_set_icon_only(leave, 1);
  style(leave, theme.danger, 22, 0, 0, 12);
  cui_widget *bottom = cui_box(call_layer, CUI_VERTICAL, 0);
  cui_set_min_size(bottom, 1, 76);
  cui_set_visible(call_layer, 0);
  scrim_layer = cui_stack_layer(layers, CUI_LAYER_FILL, 0, 0, 0);
  style(scrim_layer, cui_chat_color(.25, .04, 250, .35), 0, 0, 0, 0);
  scrim_canvas = cui_canvas(scrim_layer);
  cui_expand(scrim_canvas, 1);
  cui_canvas_on_event(scrim_canvas, scrim_event, NULL);
  cui_set_visible(scrim_layer, 0);
  verify_layer = cui_stack_layer(layers, CUI_LAYER_CENTER, 520, 0, 16);
  style(verify_layer, theme.surface, 24, 0, 0, 28);
  cui_widget *verify_content = cui_box(verify_layer, CUI_VERTICAL, 6);
  label(verify_content, "Compare emoji", 22, 800);
  label(verify_content,
        "Confirm the emoji below appear in the same order on your\nother "
        "device, signed in as @mathias:lumen.chat.",
        14, 400);
  cui_widget *gap = cui_box(verify_layer, CUI_VERTICAL, 0);
  cui_set_min_size(gap, 1, 18);
  cui_widget *emojis = cui_grid(verify_layer, 7, 6);
  const char *glyphs[] = {"🐶", "🔑", "🌵", "🎸", "🚀", "🍄", "⚓"};
  const char *words[] = {"Dog",    "Key",      "Cactus", "Guitar",
                         "Rocket", "Mushroom", "Anchor"};
  for (unsigned i = 0; i < 7; ++i) {
    cui_widget *cell = cui_grid_cell(emojis, 0, i, 1, 1);
    style(cell, theme.soft, 14, 0, 0, 2);
    cui_widget *inside = cui_box(cell, CUI_VERTICAL, 0);
    cui_widget *space = cui_box(inside, CUI_VERTICAL, 0);
    cui_set_min_size(space, 1, 8);
    center_label(inside, glyphs[i], 26, 400);
    center_label(inside, words[i], 10.5, 600);
    space = cui_box(inside, CUI_VERTICAL, 0);
    cui_set_min_size(space, 1, 8);
  }
  gap = cui_box(verify_layer, CUI_VERTICAL, 0);
  cui_set_min_size(gap, 1, 20);
  cui_widget *actions = cui_box(verify_layer, CUI_HORIZONTAL, 8);
  cui_widget *spacer = cui_box(actions, CUI_HORIZONTAL, 0);
  cui_expand(spacer, 1);
  button(actions, "They don’t match", verify_answer, NULL);
  verify_accept = button(actions, "They match", verify_answer, (void *)1);
  cui_widget_style accept_style = {
      theme.foreground, theme.surface, 0, 13, 0, 12};
  cui_set_style(verify_accept, &accept_style);
  cui_accessibility(verify_layer, "Compare emoji",
                    "Session verification demonstration");
  cui_set_visible(verify_layer, 0);
  palette_layer = cui_stack_layer(layers, CUI_LAYER_TOP, 600, 480, 148);
  style(palette_layer, theme.surface, 16, theme.foreground, 1.5, 14);
  palette = cui_picker(palette_layer, CUI_COMMAND_PALETTE,
                       "Search rooms or commands…");
  cui_picker_set_chrome(palette, 0, 0, 0);
  cui_set_role(palette, CUI_ROLE_BODY);
  cui_box_set_padding(palette, 0);
  style(palette, theme.surface, 0, 0, 0, 0);
  cui_expand(palette, 1);
  cui_widget *results = cui_picker_get_part(palette, CUI_PICKER_RESULTS);
  cui_expand(results, 1);
  style(results, theme.surface, 0, 0, 0, 0);
  style(cui_picker_get_part(palette, CUI_PICKER_INPUT), theme.surface, 0,
        theme.border, 1, 12);
  cui_choice choices[ROOM_COUNT + 8];
  for (size_t i = 0; i < ROOM_COUNT; ++i)
    choices[i] =
        (cui_choice){sample_rooms[i].room.id, sample_rooms[i].room.title,
                     sample_rooms[i].topic, "", 0};
  const char *commands[] = {"Layout: single",     "Layout: split",
                            "Layout: main + two", "Layout: quad",
                            "Mark all as read",   "Start call",
                            "Create room",        "Verify this session"};
  for (unsigned i = 0; i < 8; ++i)
    choices[ROOM_COUNT + i] = (cui_choice){100 + i, commands[i], "", "", 0};
  cui_picker_set_items(palette, choices, ROOM_COUNT + 8);
  cui_on_action(palette, palette_action, NULL);
  cui_set_visible(palette_layer, 0);
  toast_layer = cui_stack_layer(layers, CUI_LAYER_BOTTOM, 0, 0, 30);
  toast_label = label(toast_layer, "", 13, 650);
  cui_widget_style toast_style = {
      theme.foreground, theme.surface, 0, 12, 0, 14};
  cui_set_style(toast_layer, &toast_style);
  toast_style.padding = 0;
  cui_set_style(toast_label, &toast_style);
  cui_set_visible(toast_layer, 0);
  cui_command_create(app, "Search", 'K', CUI_MOD_PRIMARY, open_palette, NULL);
  escape_command = cui_command_create(app, "Close dialog", CUI_KEY_ESCAPE, 0,
                                      close_modal, NULL);
  cui_command_set_enabled(escape_command, 0);
}
