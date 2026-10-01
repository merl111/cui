#ifndef CUI_CHAT_INTERNAL_H
#define CUI_CHAT_INTERNAL_H
#include "cui_chat.h"
#include "cui_desktop.h"
#include "cui_internal.h"
#include "cui_layouts.h"
#define CHAT_PARTS 8
#define CHAT_REGIONS 256
/* Commands/text live for one refresh, source strings for the model lifetime. */
typedef struct chat_scene {
  cui_draw_command *commands;
  char **strings;
  size_t count, capacity, string_count;
  cui_canvas_region regions[CHAT_REGIONS];
  cui_chat_event actions[CHAT_REGIONS];
  size_t region_count;
  int failed;
} chat_scene;
typedef struct chat_metric {
  char *text;
  float size, width;
  int weight;
} chat_metric;
typedef struct chat_state {
  cui_widget *root, *parts[CHAT_PARTS], *splits[3];
  cui_chat_kind kind;
  cui_chat_theme theme;
  cui_chat_presentation presentation;
  cui_chat_command *commands;
  size_t command_count;
  cui_chat_message *messages;
  cui_chat_room *rooms;
  size_t count;
  float *tops, *heights;
  double offset, total, scale;
  int width, height, dirty, busy, editing;
  cui_item_id selected, context, hovered;
  unsigned focus_region, hover_region, mask, focus_pane, saved_mask;
  char *query, *status, *event_text;
  cui_chat_detail *files;
  size_t file_count;
  cui_chat_event event;
  chat_scene scene;
  chat_metric *metrics;
  char font_family[129];
  cui_icon_asset *icons[CUI_SYMBOL_COUNT];
} chat_state;
chat_state *cui__chat(const cui_widget *w);
void cui__chat_emit(chat_state *s, cui_chat_event event);
void cui__chat_messages_free(cui_chat_message *items, size_t count);
void cui__chat_rooms_free(cui_chat_room *items, size_t count);
void cui__chat_details_free(cui_chat_detail *items, size_t count);
void cui__chat_scene_free(chat_scene *scene);
int cui__chat_message_kind(cui_chat_kind kind);
int cui__chat_paint(chat_state *s);
int cui__chat_composer(chat_state *s);
void cui__chat_compose_update(chat_state *s);
int cui__chat_workspace(chat_state *s);
int cui__chat_layout(chat_state *s);
#endif
