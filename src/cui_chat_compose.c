#include "cui_chat_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *draft(chat_state *s) {
  size_t n = cui_get_text(s->parts[0], NULL, 0);
  if (n > 65536)
    return NULL;
  char *value = malloc(n + 1);
  if (value)
    cui_get_text(s->parts[0], value, n + 1);
  return value;
}
static int has_text(const char *p) {
  if (p)
    for (; *p; ++p)
      if (*p != ' ' && *p != '\n' && *p != '\r' && *p != '\t')
        return 1;
  return 0;
}
void cui__chat_compose_update(chat_state *s) {
  cui_chat_theme *t = &s->theme;
  cui_widget_style root = {s->presentation.composer == CUI_CHAT_STANDARD
                               ? t->background
                               : t->surface,
                           t->foreground,
                           0,
                           s->presentation.surface_radius,
                           0,
                           s->presentation.composer_padding};
  cui_widget_style row = {
      s->presentation.composer == CUI_CHAT_STANDARD ? t->surface : t->soft,
      t->foreground,
      s->presentation.composer == CUI_CHAT_COMPACT ? t->foreground : t->border,
      s->presentation.composer_radius,
      s->presentation.composer == CUI_CHAT_SOFT      ? 0
      : s->presentation.composer == CUI_CHAT_COMPACT ? 1.5
                                                     : 1,
      6};
  cui_widget_style plain = {row.background, t->foreground, 0, 0, 0, 0};
  cui_set_style(s->root, &root);
  cui_set_style(s->parts[0]->parent, &row);
  for (unsigned i = 0; i < CHAT_PARTS; ++i)
    if (i != 1)
      cui_set_style(s->parts[i], &plain);

  char *value = draft(s);
  if (!value)
    return;
  int enabled = !s->busy && (has_text(value) || s->file_count);
  cui_widget_style send = {
      enabled || s->presentation.composer == CUI_CHAT_STANDARD ? t->accent
                                                               : t->hover,
      enabled || s->presentation.composer == CUI_CHAT_STANDARD ? t->on_accent
                                                               : t->muted,
      0,
      s->presentation.composer == CUI_CHAT_SOFT ? 13 : 10,
      0,
      6};
  cui_set_style(s->parts[1], &send);
  cui_set_enabled(s->parts[1], enabled);
  cui_set_opacity(
      s->parts[1],
      !enabled && s->presentation.composer == CUI_CHAT_STANDARD ? .35 : 1);
  int width = 0, height = 0;
  cui_widget_get_size(s->parts[0], &width, &height);
  double measured = 0, mh = 0;
  cui_text_measure(value, NULL, s->theme.font_size, 400, &measured, &mh);
  unsigned lines = 1;
  for (const char *p = value; *p; ++p)
    if (*p == '\n')
      ++lines;
  if (width > 40)
    lines += (unsigned)(measured / (width - 20));
  int base = s->presentation.composer == CUI_CHAT_COMPACT ? 30
             : s->presentation.composer == CUI_CHAT_SOFT  ? 38
                                                          : 32;
  cui_textarea_set_height(
      s->parts[0],
      (int)fmin(s->presentation.composer == CUI_CHAT_COMPACT ? 120 : 140,
                fmax(base, lines * s->theme.font_size * 1.5 + 12)));
  for (unsigned i = 5; i <= 7; ++i)
    cui_set_visible(s->parts[i],
                    (s->presentation.composer_tools & (1u << i)) != 0);
  cui_set_visible(s->parts[2], s->context != 0);
  cui_set_visible(s->parts[3], s->file_count != 0);
  cui_set_visible(s->parts[4], s->context || s->file_count);
  size_t length = 1;
  for (size_t i = 0; i < s->file_count; ++i)
    length += strlen(s->files[i].text) + 3;
  char *files = calloc(length, 1);
  if (files) {
    for (size_t i = 0; i < s->file_count; ++i) {
      if (i)
        strcat(files, " · ");
      strcat(files, s->files[i].text);
    }
    cui_set_text(s->parts[3], files);
    free(files);
  }
  free(value);
}
int cui_chat_compose_submit(cui_widget *w) {
  chat_state *s = cui__chat(w);
  if (!s || s->kind != CUI_CHAT_COMPOSER || s->busy)
    return 0;
  char *value = draft(s);
  if (!value)
    return 0;
  if (!has_text(value) && !s->file_count) {
    free(value);
    return 0;
  }
  cui__chat_emit(s, (cui_chat_event){.action = CUI_CHAT_SEND,
                                     .id = s->context,
                                     .index = (unsigned)s->editing,
                                     .text = value});
  free(value);
  return 1;
}
static void request(cui_widget *w, void *p) {
  chat_state *s = p;
  if (w == s->parts[0]) {
    cui__chat_compose_update(s);
    cui__chat_emit(s, (cui_chat_event){.action = CUI_CHAT_CHANGED});
  } else if (w == s->parts[1])
    cui_chat_compose_submit(s->root);
  else if (w == s->parts[4]) {
    s->context = 0;
    s->editing = 0;
    cui_chat_compose_files(s->root, NULL, 0);
    cui__chat_emit(s, (cui_chat_event){.action = CUI_CHAT_CANCEL});
  } else
    cui__chat_emit(
        s, (cui_chat_event){.action = w == s->parts[5]   ? CUI_CHAT_ATTACH
                                      : w == s->parts[6] ? CUI_CHAT_EMOJI
                                                         : CUI_CHAT_POLL});
}
static int compose_key(cui_widget *w, cui_key key, unsigned mods, void *p) {
  (void)w;
  chat_state *s = p;
  if (key == CUI_KEY_ENTER && !(mods & CUI_MOD_SHIFT)) {
    cui_chat_compose_submit(s->root);
    return 1;
  }
  if (key == CUI_KEY_ESCAPE && (s->context || s->file_count)) {
    request(s->parts[4], s);
    return 1;
  }
  return 0;
}
int cui__chat_composer(chat_state *s) {
  s->parts[2] = cui_label(s->root, "");
  s->parts[3] = cui_label(s->root, "");
  cui_widget *row = cui_box(s->root, CUI_HORIZONTAL, 6);
  if (!row)
    return 0;
  cui_box_set_padding(s->root, s->presentation.composer_padding);
  s->parts[5] = cui_button(row, "+");
  s->parts[0] = cui_textarea(row, "");
  cui_expand(s->parts[0], 1);
  s->parts[7] = cui_button(row, "▥");
  s->parts[6] = cui_button(row, "☺");
  s->parts[4] = cui_button(row, "×");
  s->parts[1] = cui_button(row, "Send");
  const unsigned parts[] = {1, 4, 5, 6, 7};
  const cui_symbol symbols[] = {CUI_SYMBOL_ARROW_RIGHT, CUI_SYMBOL_CLOSE,
                                s->presentation.composer == CUI_CHAT_SOFT
                                    ? CUI_SYMBOL_PLUS
                                    : CUI_SYMBOL_ATTACH,
                                CUI_SYMBOL_EMOJI, CUI_SYMBOL_POLL};
  for (unsigned i = 0; i < 5; ++i) {
    cui_icon_asset *asset = cui_icon_symbol(symbols[i]);
    cui_set_icon(s->parts[parts[i]], asset);
    cui_icon_release(asset);
    cui_set_icon_only(s->parts[parts[i]],
                      parts[i] != 1 ||
                          s->presentation.composer == CUI_CHAT_STANDARD);
  }
  static const char *names[] = {"Message",
                                "Send",
                                "Reply or edit context",
                                "Attachments",
                                "Cancel reply or edit",
                                "Attach file",
                                "Emoji",
                                "Create poll"};
  for (unsigned i = 0; i < CHAT_PARTS; ++i) {
    if (!s->parts[i])
      return 0;
    cui_accessibility(
        s->parts[i], names[i],
        i == 0 ? "Enter sends; Shift+Enter inserts a newline; Escape cancels"
               : "");
    if (i != 2 && i != 3)
      cui_on_action(s->parts[i], request, s);
  }
  cui_set_icon_trailing(s->parts[1], 1);
  if (s->presentation.composer == CUI_CHAT_COMPACT)
    cui_set_icon(s->parts[1], NULL);
  for (unsigned i = 0; i < 5; ++i) {
    cui_set_icon_size(s->parts[parts[i]], 16);
    cui_set_font(s->parts[parts[i]], NULL, s->theme.font_size * .75, 700);
  }
  cui_on_key(s->parts[0], compose_key, s);
  cui__chat_compose_update(s);
  return 1;
}
int cui_chat_compose_context(cui_widget *w, cui_item_id id, const char *author,
                             const char *preview, int editing) {
  chat_state *s = cui__chat(w);
  if (!s || s->kind != CUI_CHAT_COMPOSER)
    return 0;
  if (!author)
    author = "";
  if (!preview)
    preview = "";
  if (strlen(author) > 4096 || strlen(preview) > 4096)
    return 0;
  char label[8300];
  snprintf(label, sizeof(label),
           editing ? "Editing message · Esc to cancel" : "Replying to %s · %s",
           author, preview);
  cui_set_text(s->parts[2], id ? label : "");
  s->context = id;
  s->editing = !!editing;
  cui__chat_compose_update(s);
  return 1;
}
int cui_chat_compose_busy(cui_widget *w, int busy) {
  chat_state *s = cui__chat(w);
  if (!s || s->kind != CUI_CHAT_COMPOSER)
    return 0;
  s->busy = !!busy;
  cui__chat_compose_update(s);
  return 1;
}
static unsigned bit_count(unsigned n) {
  unsigned count = 0;
  for (; n; n >>= 1)
    count += n & 1;
  return count;
}
static int apply(chat_state *s, unsigned mask) {
  s->mask = mask;
  for (unsigned i = 0; i < 4; ++i)
    cui_set_visible(s->parts[i], !!(mask & (1u << i)));
  cui_set_visible(cui_split_pane(s->splits[0], 0), !!(mask & 9));
  cui_set_visible(cui_split_pane(s->splits[0], 1), !!(mask & 6));
  cui_split_set_position(s->splits[0], !(mask & 6)            ? 1
                                       : !(mask & 9)          ? 0
                                       : bit_count(mask) == 3 ? 1.3 / 2.3
                                                              : .5);
  cui_split_set_position(s->splits[1], !(mask & 8) ? 1 : !(mask & 1) ? 0 : .5);
  cui_split_set_position(s->splits[2], !(mask & 4) ? 1 : !(mask & 2) ? 0 : .5);
  if (!(mask & (1u << s->focus_pane)))
    for (unsigned i = 0; i < 4; ++i)
      if (mask & (1u << i)) {
        s->focus_pane = i;
        break;
      }
  return 1;
}
int cui__chat_workspace(chat_state *s) {
  s->splits[0] = cui_split(s->root, CUI_HORIZONTAL, 1.3 / 2.3);
  if (!s->splits[0])
    return 0;
  s->splits[1] = cui_split(cui_split_pane(s->splits[0], 0), CUI_VERTICAL, .5);
  s->splits[2] = cui_split(cui_split_pane(s->splits[0], 1), CUI_VERTICAL, .5);
  if (!s->splits[1] || !s->splits[2])
    return 0;
  s->parts[0] = cui_split_pane(s->splits[1], 0);
  s->parts[3] = cui_split_pane(s->splits[1], 1);
  s->parts[1] = cui_split_pane(s->splits[2], 0);
  s->parts[2] = cui_split_pane(s->splits[2], 1);
  cui_expand(s->root, 1);
  cui_expand(s->splits[0], 1);
  for (unsigned i = 0; i < 4; ++i) {
    cui_box_set_padding(s->parts[i], 0);
    char label[40];
    snprintf(label, sizeof(label), "Conversation pane %u", i + 1);
    cui_accessibility(s->parts[i], label, "");
  }
  return apply(s, 7);
}
static chat_state *workspace(cui_widget *w) {
  chat_state *s = cui__chat(w);
  return s && s->kind == CUI_CHAT_WORKSPACE ? s : NULL;
}
int cui_chat_workspace_layout(cui_widget *w, unsigned layout) {
  chat_state *s = workspace(w);
  if (!s || layout < 1 || layout > 4)
    return 0;
  s->saved_mask = 0;
  return apply(s, (1u << layout) - 1);
}
int cui_chat_workspace_focus(cui_widget *w, unsigned pane) {
  chat_state *s = workspace(w);
  if (!s || pane > 3 || !(s->mask & (1u << pane)))
    return 0;
  s->focus_pane = pane;
  return 1;
}
int cui_chat_workspace_close(cui_widget *w, unsigned pane) {
  chat_state *s = workspace(w);
  if (!s || pane > 3 || !(s->mask & (1u << pane)) || bit_count(s->mask) == 1)
    return 0;
  s->saved_mask = 0;
  return apply(s, s->mask & ~(1u << pane));
}
int cui_chat_workspace_maximize(cui_widget *w, unsigned pane) {
  chat_state *s = workspace(w);
  if (!s || pane > 3 || !(s->mask & (1u << pane)))
    return 0;
  if (!s->saved_mask)
    s->saved_mask = s->mask;
  return apply(s, 1u << pane);
}
int cui_chat_workspace_restore(cui_widget *w) {
  chat_state *s = workspace(w);
  if (!s)
    return 0;
  if (s->saved_mask) {
    unsigned mask = s->saved_mask;
    s->saved_mask = 0;
    return apply(s, mask);
  }
  return 1;
}
unsigned cui_chat_workspace_mask(const cui_widget *w) {
  chat_state *s = cui__chat(w);
  return s && s->kind == CUI_CHAT_WORKSPACE ? s->mask : 0;
}
unsigned cui_chat_workspace_focused(const cui_widget *w) {
  chat_state *s = cui__chat(w);
  return s && s->kind == CUI_CHAT_WORKSPACE ? s->focus_pane : 0;
}

int cui_chat_compose_cancel(cui_widget *w) {
  chat_state *s = cui__chat(w);
  if (!s || s->kind != CUI_CHAT_COMPOSER)
    return 0;
  request(s->parts[4], s);
  return 1;
}
