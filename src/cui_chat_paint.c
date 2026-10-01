#include "cui_chat_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cui__chat_scene_free(chat_scene *p) {
  for (size_t i = 0; i < p->string_count; ++i)
    free(p->strings[i]);
  free(p->strings);
  free(p->commands);
  memset(p, 0, sizeof(*p));
}
static const char *owned(chat_scene *p, const char *text) {
  size_t n = strlen(text);
  char *s = malloc(n + 1);
  if (!s) {
    p->failed = 1;
    return "";
  }
  char **strings =
      realloc(p->strings, (p->string_count + 1) * sizeof(*strings));
  if (!strings) {
    free(s);
    p->failed = 1;
    return "";
  }
  memcpy(s, text, n + 1);
  p->strings = strings;
  p->strings[p->string_count++] = s;
  return s;
}
static cui_draw_command *command(chat_scene *p, cui_draw_op op) {
  static cui_draw_command discarded;
  if (p->count >= 8192) {
    p->failed = 1;
    return &discarded;
  }
  if (p->count == p->capacity) {
    size_t cap = p->capacity ? p->capacity * 2 : 128;
    cui_draw_command *q = realloc(p->commands, cap * sizeof(*q));
    if (!q) {
      p->failed = 1;
      return &discarded;
    }
    p->commands = q;
    p->capacity = cap;
  }
  cui_draw_command *c = p->commands + p->count++;
  memset(c, 0, sizeof(*c));
  c->op = op;
  return c;
}
static void rect(chat_state *s, float x, float y, float w, float h, float r,
                 unsigned color) {
  if (w <= 0 || h <= 0 || y + h < 0 || y > s->height)
    return;
  cui_draw_command *c = command(&s->scene, CUI_DRAW_RECT);
  c->p[0] = x;
  c->p[1] = y;
  c->p[2] = w;
  c->p[3] = h;
  c->p[4] = r;
  c->color = color;
}
static void icon(chat_state *s, cui_symbol symbol, float x, float y, float size,
                 unsigned color) {
  if (!s->icons[symbol])
    s->icons[symbol] = cui_icon_symbol(symbol);
  if (!s->icons[symbol]) {
    s->scene.failed = 1;
    return;
  }
  cui_draw_command *c = command(&s->scene, CUI_DRAW_ICON);
  c->p[0] = x;
  c->p[1] = y;
  c->p[2] = size;
  c->p[3] = size;
  c->color = color;
  c->icon = s->icons[symbol];
}
static void border(chat_state *s, float x, float y, float w, float h, float r,
                   float thickness, unsigned stroke, unsigned fill) {
  rect(s, x, y, w, h, r, stroke);
  rect(s, x + thickness, y + thickness, w - 2 * thickness, h - 2 * thickness,
       fmaxf(0, r - thickness), fill);
}
static void text(chat_state *s, float x, float y, float w, float size,
                 int weight, unsigned color, const char *value) {
  if (!*value || w <= 0 || y + size * 1.5 < 0 || y > s->height)
    return;
  cui_draw_command *c = command(&s->scene, CUI_DRAW_TEXT);
  c->p[0] = x;
  c->p[1] = y;
  c->p[2] = w;
  c->p[3] = size;
  c->p[4] = (float)weight;
  c->color = color;
  c->text = owned(&s->scene, value);
  c->font = s->font_family;
}
static void centered_text(chat_state *s, float x, float y, float w, float h,
                          float size, int weight, unsigned color,
                          const char *value) {
  size_t before = s->scene.count;
  text(s, x, y, w, size, weight, color, value);
  if (s->scene.count > before)
    s->scene.commands[s->scene.count - 1].p[6] = h;
}
/* An outline must not paint over the content it encloses. Approximate the
 * rounded corners with short antialiased line segments, preserving alpha. */
static void focus_outline(chat_state *s, float x, float y, float w, float h,
                          float radius, unsigned color) {
  if (w <= 0 || h <= 0) return;
  float r = fminf(radius, fminf(w, h) / 2), first_x = 0, first_y = 0,
        previous_x = 0, previous_y = 0;
  for (int corner = 0; corner < 4; ++corner) {
    float cx = corner == 0 || corner == 3 ? x + r : x + w - r;
    float cy = corner < 2 ? y + r : y + h - r;
    for (int step = 0; step <= 6; ++step) {
      float angle = (float)(3.141592653589793 * (1 + corner * .5 + step / 12.));
      float px = cx + r * cosf(angle), py = cy + r * sinf(angle);
      if (corner || step) {
        cui_draw_command *c = command(&s->scene, CUI_DRAW_LINE);
        c->p[0] = previous_x; c->p[1] = previous_y;
        c->p[2] = px; c->p[3] = py; c->p[4] = 1.5f; c->color = color;
      } else { first_x = px; first_y = py; }
      previous_x = px; previous_y = py;
    }
  }
  cui_draw_command *c = command(&s->scene, CUI_DRAW_LINE);
  c->p[0] = previous_x; c->p[1] = previous_y;
  c->p[2] = first_x; c->p[3] = first_y; c->p[4] = 1.5f; c->color = color;
}
static float measure(chat_state *s, const char *text, float size, int weight) {
  if (!s->metrics)
    s->metrics = calloc(8192, sizeof(*s->metrics));
  unsigned hash = 2166136261u;
  for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
    hash = (hash ^ *p) * 16777619u;
  hash = (hash ^ (unsigned)(size * 100)) * 16777619u ^ (unsigned)weight;
  chat_metric *slot = s->metrics ? s->metrics + (hash % 8192) : NULL;
  if (slot && slot->text && slot->size == size && slot->weight == weight &&
      !strcmp(slot->text, text))
    return slot->width;
  double width = 0, height = 0;
  cui_text_measure(text, s->font_family, size, weight, &width, &height);
  if (slot) {
    size_t n = strlen(text);
    char *copy = malloc(n + 1);
    if (copy) {
      memcpy(copy, text, n + 1);
      free(slot->text);
      *slot = (chat_metric){copy, size, (float)width, weight};
    }
  }
  return (float)width;
}
static unsigned tint(unsigned color, unsigned alpha) {
  return (color & 0xffffff00u) | (alpha & 255u);
}
static void action(chat_state *s, unsigned id, float x, float y, float w,
                   float h, const char *label, cui_chat_event event,
                   int enabled) {
  float left = fmaxf(0, x), top = fmaxf(0, y),
        right = fminf((float)s->width, x + w),
        bottom = fminf((float)s->height, y + h);
  chat_scene *p = &s->scene;
  if (right <= left || bottom <= top || p->region_count >= CHAT_REGIONS)
    return;
  size_t i = p->region_count++;
  p->regions[i] = (cui_canvas_region){
      id, left, top, right - left, bottom - top, owned(p, label), enabled};
  p->actions[i] = event;
  if (event.text)
    p->actions[i].text = owned(p, event.text);
  if (s->focus_region == id) {
    int room = event.action == CUI_CHAT_OPEN_ROOM && s->kind == CUI_CHAT_ROOMS;
    float radius = room && s->presentation.rooms == CUI_CHAT_SOFT ? 12 : 5;
    unsigned color = room && event.id == s->selected &&
                             s->presentation.rooms == CUI_CHAT_SOFT
                         ? s->theme.surface : s->theme.accent;
    focus_outline(s, left + 2, top + 2, right - left - 4,
                  bottom - top - 4, radius, color);
  }
}
static void avatar(chat_state *s, float x, float y, float size,
                   const char *name, unsigned color, unsigned flags) {
  float radius = s->presentation.avatar_border_width > 0
                     ? 7
                     : (flags & CUI_CHAT_SQUARE ? size * .3f : size * .5f);
  if (s->presentation.avatar_border_width > 0)
    border(s, x, y, size, size, radius, s->presentation.avatar_border_width,
           s->theme.foreground, color);
  else
    rect(s, x, y, size, size, radius, color);
  while (*name == '#' || *name == '@' || *name == '!' || *name == ' ')
    ++name;
  char initial[8] = {0};
  size_t n = 0;
  if (*name) {
    n = 1;
    while (n < 7 && ((unsigned char)name[n] & 0xc0) == 0x80)
      ++n;
    memcpy(initial, name, n);
  }
  if (initial[0] >= 'a' && initial[0] <= 'z')
    initial[0] -= 32;
  float fs = size * .43f, tw = measure(s, initial, fs, 700);
  text(s, x + (size - tw) / 2, y + (size - fs * 1.25f) / 2, size, fs, 700,
       cui_chat_color(.2, .03, 265, 1), initial);
  if (flags & CUI_CHAT_ONLINE) {
    rect(s, x + size - 9, y + size - 9, 10, 10, 5, s->theme.surface);
    rect(s, x + size - 7, y + size - 7, 6, 6, 3, s->theme.online);
  }
}
/* Break only on UTF-8 boundaries, retain newlines and native shaping. Each word
 * is measured with the same font backend as the rendered command. */
