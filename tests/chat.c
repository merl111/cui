#include "cui_chat.h"
#include "cui_desktop.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static cui_app *app;
static cui_widget *timeline, *composer, *workspace, *rooms;
static cui_widget *elements[6], *custom_header;
static const cui_chat_action element_actions[] = {
    CUI_CHAT_NONE, CUI_CHAT_ATTACHMENT, CUI_CHAT_REACT,
    CUI_CHAT_VOTE, CUI_CHAT_NONE,       CUI_CHAT_THREAD};
static cui_chat_event last;
static char event_text[128];
static void action(cui_widget *w, void *data) {
  (void)data;
  assert(cui_chat_event_get(w, &last));
  snprintf(event_text, sizeof(event_text), "%s", last.text);
}
static void verify(void *data) {
  (void)data;
  assert(cui_chat_refresh(custom_header, 1));
  cui_chat_event custom = {.action = CUI_CHAT_MORE, .id = 88, .detail_id = 901};
  unsigned custom_hit = cui_chat_action_region(custom_header, &custom);
  assert(custom_hit);
  assert(
      cui_canvas_activate_region(cui_chat_part(custom_header, 0), custom_hit));
  assert(last.detail_id == 901 && !strcmp(event_text, "Localized action"));
  const cui_chat_command disabled = {
      901,           "Disabled action", "", CUI_SYMBOL_SEARCH,
      CUI_CHAT_MORE, CUI_CHAT_DISABLED};
  assert(cui_chat_set_commands(custom_header, &disabled, 1));
  assert(cui_chat_refresh(custom_header, 1));
  custom_hit = cui_chat_action_region(custom_header, &custom);
  assert(custom_hit);
  assert(
      !cui_canvas_activate_region(cui_chat_part(custom_header, 0), custom_hit));
  assert(cui_chat_set_commands(custom_header, NULL, 0));
  assert(cui_chat_refresh(custom_header, 1));
  assert(!cui_chat_action_region(custom_header, &custom));
  for (unsigned i = 0; i < 6; ++i) {
    assert(cui_chat_refresh(elements[i], 1));
    if (element_actions[i] == CUI_CHAT_NONE)
      continue;
    cui_chat_event expected = {.action = element_actions[i], .id = 7};
    if (i == 1)
      expected.detail_id = 4;
    unsigned hit = cui_chat_action_region(elements[i], &expected);
    assert(hit);
    assert(cui_canvas_activate_region(cui_chat_part(elements[i], 0), hit));
    assert(last.action == expected.action && last.id == expected.id);
  }
  assert(cui_chat_refresh(timeline, 1));
  cui_chat_event wanted = {.action = CUI_CHAT_THREAD, .id = 7};
  unsigned region = cui_chat_action_region(timeline, &wanted);
  assert(region);
  assert(cui_canvas_activate_region(cui_chat_part(timeline, 0), region));
  assert(last.action == CUI_CHAT_THREAD && last.id == 7);
  wanted = (cui_chat_event){.action = CUI_CHAT_VOTE, .id = 7, .index = 1};
  region = cui_chat_action_region(timeline, &wanted);
  assert(region);
  assert(cui_canvas_activate_region(cui_chat_part(timeline, 0), region));
  assert(last.action == CUI_CHAT_VOTE && last.index == 1);
  cui_chat_message duplicate[2] = {{.id = 2, .selected_option = -1},
                                   {.id = 2, .selected_option = -1}};
  assert(!cui_chat_set_messages(timeline, duplicate, 2));
  assert(cui_chat_action_region(timeline, &wanted) == region);
  assert(!cui_chat_refresh(timeline, NAN));
  assert(!cui_chat_scroll(timeline, NAN));
  assert(cui_chat_scroll_to(timeline, 7));
  assert(!cui_chat_scroll_to(timeline, 999));
  cui_widget *input = cui_chat_part(composer, 0);
  cui_set_text(input, "  hello 日本語\nworld  ");
  assert(cui_chat_compose_context(composer, 7, "Ana", "Original message", 0));
  assert(cui_chat_compose_busy(composer, 1));
  assert(!cui_chat_compose_submit(composer));
  assert(cui_chat_compose_busy(composer, 0));
  assert(cui_chat_compose_submit(composer));
  assert(last.action == CUI_CHAT_SEND && last.id == 7 &&
         !strcmp(event_text, "  hello 日本語\nworld  "));
  char draft[128];
  cui_get_text(input, draft, sizeof(draft));
  assert(!strcmp(draft, event_text));
  assert(cui_chat_compose_cancel(composer));
  assert(last.action == CUI_CHAT_CANCEL);
  cui_set_text(input, " \n\t");
  assert(!cui_chat_compose_submit(composer));
  cui_chat_detail file = {.id = 1, .text = "design.fig"};
  assert(cui_chat_compose_files(composer, &file, 1));
  assert(cui_chat_compose_submit(composer));
  cui_widget *pane = cui_chat_part(workspace, 2);
  assert(cui_chat_workspace_layout(workspace, 4));
  assert(cui_chat_workspace_focus(workspace, 2));
  assert(cui_chat_workspace_close(workspace, 1));
  assert(cui_chat_workspace_mask(workspace) == 13);
  assert(cui_chat_workspace_maximize(workspace, 2));
  assert(cui_chat_workspace_mask(workspace) == 4);
  assert(!cui_chat_workspace_close(workspace, 2));
  assert(cui_chat_workspace_restore(workspace));
  assert(cui_chat_workspace_mask(workspace) == 13 &&
         cui_chat_part(workspace, 2) == pane);
  assert(!cui_chat_workspace_layout(workspace, 0));
  assert(!cui_chat_workspace_focus(workspace, 8));
  assert(!cui_chat_select(rooms, 99));
  cui_widget_style invalid = {.radius = NAN};
  assert(!cui_set_style(input, &invalid));
  assert(cui_set_style(input, NULL));
  /* Activation metadata must outlive replacement of the copied message model.
   */
  const cui_chat_span link_span = {"Open several linked words",
                                   "https://example.test/message",
                                   CUI_CHAT_BODY};
  const cui_chat_message linked = {.id = 7,
                                   .author = "Ana",
                                   .spans = &link_span,
                                   .span_count = 1,
                                   .selected_option = -1};
  assert(cui_chat_set_messages(timeline, &linked, 1));
  assert(cui_chat_refresh(timeline, 1));
  wanted = (cui_chat_event){.action = CUI_CHAT_LINK, .id = 7};
  unsigned link = cui_chat_action_region(timeline, &wanted);
  assert(link);
  cui_chat_message replacement = {.id = 99, .selected_option = -1};
  assert(cui_chat_set_messages(timeline, &replacement, 1));
  assert(cui_canvas_activate_region(cui_chat_part(timeline, 0), link));
  assert(last.action == CUI_CHAT_LINK &&
         !strcmp(event_text, "https://example.test/message"));
  cui_app_quit(app);
}
int main(int argc, char **argv) {
  (void)argv;
  assert(cui_chat_color(0, 0, 0, 1) == 0xff);
  assert(cui_chat_color(1, 0, 0, 1) == 0xffffffffu);
  assert(cui_chat_color(NAN, 0, 0, 1) == 0);
  cui_chat_theme t;
  assert(cui_chat_theme_get(CUI_CHAT_DAYLIGHT, &t));
  assert(t.accent == cui_chat_color(.56, .21, 22, 1));
  assert(!cui_chat_theme_get((cui_chat_appearance)9, &t));
  app = cui_app_create();
  assert(app);
  cui_window *w = cui_window_create(app, "Shared C chat contracts", 800, 1000);
  cui_widget *root = cui_window_root(w);
  timeline = cui_chat_create(root, CUI_CHAT_TIMELINE,
                             argc > 1 ? CUI_CHAT_DAYLIGHT : CUI_CHAT_NEBULA);
  composer = cui_chat_create(root, CUI_CHAT_COMPOSER, CUI_CHAT_NEBULA);
  rooms = cui_chat_create(root, CUI_CHAT_ROOMS, CUI_CHAT_NEBULA);
  workspace = cui_chat_create(root, CUI_CHAT_WORKSPACE, CUI_CHAT_TILES);
  assert(timeline && composer && rooms && workspace);
  cui_window *custom_window =
      cui_window_create(app, "Mixed presentation", 600, 100);
  custom_header = cui_chat_create(cui_window_root(custom_window),
                                  CUI_CHAT_HEADER, CUI_CHAT_NEBULA);
  cui_chat_room custom_room = {.id = 88, .title = "Custom"};
  assert(cui_chat_set_rooms(custom_header, &custom_room, 1));
  char custom_label[] = "Localized action";
  cui_chat_command commands[] = {
      {901, custom_label, "", CUI_SYMBOL_SEARCH, CUI_CHAT_MORE, CUI_CHAT_MINE}};
  assert(cui_chat_set_commands(custom_header, commands, 1));
  custom_label[0] = 'X';
  cui_chat_command duplicate_commands[] = {commands[0], commands[0]};
  assert(!cui_chat_set_commands(custom_header, duplicate_commands, 2));
  commands[0].id = 0;
  assert(!cui_chat_set_commands(custom_header, commands, 1));
  cui_chat_presentation presentation, copied;
  assert(cui_chat_presentation_preset(CUI_CHAT_DAYLIGHT, &presentation));
  presentation.rooms = CUI_CHAT_COMPACT;
  presentation.room_height = 36;
  presentation.spaces = CUI_VERTICAL;
  assert(cui_chat_set_presentation(custom_header, &presentation));
  assert(cui_chat_theme_get(CUI_CHAT_NEBULA, &t));
  assert(cui_chat_set_theme(custom_header, &t));
  assert(cui_chat_presentation_get(custom_header, &copied));
  assert(copied.messages == CUI_CHAT_SOFT && copied.rooms == CUI_CHAT_COMPACT &&
         copied.room_height == 36);
  presentation.bubble_radius = NAN;
  assert(!cui_chat_set_presentation(custom_header, &presentation));
  assert(cui_chat_presentation_get(custom_header, &presentation));
  assert(presentation.bubble_radius == 18);
  cui_on_action(custom_header, action, NULL);
  cui_window_show(custom_window);
  cui_set_visible(rooms, 0);
  cui_set_visible(workspace, 0);
  cui_on_action(timeline, action, NULL);
  cui_on_action(composer, action, NULL);
  char body[] = "Original copied body";
  cui_chat_span spans[] = {
      {body, "https://example.test/message", CUI_CHAT_BODY}};
  cui_chat_detail options[] = {{.text = "One", .count = 2},
                               {.text = "Two", .count = 3}};
  cui_chat_message message = {.id = 7,
                              .author = "Ana",
                              .spans = spans,
                              .span_count = 1,
                              .poll_question = "Choose",
                              .options = options,
                              .option_count = 2,
                              .selected_option = -1,
                              .thread_count = 3,
                              .thread_preview = "Ben: call tile"};
  assert(cui_chat_set_messages(timeline, &message, 1));
  cui_chat_room faces[] = {
      {.id = 8, .title = "Kai", .avatar_color = 0x28c9bfff}};
  cui_chat_detail files[] = {
      {.id = 4, .text = "drawing.fig", .detail = "4.2 MB"}};
  cui_chat_detail reactions[] = {{.text = "❤️", .count = 3}};
  message.thread_participants = faces;
  message.thread_participant_count = 1;
  message.author_color = 0x235567ff;
  message.attachments = files;
  message.attachment_count = 1;
  message.reactions = reactions;
  message.reaction_count = 1;
  message.reply_id = 6;
  message.reply_author = "Noor";
  message.reply_text = "Original";
  for (unsigned i = 0; i < 6; ++i) {
    cui_window *leaf =
        cui_window_create(app, "Reusable chat element", 500, 600);
    elements[i] = cui_chat_create(cui_window_root(leaf), CUI_CHAT_MESSAGE + i,
                                  CUI_CHAT_DAYLIGHT);
    assert(elements[i]);
    assert(cui_chat_set_messages(elements[i], &message, 1));
    assert(!cui_chat_set_messages(elements[i], &message, 2));
    cui_on_action(elements[i], action, NULL);
    cui_window_show(leaf);
  }
  faces[0].title = "Changed after copying";
  memset(body, 'x', sizeof(body) - 1);
  cui_chat_room room = {.id = 1, .title = "design-crit"};
  assert(cui_chat_set_rooms(rooms, &room, 1));
  assert(cui_chat_select(rooms, 1));
  cui_set_placeholder(cui_chat_part(composer, 0), "Message #design-crit");
  cui_every(app, 300, verify, NULL);
  cui_window_show(w);
  cui_app_run(app);
  cui_app_destroy(app);
  puts(
      "chat: shared C model, events, composer, and workspace contracts passed");
  return 0;
}
