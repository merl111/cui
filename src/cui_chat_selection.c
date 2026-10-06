#include "cui_chat_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

size_t cui__chat_selected_text(const cui_widget *w, char *buffer, size_t capacity) {
  chat_state *s = NULL;
  for (; w && !s; w = w->parent) s = cui__chat(w);
  if (!s || !s->selection_id || s->selection_anchor == s->selection_end)
    return cui__copy_text("", buffer, capacity);
  size_t a = s->selection_anchor < s->selection_end ? s->selection_anchor : s->selection_end;
  size_t z = s->selection_anchor > s->selection_end ? s->selection_anchor : s->selection_end;
  const cui_chat_message *message = NULL;
  for (size_t i = 0; i < s->count; ++i)
    if (s->messages[i].id == s->selection_id) { message = s->messages + i; break; }
  if (!message) return cui__copy_text("", buffer, capacity);
  size_t total = 0, position = 0;
  for (size_t j = 0; j < message->span_count && position < z; ++j) {
    const char *text = message->spans[j].text;
    size_t length = strlen(text), base = position;
    position += length;
    if (position <= a) continue;
    size_t from = a > base ? a - base : 0;
    size_t to = z < position ? z - base : length;
    size_t bytes = to - from;
    if (buffer && capacity && total < capacity - 1) {
      size_t copy = bytes < capacity - 1 - total ? bytes : capacity - 1 - total;
      memcpy(buffer + total, text + from, copy);
    }
    total += bytes;
  }
  if (buffer && capacity) buffer[total < capacity ? total : capacity - 1] = 0;
  return total;
}
int cui__canvas_copy(cui_widget *w) {
  size_t n = cui__chat_selected_text(w, NULL, 0);
  if (!n) return 0;
  char *text = malloc(n + 1);
  if (!text) return 0;
  cui__chat_selected_text(w, text, n + 1);
  cui_clipboard_set_text(w->window, text);
  free(text); return 1;
}
int cui__chat_selection_event(chat_state *s, const cui_canvas_event *e) {
  if (!cui__chat_message_kind(s->kind)) return 0;
  if (e->kind == CUI_CANVAS_PRESS) {
    s->selection_dragged = 0; s->selecting = 0;
    /* Toolbars, reactions and attachment controls keep their ordinary actions. */
    for (size_t i = 0; i < s->scene.region_count; ++i)
      if (s->scene.regions[i].id == e->id && s->scene.actions[i].action != CUI_CHAT_NONE &&
          s->scene.actions[i].action != CUI_CHAT_LINK) return 0;
    s->selection_id = 0;
    if (cui__chat_text_hit(s, e->x, e->y, 0, &s->selection_id, &s->selection_anchor)) {
      s->selection_end = s->selection_anchor; s->selecting = 1;
      s->selection_x = e->x; s->selection_y = e->y;
    }
    s->dirty = 1;
  } else if (e->kind == CUI_CANVAS_MOVE && s->selecting) {
    if (fabs(e->x - s->selection_x) + fabs(e->y - s->selection_y) > 3) s->selection_dragged = 1;
    if (s->selection_dragged) {
      cui_item_id id;
      cui__chat_text_hit(s, e->x, e->y, 1, &id, &s->selection_end);
      s->dirty = 1;
      return 1;
    }
  } else if (e->kind == CUI_CANVAS_RELEASE) {
    s->selecting = 0;
  } else if (e->kind == CUI_CANVAS_ACTIVATE && s->selection_dragged) {
    s->selection_dragged = 0;
    return 1;
  }
  return 0;
}
void cui__chat_selection_update(chat_state *s, const cui_chat_message *items, size_t count) {
  if (!s->selection_id) return;
  const cui_chat_message *old = NULL, *next = NULL;
  for (size_t i = 0; i < s->count; ++i) if (s->messages[i].id == s->selection_id) old = s->messages + i;
  for (size_t i = 0; i < count; ++i) if (items[i].id == s->selection_id) next = items + i;
  int same = old && next && old->span_count == next->span_count;
  if (same) for (size_t i = 0; i < old->span_count; ++i)
    if (strcmp(old->spans[i].text, next->spans[i].text)) { same = 0; break; }
  if (!same) { s->selection_id = 0; s->selecting = 0; }
  s->scene.word_count = 0;
}