static size_t scalar_bytes(const char *p) {
  size_t n = 1;
  while (p[n] && ((unsigned char)p[n] & 0xc0) == 0x80)
    ++n;
  return n;
}
typedef struct paragraph_size {
  float width, height, last;
} paragraph_size;
static paragraph_size paragraph(chat_state *s, const cui_chat_message *m,
                                float x, float y, float width, unsigned color,
                                int paint) {
  float size = (float)s->theme.font_size, line = size * 1.5f, cx = 0, cy = 0,
        maximum = 0;
  int any = 0;
  for (size_t i = 0; i < m->span_count; ++i) {
    const cui_chat_span *span = m->spans + i;
    const char *p = span->text;
    int weight =
        span->style == CUI_CHAT_STRONG || span->style == CUI_CHAT_MENTION ? 700
                                                                          : 400;
    unsigned ink = span->style == CUI_CHAT_MUTED ? s->theme.muted : color;
    if (span->style == CUI_CHAT_MENTION && s->presentation.mention_foreground)
      ink = s->presentation.mention_foreground;
    while (*p) {
      if (*p == '\n') {
        maximum = fmaxf(maximum, cx);
        cx = 0;
        cy += line;
        ++p;
        any = 1;
        continue;
      }
      size_t n = 0;
      while (p[n] && p[n] != '\n' && p[n] != ' ' && p[n] != '\t')
        ++n;
      if (n == 0)
        n = 1;
      char *word = malloc(n + 1);
      if (!word) {
        s->scene.failed = 1;
        return (paragraph_size){0};
      }
      memcpy(word, p, n);
      word[n] = 0;
      float w = measure(s, word, size, weight);
      if (cx > 0 && cx + w > width) {
        maximum = fmaxf(maximum, cx);
        cx = 0;
        cy += line;
        if (n == 1 && (*p == ' ' || *p == '\t')) {
          free(word);
          p += n;
          continue;
        }
      }
      /* Oversized words are split using scalar boundaries. */
      if (w > width) {
        free(word);
        n = 0;
        size_t step = scalar_bytes(p);
        while (p[n] && p[n] != '\n' && p[n] != ' ') {
          size_t next = n + scalar_bytes(p + n);
          char *part = malloc(next + 1);
          if (!part) {
            s->scene.failed = 1;
            break;
          }
          memcpy(part, p, next);
          part[next] = 0;
          float pw = measure(s, part, size, weight);
          free(part);
          if (next > step && pw > width)
            break;
          n = next;
          w = pw;
        }
        if (!n)
          n = step;
        word = malloc(n + 1);
        if (!word) {
          s->scene.failed = 1;
          return (paragraph_size){0};
        }
        memcpy(word, p, n);
        word[n] = 0;
        w = measure(s, word, size, weight);
      }
      if (paint) {
        if (span->style == CUI_CHAT_MENTION || span->style == CUI_CHAT_CODE)
          rect(s, x + cx - 2, y + cy, w + 4, line, 3,
               span->style == CUI_CHAT_MENTION
                   ? s->presentation.mention_background
                   : s->theme.soft);
        text(s, x + cx, y + cy, width - cx, size, weight, ink, word);
        if (span->link && *span->link)
          action(s, 0x80000000u + (unsigned)s->scene.region_count, x + cx,
                 y + cy, w, line, span->link,
                 (cui_chat_event){
                     .action = CUI_CHAT_LINK, .id = m->id, .text = span->link},
                 1);
      }
      cx += w;
      maximum = fmaxf(maximum, cx);
      any = 1;
      p += n;
      free(word);
    }
  }
  return (paragraph_size){maximum, any ? cy + line : 0, cx};
}
static float date_height(const chat_state *s, const cui_chat_message *m) {
  return *m->date ? (s->presentation.messages == CUI_CHAT_SOFT      ? 49
                     : s->presentation.messages == CUI_CHAT_COMPACT ? 32
                                                                    : 42)
                  : 0;
}
static void date(chat_state *s, const cui_chat_message *m, float y) {
  if (!*m->date)
    return;
  float tw = measure(s, m->date, 11.5, 500), left = (s->width - tw) / 2.f;
  if (s->presentation.messages == CUI_CHAT_SOFT)
    rect(s, left - 12, y + 9, tw + 24, 23, 10, s->theme.soft);
  else if (s->presentation.messages != CUI_CHAT_COMPACT) {
    rect(s, 20, y + 20, left - 32, 1, 0, s->theme.border);
    rect(s, left + tw + 12, y + 20, s->width - left - tw - 32, 1, 0,
         s->theme.border);
  }
  text(s, left, y + 12, tw + 1, 11.5, 500, s->theme.muted, m->date);
}
typedef struct message_metrics {
  float x, width, body_height, height, header, padding, avatar;
  paragraph_size body;
} message_metrics;
static message_metrics dimensions(chat_state *s, const cui_chat_message *m) {
  int bubble = s->presentation.messages == CUI_CHAT_SOFT;
  int tiles = s->presentation.messages == CUI_CHAT_COMPACT;
  int outgoing = bubble && (m->flags & CUI_CHAT_OUTGOING);
  float pad = bubble ? 22 : tiles ? 20 : 20;
  float av = bubble ? 28 : tiles ? 26 : 32, gap = bubble ? 10 : tiles ? 9 : 18;
  float width = fmaxf(40, s->width - pad * 2 - av - gap);
  if (bubble)
    width = fmaxf(40, (s->width - 44) * .78f - (outgoing ? 0 : 38) - 28);
  else if (!tiles)
    width = fminf(width, measure(s, "0", s->theme.font_size, 400) * 72);
  paragraph_size body =
      bubble && m->option_count
          ? (paragraph_size){0}
          : paragraph(s, m, 0, 0, width, s->theme.foreground, 0);
  float content = body.width, h = body.height;
  if (m->reply_id || *m->reply_text) {
    content = fmaxf(content,
                    fminf(width, measure(s, m->reply_author, 12, 700) +
                                     measure(s, m->reply_text, 12, 400) + 30));
    h += 32;
  }
  if (m->attachment_count) {
    content = fmaxf(content, bubble ? 240 : 340);
    h += m->attachment_count * (bubble ? 66 : tiles ? 56 : 68);
  }
  if (m->option_count) {
    content = fmaxf(content, bubble ? 292 : tiles ? 350 : 380);
    h += bubble  ? 50 + m->option_count * 43
         : tiles ? 64 + m->option_count * 37
                 : 80 + m->option_count * 45;
  }
  if (bubble && m->thread_count) {
    h += 26;
    content = fmaxf(content, measure(s, "3 replies in thread →", 13, 700));
  }
  if (bubble) {
    float stamp = measure(s, m->time, 10.5, 400) + 8;
    if (m->attachment_count || m->option_count || m->thread_count ||
        body.last + stamp > width)
      h += 20;
    else
      content = fmaxf(content, body.last + stamp);
    width = fminf(width, content);
  }
  float body_h = h + (bubble ? 18 : 0);
  if (m->reaction_count)
    h += bubble ? 30 : tiles ? 28 : 32;
  if (!bubble && m->thread_count)
    h += tiles ? 34 : 58;
  if (bubble)
    h += 18;
  float header = (m->flags & CUI_CHAT_CONTINUED) || outgoing ||
                         !s->presentation.show_sender
                     ? 0
                 : bubble ? 23
                          : 21;
  h += header +
       ((m->flags & CUI_CHAT_CONTINUED) ? 3
        : bubble                        ? 12
        : tiles                         ? 10
                                        : 12) +
       date_height(s, m);
  float x = pad + av + gap;
  if (outgoing)
    x = s->width - 22 - width - 28;
  return (message_metrics){x, width, body_h, h, header, pad, av, body};
}
int cui__chat_layout(chat_state *s) {
  if (s->kind != CUI_CHAT_TIMELINE)
    return 1;
  float y = s->presentation.messages == CUI_CHAT_SOFT      ? 18
            : s->presentation.messages == CUI_CHAT_COMPACT ? 10
                                                           : 16;
  for (size_t i = 0; i < s->count; ++i) {
    message_metrics d = dimensions(s, s->messages + i);
    s->tops[i] = y;
    s->heights[i] = d.height;
    y += d.height;
  }
  s->total = y + 8;
  return !s->scene.failed;
}
static void attachment(chat_state *s, const cui_chat_message *m, size_t index,
                       float x, float y, float width, unsigned base) {
  const cui_chat_detail *a = m->attachments + index;
  int tiles = s->presentation.messages == CUI_CHAT_COMPACT;
  float h = tiles                                       ? 50
            : s->presentation.messages == CUI_CHAT_SOFT ? 60
                                                        : 62,
        w = fminf(width, 340), r = tiles ? 8 : 12;
  border(s, x, y, w, h, r, tiles ? 1.5f : 1, s->theme.border, s->theme.surface);
  if (!tiles)
    rect(s, x + 10, y + 9, 36, 36, 10, s->presentation.attachment_background);
  icon(s, CUI_SYMBOL_FILE, x + (tiles ? 8 : 18), y + 17, tiles ? 17 : 20,
       tiles ? s->theme.foreground : cui_chat_color(.22, .03, 260, 1));
  if (s->presentation.messages == CUI_CHAT_STANDARD)
    icon(s, CUI_SYMBOL_DOWNLOAD, x + w - 30, y + 20, 16, s->theme.muted);
  float inset = tiles ? 35 : 56;
  text(s, x + inset, y + 8, w - inset - 10, 13, 650, s->theme.foreground, a->text);
  char metadata[4097];
  snprintf(metadata, sizeof(metadata), "%s", a->detail);
  if (s->presentation.messages != CUI_CHAT_STANDARD) {
    char *separator = strstr(metadata, " · ");
    if (separator)
      *separator = 0;
  }
  text(s, x + inset, y + 28, w - inset - 10, 12, 400, s->theme.muted, metadata);
  action(s, base + 32 + (unsigned)index, x, y, w, h, a->text,
         (cui_chat_event){
             .action = CUI_CHAT_ATTACHMENT, .id = m->id, .detail_id = a->id},
         !(a->flags & CUI_CHAT_DISABLED));
}
static float poll(chat_state *s, const cui_chat_message *m, float x, float y,
                  float width, unsigned base) {
  if (!m->option_count)
    return 0;
  int tiles = s->presentation.messages == CUI_CHAT_COMPACT,
      bubble = s->presentation.messages == CUI_CHAT_SOFT;
  float row = bubble  ? 43
              : tiles ? 37
                      : 45,
        h = bubble  ? 50 + m->option_count * row
            : tiles ? 64 + m->option_count * row
                    : 80 + m->option_count * row;
  float w = fminf(width, bubble ? 348 : 380), pad = bubble  ? 0
                                                    : tiles ? 10
                                                            : 14;
  if (!bubble)
    border(s, x, y, w, h - 6, tiles ? 8 : 12, tiles ? 1.5f : 1,
           tiles ? s->theme.foreground : s->theme.border, s->theme.surface);
  text(s, x + pad, y + pad, w - pad * 2, tiles ? 13 : 14, 700,
       s->theme.foreground, m->poll_question);
  uint64_t votes = 0;
  for (size_t i = 0; i < m->option_count; ++i)
    votes += m->options[i].count;
  for (size_t i = 0; i < m->option_count; ++i) {
    float py = y + pad + 27 + (float)i * row, ow = w - 2 * pad,
          oh = bubble  ? 37
               : tiles ? 32
                       : 39;
    int selected = m->selected_option == (int)i;
    border(s, x + pad, py, ow, oh, tiles ? 6 : 12, selected ? 1.5f : 1,
           selected ? s->theme.foreground : s->theme.border, s->theme.surface);
    float fill = votes && m->selected_option >= 0
                     ? (float)((double)m->options[i].count / votes)
                     : 0;
    if (fill > 0)
      rect(s, x + pad + 2, py + 2, (ow - 4) * fill, oh - 4, tiles ? 4 : 10,
           cui_chat_color(.90, .06,
                          bubble  ? 200
                          : tiles ? 292
                                  : 235,
                          1));
    text(s, x + pad + 10, py + 7, ow - 62, 13, selected ? 650 : 400,
         s->theme.foreground, m->options[i].text);
    char count[32];
    snprintf(count, sizeof(count), "%u", m->options[i].count);
    if (m->selected_option >= 0)
      text(s, x + pad + ow - 36, py + 7, 28, 12, 500, s->theme.muted, count);
    action(s, base + 80 + (unsigned)i, x + pad, py, ow, oh, m->options[i].text,
           (cui_chat_event){
               .action = CUI_CHAT_VOTE, .id = m->id, .index = (unsigned)i},
           !(m->flags & CUI_CHAT_CLOSED));
  }
  char footer[80];
  snprintf(footer, sizeof(footer), "%llu votes · %s", (unsigned long long)votes,
           m->flags & CUI_CHAT_CLOSED ? "Poll closed" : "Select an option");
  text(s, x + pad, y + h - 27, w - pad * 2, 12, 400, s->theme.muted,
       m->selected_option < 0
           ? (bubble ? "Tap an option to vote" : "Vote to see results")
           : footer);
  return h;
}
static void hover_actions(chat_state *s, const cui_chat_message *m, float y,
                          unsigned base) {
  int focus = 0;
  for (size_t i = 0; i < s->scene.region_count; ++i)
    if (s->scene.regions[i].id == s->focus_region &&
        s->scene.actions[i].id == m->id)
      focus = 1;
  if (s->hovered != m->id && !focus)
    return;
  int day = s->presentation.messages == CUI_CHAT_SOFT;
  size_t count = s->command_count;
  if (!count)
    return;
  float w = count * 30 + 8, x = s->width - w - 20, y0 = fmaxf(1, y - 14);
  if (day) {
    message_metrics d = dimensions(s, m);
    x = d.x + d.width + 28 - w - 6;
  }
  x = fmaxf(0, x);
  border(s, x, y0, w, 34, day ? 12 : 9, 1, s->theme.border,
         day ? s->theme.surface : s->theme.soft);
  for (size_t i = 0; i < count; ++i) {
    const cui_chat_command *c = s->commands + i;
    if (c->flags & CUI_CHAT_MINE)
      rect(s, x + 4 + i * 30, y0 + 3, 30, 28, 6, s->theme.hover);
    if (*c->text)
      text(s, x + 8 + i * 30, y0 + 7, 22, 14, 400, s->theme.foreground,
           c->text);
    else
      icon(s, c->symbol, x + 10 + i * 30, y0 + 10, 15, s->theme.muted);
    action(s, base + 200 + (unsigned)i, x + 4 + i * 30, y0 + 3, 30, 28,
           c->label,
           (cui_chat_event){.action = c->action,
                            .id = m->id,
                            .detail_id = c->id,
                            .index = UINT32_MAX,
                            .text = c->text},
           !(c->flags & CUI_CHAT_DISABLED));
  }
}
static float reply_preview(chat_state *s, const cui_chat_message *m, float x,
                           float py, float width, unsigned foreground) {
  int bubble = s->presentation.messages == CUI_CHAT_SOFT;
  float start = py;
  if (m->reply_id || *m->reply_text) {
    float aw = measure(s, m->reply_author, 12, 700);
    float rw = bubble
                   ? width
                   : fminf(width, aw + measure(s, m->reply_text, 12, 400) + 30);
    if (s->presentation.messages == CUI_CHAT_COMPACT)
      rect(s, x - 4, py + 2, 2.5, 22, 0, s->theme.foreground);
    else
      rect(s, x - 4, py, rw + 8, 26, 10,
           bubble ? tint(s->theme.foreground, 18) : s->theme.soft);
    text(s, x + 6, py + 4, rw - 12, 12, 700, foreground, m->reply_author);
    text(s, x + aw + 10, py + 4, rw - aw - 16, 12, 400,
         bubble ? foreground : s->theme.muted, m->reply_text);
    py += 32;
  }
  return py - start;
}
static float reactions(chat_state *s, const cui_chat_message *m, float x,
                       float py, unsigned base) {
  int tiles = s->presentation.messages == CUI_CHAT_COMPACT;
  float start = py;
  if (m->reaction_count) {
    float rx = x;
    py += 4;
    for (size_t i = 0; i < m->reaction_count; ++i) {
      const cui_chat_detail *r = m->reactions + i;
      char value[4200];
      snprintf(value, sizeof(value), "%s  %u", r->text, r->count);
      float w = measure(s, value, 12, 500) + 18, h = tiles ? 24 : 26;
      border(s, rx, py, w, h, tiles ? 6 : 13,
             (r->flags & CUI_CHAT_MINE) ? 1.5f : 1,
             (r->flags & CUI_CHAT_MINE) ? s->theme.foreground : s->theme.border,
             s->theme.surface);
      text(s, rx + 9, py + 4, w - 18, 12, 500, s->theme.foreground, value);
      action(s, base + 48 + (unsigned)i, rx, py, w, h, value,
             (cui_chat_event){.action = CUI_CHAT_REACT,
                              .id = m->id,
                              .index = (unsigned)i,
                              .text = r->text},
             1);
      rx += w + 4;
    }
    py += tiles ? 24 : 28;
  }
  return py - start;
}
static void thread_summary(chat_state *s, const cui_chat_message *m, float x,
                           float py, float width, unsigned base) {
  if (!m->thread_count)
    return;
  if (s->presentation.messages == CUI_CHAT_SOFT) {
    char title[80];
    snprintf(title, sizeof(title), "%u replies in thread →", m->thread_count);
    unsigned ink =
        m->flags & CUI_CHAT_OUTGOING ? s->theme.on_accent : s->theme.foreground;
    text(s, x, py + 4, width, 13, 700, ink, title);
    action(s, base + 100, x, py, width, 26, title,
           (cui_chat_event){.action = CUI_CHAT_THREAD, .id = m->id}, 1);
    return;
  }
  int tiles = s->presentation.messages == CUI_CHAT_COMPACT;
  py += 5;
  char title[4200];
  if (tiles && *m->thread_preview)
    snprintf(title, sizeof(title), "%u replies · %s", m->thread_count, m->thread_preview);
  else
    snprintf(title, sizeof(title), "%u replies", m->thread_count);
  size_t faces = tiles ? 0 : m->thread_participant_count;
  float inset = faces ? 16 * faces + 24 : 10;
  float w = fminf(width,
                  tiles ? measure(s, title, 12, 600) + 20
                        : measure(s, m->thread_preview, 12, 400) + inset + 14),
        h = tiles ? 28 : 56;
  border(s, x, py, w, h, tiles ? 6 : 10, tiles ? 1.5f : 1, s->theme.border,
         s->theme.surface);
  for (size_t i = 0; i < faces; ++i) {
    const cui_chat_room *face = m->thread_participants + i;
    rect(s, x + 8 + 16 * i, py + 17, 22, 22, 11, s->theme.surface);
    avatar(s, x + 9 + 16 * i, py + 18, 20, face->title, face->avatar_color, 0);
  }
  text(s, x + inset, py + (tiles ? 6 : 9), w - inset - 10, tiles ? 12 : 13, 650, s->theme.foreground,
       title);
  if (!tiles)
    text(s, x + inset, py + 29, w - inset - 10, 12, 400, s->theme.muted,
         m->thread_preview);
  action(s, base + 100, x, py, w, h, title,
         (cui_chat_event){.action = CUI_CHAT_THREAD, .id = m->id}, 1);
}
static void message(chat_state *s, const cui_chat_message *m, size_t index,
                    float top) {
  message_metrics d = dimensions(s, m);
  int bubble = s->presentation.messages == CUI_CHAT_SOFT,
      tiles = s->presentation.messages == CUI_CHAT_COMPACT;
  unsigned base = (unsigned)(index + 1) * 256;
  date(s, m, top);
  float y = top + date_height(s, m) +
            ((m->flags & CUI_CHAT_CONTINUED) ? 0
             : bubble                        ? 10
                                             : 5);
  float content_y = y + d.header;
  if (!bubble && s->hovered == m->id && !(m->flags & CUI_CHAT_HIGHLIGHT))
    rect(s, 0, y, s->width, d.height - date_height(s, m), 0,
         tint(s->theme.foreground, 10));
  if (!bubble && (m->flags & CUI_CHAT_HIGHLIGHT)) {
    rect(s, 0, y, s->width, d.height - date_height(s, m), 0,
         tint(s->theme.accent, 18));
    rect(s, 0, y, tiles ? 2.5f : 2, d.height - date_height(s, m), 0,
         s->theme.accent);
  }
  int outgoing = bubble && (m->flags & CUI_CHAT_OUTGOING);
  int last_group = index + 1 == s->count ||
                   !(s->messages[index + 1].flags & CUI_CHAT_CONTINUED);
  if (!outgoing && (bubble ? last_group : !(m->flags & CUI_CHAT_CONTINUED)))
    avatar(s, d.padding, bubble ? top + d.height - 28 : y, d.avatar, m->author,
           m->avatar_color, m->flags & ~CUI_CHAT_ONLINE);
  if (!(m->flags & CUI_CHAT_CONTINUED) && !outgoing &&
      s->presentation.show_sender) {
    if (!bubble) {
      text(s, d.x, y, d.width, 14, 650,
           m->author_color ? m->author_color : m->avatar_color, m->author);
      float aw = measure(s, m->author, 14, 650);
      text(s, d.x + aw + 9, y + 2, 60, 11.5, 400, s->theme.muted, m->time);
    } else {
      unsigned c = m->avatar_color;
      unsigned ink = ((unsigned)(((c >> 24) & 255) * .48f) << 24) |
                     ((unsigned)(((c >> 16) & 255) * .48f) << 16) |
                     ((unsigned)(((c >> 8) & 255) * .48f) << 8) | 255;
      text(s, d.x + 12, y, d.width, 12, 700,
           m->author_color ? m->author_color : ink, m->author);
    }
  }
  unsigned foreground = bubble && (m->flags & CUI_CHAT_OUTGOING)
                            ? s->theme.on_accent
                            : s->theme.foreground;
  if (bubble) {
    unsigned bg =
        (m->flags & CUI_CHAT_OUTGOING) ? s->theme.accent : s->theme.soft;
    if (s->hovered == m->id && !outgoing) bg = s->theme.hover;
    rect(s, d.x, content_y, d.width + 28, d.body_height,
         s->presentation.bubble_radius, bg);
    /* Last bubble corner is six pixels; a small same-color square and
     * rounded corner produces an asymmetric tail without extra geometry. */
    float tx = (m->flags & CUI_CHAT_OUTGOING) ? d.x + d.width + 28 - 18 : d.x;
    if (last_group && s->presentation.bubble_radius >= 6)
      rect(s, tx, content_y + d.body_height - 18, 18, 18, 6, bg);
  }
  action(s, base + 1, d.x, content_y, d.width + (bubble ? 28 : 0),
         fmaxf(21, d.body_height), m->author,
         (cui_chat_event){.action = CUI_CHAT_MORE, .id = m->id}, 1);
  float x = d.x + (bubble ? 14 : 0), py = content_y + (bubble ? 9 : 0);
  py += reply_preview(s, m, x, py, d.width, foreground);
  if (!(bubble && m->option_count))
    paragraph(s, m, x, py, d.width, foreground, 1);
  py += d.body.height;
  for (size_t i = 0; i < m->attachment_count; ++i) {
    py += 6;
    attachment(s, m, i, x, py, d.width, base);
    py += bubble ? 60 : tiles ? 50 : 62;
  }
  if (m->option_count)
    py += poll(s, m, x, py + (bubble ? 0 : 6), d.width, base);
  if (bubble) {
    if (m->thread_count) {
      thread_summary(s, m, x, py, d.width, base);
      py += 26;
    }
    float tx = x, ty = py;
    float stamp = measure(s, m->time, 10.5, 400) + 8;
    if (!m->attachment_count && !m->option_count && !m->thread_count &&
        d.body.last + stamp <= d.width + .5f) {
      tx = x + d.body.last + 8;
      ty = py - s->theme.font_size * 1.5f + 3;
    }
    text(s, tx, ty + 2, stamp, 10.5, 400,
         outgoing ? tint(foreground, 180) : s->theme.muted, m->time);
    py = content_y + d.body_height;
  }
  py += reactions(s, m, bubble ? d.x : x, py, base);
  if (!bubble)
    thread_summary(s, m, x, py, d.width, base);
  hover_actions(s, m, y, base);
}
static int matches(const cui_chat_room *r, const char *query) {
  if (!*query)
    return 1;
  char *a = cui__search_key(r->title), *b = cui__search_key(r->detail),
       *q = cui__search_key(query);
  int match = a && b && q && (strstr(a, q) || strstr(b, q));
  free(a);
  free(b);
  free(q);
  return match;
}
static void rooms(chat_state *s) {
  int tiles = s->presentation.rooms == CUI_CHAT_COMPACT,
      day = s->presentation.rooms == CUI_CHAT_SOFT;
  float y = 4, margin = tiles ? 8 : day ? 10 : 8;
  const char *group = "";
  for (size_t i = 0; i < s->count; ++i) {
    const cui_chat_room *r = s->rooms + i;
    if (!matches(r, s->query))
      continue;
    if (strcmp(group, r->group)) {
      group = r->group;
      text(s, margin + 8, y + 9 - (float)s->offset, s->width - margin * 2, 11,
           650, s->theme.muted, group);
      y += 32;
    }
    float h = s->presentation.room_height, py = y - (float)s->offset,
          w = s->width - margin * 2;
    if (py + h >= 0 && py < s->height) {
      int selected = r->id == s->selected || (r->flags & CUI_CHAT_MINE);
      unsigned fg = selected && day ? s->theme.surface : s->theme.foreground;
      if (!selected && s->hover_region == i + 1)
        rect(s, margin, py, w, h, tiles ? 7 : day ? 14 : 9, s->theme.soft);
      if (selected)
        border(s, margin, py, w, h,
               tiles ? 7
               : day ? 14
                     : 9,
               tiles ? 1.5f : 1,
               tiles ? s->theme.foreground
               : day ? s->theme.foreground
                     : s->theme.border,
               day     ? s->theme.foreground
               : tiles ? s->theme.surface
                       : s->theme.soft);
      float x = margin + 8;
      if (tiles) {
        border(s, x, py + 12, 10, 10, (r->flags & CUI_CHAT_SQUARE) ? 3 : 5,
               1.5, s->theme.foreground, r->avatar_color);
        x += 19;
      } else {
        float size = fminf(h - 8, day ? 40 : 32);
        avatar(s, x, py + (h - size) / 2, size, r->title, r->avatar_color,
               r->flags);
        x += size + (day ? 12 : 10);
      }
      int preview = !tiles && h >= 44 && s->presentation.show_room_previews && *r->detail;
      if (!tiles && !preview)
        centered_text(s, x, py, w - (x - margin) - 40, h, 14,
                      day || r->unread ? 650 : 550, fg, r->title);
      else text(s, x, py + 7, w - (x - margin) - 40, tiles ? 13.5 : 14,
           day         ? 650
           : r->unread ? 700
                       : 550,
           fg, r->title);
      if (preview)
        text(s, x, py + 29, w - (x - margin) - (day ? 48 : 24), day ? 12.5 : 12,
             400,
             selected && day ? tint(s->theme.surface, 180) : s->theme.muted,
             r->detail);
      if (r->unread) {
        char badge[32];
        snprintf(badge, sizeof(badge), "%u", r->unread);
        float bw = fmaxf(20, measure(s, badge, 11, 700) + 10);
        unsigned bg =
            (r->flags & CUI_CHAT_HIGHLIGHT) ? (tiles ? s->theme.accent : s->theme.danger)
                                          : (tiles ? s->theme.foreground : s->theme.soft);
        rect(s, s->width - margin - bw - 8, py + (day ? 30 : 8), bw,
             tiles ? 18 : 20, tiles ? 4 : 10, bg);
        text(s, s->width - margin - bw - 3, py + (day ? 32 : 10), bw - 6, 11,
             700,
             tiles ? s->theme.surface : (r->flags & CUI_CHAT_HIGHLIGHT) ? s->theme.on_accent
                                             : s->theme.foreground,
             badge);
      }
      if (day) {
        float tw = measure(s, r->trailing, 10.5, 400);
        text(s, s->width - margin - 10 - tw, py + 9, tw + 1, 10.5, 400,
             s->theme.muted, r->trailing);
      } else if (tiles && *r->trailing) {
        float tw = measure(s, r->trailing, 10, 600);
        float end = s->width - margin - (r->unread ? 36 : 8);
        text(s, end - tw, py + 10, tw + 1, 10, 600, s->theme.muted, r->trailing);
      }
      action(s, (unsigned)i + 1, margin, py, w, h, r->title,
             (cui_chat_event){.action = CUI_CHAT_OPEN_ROOM, .id = r->id},
             !(r->flags & CUI_CHAT_DISABLED));
    }
    y += h + 2;
  }
  s->total = y + 12;
}
static void spaces(chat_state *s) {
  int rail = s->presentation.spaces == CUI_VERTICAL;
  float cursor = rail ? 12 : 4;
  for (size_t i = 0; i < s->count; ++i) {
    const cui_chat_room *r = s->rooms + i;
    if (rail && i && *r->group) {
      rect(s, 22, cursor, 28, 1, 0, s->theme.border);
      cursor += 11;
    }
    float x = rail ? 12 : cursor, y = rail ? cursor : 4,
          w = rail ? 46
                   : measure(s, r->title, 13, 600) + 52 + (r->unread ? 26 : 0),
          h = rail ? 46 : 34;
    if (r->id == s->selected || s->hover_region == i + 1)
      rect(s, x, y, w, h, rail ? 15 : 10,
           rail ? s->theme.soft : s->theme.surface);
    if (rail && r->symbol != CUI_SYMBOL_NONE)
      rect(s, x, y, w, h, 15, r->avatar_color);
    if (r->symbol != CUI_SYMBOL_NONE)
      icon(s, r->symbol, x + (rail ? 14 : 11), y + (rail ? 14 : 8), 18,
           s->theme.muted);
    else if (rail)
      avatar(s, x, y, 46, r->title, r->avatar_color,
             r->flags | CUI_CHAT_SQUARE);
    else
      avatar(s, x + 6, y + 5, 24, r->title, r->avatar_color, CUI_CHAT_SQUARE);
    if (!rail)
      text(s, x + 38, y + 8, w - 44, 13, 600,
           r->id == s->selected ? s->theme.foreground : s->theme.muted,
           r->title);
    if (r->unread) {
      char value[32];
      snprintf(value, sizeof(value), "%u", r->unread);
      float bx = rail ? x + w - 14 : x + w - 23, by = rail ? y + h - 12 : y + 8;
      rect(s, bx, by, 18, 18, 9, rail ? s->theme.danger : s->theme.foreground);
      text(s, bx + 4, by + 2, 18, 10.5, 700, s->theme.surface, value);
    }
    action(s, (unsigned)i + 1, x, y, w, h, r->title,
           (cui_chat_event){.action = CUI_CHAT_OPEN_ROOM, .id = r->id}, 1);
    cursor += (rail ? h : w) + (rail ? 10 : 4);
  }
}
static void header(chat_state *s) {
  if (!s->count)
    return;
  const cui_chat_room *r = s->rooms;
  int tiles = s->presentation.header == CUI_CHAT_COMPACT,
      day = s->presentation.header == CUI_CHAT_SOFT;
  float size = tiles ? 28 : day ? 38 : 32;
  float available = fmaxf(0, s->width - 14 - s->command_count * (size + 4));
  float av = tiles ? 14
             : day ? 40
                   : 38,
        x = tiles ? 14 : 20, y = (s->height - av) / 2.f;
  if (tiles) {
    border(s, x, y, 14, 14, 4, 1.5, s->theme.foreground, r->avatar_color);
    float title_width = measure(s, r->title, 14, 700);
    text(s, x + av + 8, (s->height - 18) / 2, title_width + 1, 14, 700,
         s->theme.foreground, r->title);
    float detail_x = x + av + 16 + title_width;
    float trailing_width = *r->trailing ? measure(s, r->trailing, 10, 600) + 16 : 0;
    text(s, detail_x, (s->height - 16) / 2, fmaxf(0, available - detail_x - trailing_width - 8),
         12, 400, s->theme.muted, r->detail);
  } else {
    avatar(s, x, y, av, r->title, r->avatar_color, r->flags);
    if (*r->detail)
      text(s, x + av + 12, day ? y : y - 1, fmaxf(0, available - x - av - 24),
           day ? 18 : 16, 700, s->theme.foreground, r->title);
    else
      centered_text(s, x + av + 12, 0, fmaxf(0, available - x - av - 24),
                    (float)s->height, day ? 18 : 16, 700, s->theme.foreground, r->title);
    text(s, x + av + 12, y + (day ? 24 : 22), fmaxf(0, available - x - av - 24),
         12.5, 400, s->theme.muted, r->detail);
  }
  if (tiles && *r->trailing) {
    float tw = measure(s, r->trailing, 10, 600);
    text(s, available - tw - 8, (s->height - 14) / 2, tw, 10, 600,
         s->theme.muted, r->trailing);
  } else if (*r->trailing) {
    float bx = x + av + 20 +
               measure(s, r->title,
                       tiles ? 14
                       : day ? 18
                             : 16,
                       700);
    float badge_icon = r->symbol != CUI_SYMBOL_NONE ? 14 : 0;
    float bw = measure(s, r->trailing, 11, 500) + 16 + badge_icon;
    if (bx + bw < available) {
      rect(s, bx, tiles ? 5 : y, bw, 21, 10, s->theme.soft);
      if (badge_icon)
        icon(s, r->symbol, bx + 6, (tiles ? 5 : y) + 4, 12, s->theme.muted);
      text(s, bx + 8 + badge_icon, (tiles ? 5 : y) + 3, bw - 16 - badge_icon,
           11, 500, s->theme.muted, r->trailing);
    }
  }
  for (size_t i = 0; i < s->command_count; ++i) {
    const cui_chat_command *c = s->commands + i;
    float bx = available + i * (size + 4);
    int selected = (c->flags & CUI_CHAT_MINE) != 0;
    if (!selected && s->hover_region == i + 1)
      rect(s, bx, (s->height - size) / 2, size, size, 12, s->theme.hover);
    if (selected)
      rect(s, bx, (s->height - size) / 2, size, size, 12, s->theme.foreground);
    if (*c->text)
      text(s, bx + 8, (s->height - 18) / 2, size - 8, 14, 500,
           selected ? s->theme.surface : s->theme.muted, c->text);
    else
      icon(s, c->symbol, bx + (size - 18) / 2, (s->height - 18) / 2, 18,
           selected ? s->theme.surface : s->theme.muted);
    action(s, (unsigned)i + 1, bx, (s->height - size) / 2, size, size, c->label,
           (cui_chat_event){.action = c->action,
                            .id = r->id,
                            .detail_id = c->id,
                            .index = (unsigned)i,
                            .text = c->label},
           !(c->flags & CUI_CHAT_DISABLED));
  }
  rect(s, 0, s->height - 1, s->width, 1, 0, s->theme.border);
}
static void inspector(chat_state *s) {
  if (!s->count)
    return;
  const cui_chat_room *hero = s->rooms;
  float w = s->width;
  for (size_t i = 0; i < s->command_count; ++i) {
    const cui_chat_command *c = s->commands + i;
    float width = (w - 28) / s->command_count;
    float x = 14 + i * width, tw = measure(s, c->label, 13, 650);
    if (s->selected == c->id || s->hover_region == i + 1)
      rect(s, x, 14, width - 3, 34, 11, s->theme.soft);
    text(s, x + fmaxf(0, (width - tw) / 2), 23, width - 3, 13, 650,
         s->selected == c->id ? s->theme.foreground : s->theme.muted, c->label);
    action(s, (unsigned)i + 1, x, 14, width - 3, 34, c->label,
           (cui_chat_event){.action = c->action, .id = c->id},
           !(c->flags & CUI_CHAT_DISABLED));
  }
  if (s->presentation.inspector == CUI_CHAT_MEDIA_GRID) {
    unsigned columns = s->presentation.media_columns;
    float cell = (w - 28 - (columns - 1) * 6) / columns;
    float start_y = 62;
    if (*s->status) {
      cui_chat_span caption = {s->status, "", CUI_CHAT_BODY};
      cui_chat_message message = {.spans = &caption, .span_count = 1};
      double old = s->theme.font_size;
      s->theme.font_size = 12.5;
      start_y += paragraph(s, &message, 14, start_y, w - 28, s->theme.muted, 1).height + 10;
      s->theme.font_size = old;
    }
    for (size_t i = 1; i < s->count; ++i) {
      const cui_chat_room *r = s->rooms + i;
      float x = 14 + ((i - 1) % columns) * (cell + 6);
      float y = start_y + ((i - 1) / columns) * (cell + 6);
      if (s->hover_region == i + 32)
        rect(s, x - 3, y - 3, cell + 6, cell + 6, 15, s->theme.foreground);
      rect(s, x, y, cell, cell, 14, r->avatar_color);
      text(s, x + 6, y + cell - 20, cell - 12, 10, 400, s->theme.foreground,
           r->title);
      action(s, (unsigned)i + 32, x, y, cell, cell, r->title,
             (cui_chat_event){.action = CUI_CHAT_MORE, .id = r->id},
             !(r->flags & CUI_CHAT_DISABLED));
    }
    return;
  }
  if (s->presentation.inspector == CUI_CHAT_PEOPLE_LIST) {
    for (size_t i = 1; i < s->count; ++i) {
      const cui_chat_room *r = s->rooms + i;
      float y = 62 + (i - 1) * 56;
      if (s->hover_region == i + 10)
        rect(s, 14, y, w - 28, 54, 12, s->theme.soft);
      if (r->symbol != CUI_SYMBOL_NONE) {
        text(s, 26, y + 15, w - 80, 14, 600, s->theme.foreground, r->title);
        icon(s, r->symbol, w - 44, y + 16, 16, s->theme.muted);
        action(s, (unsigned)i + 10, 14, y, w - 28, 54, r->title,
               (cui_chat_event){.action=CUI_CHAT_MORE, .id=r->id}, !(r->flags&CUI_CHAT_DISABLED));
        continue;
      }
      avatar(s, 22, y + 7, 36, r->title, r->avatar_color, r->flags);
      text(s, 68, y + 8, w - 90, 13.5, 650, s->theme.foreground, r->title);
      text(s, 68, y + 28, w - 90, 11, 400, s->theme.muted, r->detail);
      if (*r->trailing) {
        float tw = measure(s, r->trailing, 10.5, 700);
        rect(s, w - tw - 36, y + 16, tw + 16, 22, 8, s->theme.soft);
        text(s, w - tw - 28, y + 19, tw + 1, 10.5, 700, s->theme.muted,
             r->trailing);
      }
      action(s, (unsigned)i + 10, 14, y, w - 28, 54, r->title,
             (cui_chat_event){.action = CUI_CHAT_MORE, .id = r->id}, 1);
    }
    if (s->count == 1)
      text(s, 26, 76, w - 52, 13, 400, s->theme.muted, s->status);
    return;
  }
  avatar(s, (w - 76) / 2, 70, 76, hero->title, hero->avatar_color,
         CUI_CHAT_SQUARE);
  float tw = measure(s, hero->title, 18, 800);
  text(s, (w - tw) / 2, 158, w - 28, 18, 800, s->theme.foreground, hero->title);
  cui_chat_span span = {hero->detail, "", CUI_CHAT_BODY};
  cui_chat_message topic = {.spans = &span, .span_count = 1};
  double old = s->theme.font_size;
  s->theme.font_size = 13;
  paragraph_size d = paragraph(s, &topic, 0, 0, w - 40, s->theme.muted, 0);
  size_t first = s->scene.count;
  paragraph(s, &topic, 20, 188, w - 40, s->theme.muted, 1);
  /* Center each wrapped line, rather than just the widest paragraph line. */
  for (size_t i = first; i < s->scene.count;) {
    size_t end = i + 1;
    while (end < s->scene.count && s->scene.commands[end].p[1] == s->scene.commands[i].p[1]) ++end;
    cui_draw_command *last = s->scene.commands + end - 1;
    float right = last->p[0] + measure(s, last->text, last->p[3], (int)last->p[4]);
    float shift = (w - (right - 20)) / 2 - 20;
    for (; i < end; ++i) s->scene.commands[i].p[0] += shift;
  }
  s->theme.font_size = old;
  float y = 188 + d.height + 18;
  for (size_t i = 1; i < s->count && i <= 2; ++i) {
    const cui_chat_room *r = s->rooms + i;
    float x = 14 + (i - 1) * (w - 20) / 2, fw = (w - 36) / 2;
    rect(s, x, y, fw, 70, 14, s->theme.soft);
    text(s, x + 12, y + 12, fw - 24, 18, 800, s->theme.foreground, r->detail);
    text(s, x + 12, y + 40, fw - 24, 12, 400, s->theme.muted, r->title);
  }
  y += 84;
  for (size_t i = 3; i < s->count; ++i, y += 44) {
    const cui_chat_room *r = s->rooms + i;
    if (s->hover_region == i + 10)
      rect(s, 14, y, w - 28, 42, 12, s->theme.soft);
    text(s, 26, y + 12, w - 60, 13, 600, s->theme.foreground, r->title);
    float dw = measure(s, r->detail, 12, 400);
    text(s, w - 26 - dw, y + 13, dw + 1, 12, 400, s->theme.muted, r->detail);
    action(s, (unsigned)i + 10, 14, y, w - 28, 42, r->title,
           (cui_chat_event){.action = CUI_CHAT_MORE, .id = r->id},
           !(r->flags & CUI_CHAT_DISABLED));
  }
}
static void element(chat_state *s) {
  if (!s->count)
    return;
  float width = fmaxf(1, s->width - 24);
  if (s->kind == CUI_CHAT_AVATAR) {
    const cui_chat_room *r = s->rooms;
    float size = fminf(76, fminf(s->width, s->height) - 24);
    avatar(s, 12, 12, size, r->title, r->avatar_color, r->flags);
    return;
  }
  const cui_chat_message *m = s->messages;
  switch (s->kind) {
  case CUI_CHAT_MESSAGE:
    message(s, m, 0, 12);
    break;
  case CUI_CHAT_ATTACHMENT_CARD:
    if (m->attachment_count)
      attachment(s, m, 0, 12, 12, width, 256);
    break;
  case CUI_CHAT_REACTION_STRIP:
    reactions(s, m, 12, 8, 256);
    break;
  case CUI_CHAT_POLL_CARD:
    if (s->presentation.messages == CUI_CHAT_SOFT) {
      width = fminf(width, 320);
      rect(s, 12, 12, width, 68 + m->option_count * 43, 18, s->theme.soft);
      poll(s, m, 26, 21, fmaxf(1, width - 28), 256);
    } else
      poll(s, m, 12, 12, width, 256);
    break;
  case CUI_CHAT_REPLY_PREVIEW:
    reply_preview(s, m, 16, 12, width - 8, s->theme.foreground);
    break;
  case CUI_CHAT_THREAD_SUMMARY:
    thread_summary(s, m, 12, 8, width, 256);
    break;
  default:
    break;
  }
}
int cui__chat_paint(chat_state *s) {
  chat_scene old = s->scene;
  memset(&s->scene, 0, sizeof(s->scene));
  cui_draw_command *clear = command(&s->scene, CUI_DRAW_CLEAR);
  clear->color = s->kind == CUI_CHAT_TIMELINE &&
                         s->presentation.messages == CUI_CHAT_STANDARD
                     ? s->theme.background
                     : s->theme.surface;
  if (s->presentation.surface_radius > 0 &&
      (s->kind == CUI_CHAT_HEADER || s->kind == CUI_CHAT_INSPECTOR ||
       s->kind == CUI_CHAT_ROOMS)) {
    clear->color = s->theme.background;
    rect(s, 0, s->kind == CUI_CHAT_ROOMS ? -20 : 0, s->width,
         s->height + (s->kind == CUI_CHAT_INSPECTOR ? 0 : 20),
         s->presentation.surface_radius, s->theme.surface);
  }
  if (s->kind == CUI_CHAT_SPACES && s->presentation.spaces == CUI_HORIZONTAL) {
    clear->color = s->theme.background;
    rect(s, 0, 0, s->width, s->height, 14, s->theme.soft);
  }
  if (s->kind >= CUI_CHAT_MESSAGE)
    element(s);
  else if (s->kind == CUI_CHAT_ROOMS)
    rooms(s);
  else if (s->kind == CUI_CHAT_SPACES)
    spaces(s);
  else if (s->kind == CUI_CHAT_HEADER)
    header(s);
  else if (s->kind == CUI_CHAT_INSPECTOR)
    inspector(s);
  else {
    size_t start = 0;
    while (start < s->count && s->tops[start] + s->heights[start] < s->offset)
      ++start;
    for (size_t i = start; i < s->count && s->tops[i] < s->offset + s->height;
         ++i)
      message(s, s->messages + i, i, s->tops[i] - (float)s->offset);
    if (*s->status) {
      float status_y = s->count ? s->height - 20 : 24;
      if (s->count)
        rect(s, 0, s->height - 24, s->width, 24, 0,
             s->presentation.messages == CUI_CHAT_STANDARD ? s->theme.background
                                                           : s->theme.surface);
      text(s, 20, status_y, s->width - 40, 12, 400, s->theme.muted, s->status);
    }
  }
  double scale = fmin(s->scale, 4096. / fmax(s->width, s->height));
  cui_surface *surface = cui_surface_create(s->width, s->height, scale);
  int ok = !s->scene.failed && surface &&
           cui_surface_render(surface, s->scene.commands, s->scene.count);
  if (ok)
    ok = cui_canvas_set_regions(s->parts[0], s->scene.regions,
                                s->scene.region_count) &&
         cui_canvas_set_surface(s->parts[0], surface);
  cui_surface_release(surface);
  if (ok)
    cui__chat_scene_free(&old);
  else {
    cui__chat_scene_free(&s->scene);
    s->scene = old;
  }
  return ok;
}
