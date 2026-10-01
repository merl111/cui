#include "cui_chat_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static unsigned channel(double v) {
  v = v <= 0.0031308 ? 12.92 * v : 1.055 * pow(v, 1. / 2.4) - .055;
  return (unsigned)lround(fmax(0., fmin(1., v)) * 255.);
}
unsigned cui_chat_color(double l, double c, double hue, double alpha) {
  if (!isfinite(l) || !isfinite(c) || !isfinite(hue) || !isfinite(alpha) ||
      l < 0 || l > 1 || c < 0 || c > .5 || alpha < 0 || alpha > 1)
    return 0;
  double a = c * cos(hue * 3.141592653589793 / 180.),
         b = c * sin(hue * 3.141592653589793 / 180.);
  double x = l + .3963377774 * a + .2158037573 * b,
         y = l - .1055613458 * a - .0638541728 * b,
         z = l - .0894841775 * a - 1.291485548 * b;
  x = x * x * x;
  y = y * y * y;
  z = z * z * z;
  return channel(4.0767416621 * x - 3.3077115913 * y + .2309699292 * z) << 24 |
         channel(-1.2684380046 * x + 2.6097574011 * y - .3413193965 * z) << 16 |
         channel(-.0041960863 * x - .7034186147 * y + 1.707614701 * z) << 8 |
         (unsigned)lround(alpha * 255.);
}
int cui_chat_theme_get(cui_chat_appearance appearance, cui_chat_theme *t) {
  if (!t || appearance < 0 || appearance > CUI_CHAT_TILES)
    return 0;
  static const double tokens[3][12][3] = {{{.17, .02, 265},
                                           {.21, .022, 265},
                                           {.96, .008, 265},
                                           {.72, .02, 265},
                                           {.29, .022, 265},
                                           {.84, .17, 165},
                                           {.20, .05, 165},
                                           {.25, .024, 265},
                                           {.28, .024, 265},
                                           {.135, .018, 265},
                                           {.64, .21, 22},
                                           {.72, .17, 155}},
                                          {{.94, .035, 235},
                                           {.995, .003, 235},
                                           {.22, .03, 260},
                                           {.50, .03, 255},
                                           {.90, .015, 240},
                                           {.56, .21, 22},
                                           {.99, .01, 22},
                                           {.965, .012, 240},
                                           {.935, .018, 240},
                                           {.94, .035, 235},
                                           {.56, .21, 22},
                                           {.62, .15, 155}},
                                          {{.955, .022, 85},
                                           {.99, .008, 85},
                                           {.20, .02, 280},
                                           {.48, .02, 280},
                                           {.87, .02, 85},
                                           {.54, .25, 292},
                                           {.99, .01, 292},
                                           {.97, .015, 85},
                                           {.92, .025, 85},
                                           {.955, .022, 85},
                                           {.56, .21, 22},
                                           {.62, .15, 155}}};
  unsigned colors[12];
  for (int i = 0; i < 12; ++i)
    colors[i] =
        cui_chat_color(tokens[appearance][i][0], tokens[appearance][i][1],
                       tokens[appearance][i][2], 1.);
  *t = (cui_chat_theme){appearance, colors[0],
                        colors[1],  colors[2],
                        colors[3],  colors[4],
                        colors[5],  colors[6],
                        colors[7],  colors[8],
                        colors[9],  colors[10],
                        colors[11], appearance == CUI_CHAT_TILES ? 13.5 : 14.};
  return 1;
}
int cui_chat_presentation_preset(cui_chat_appearance preset,
                                 cui_chat_presentation *p) {
  cui_chat_theme t;
  if (!p || !cui_chat_theme_get(preset, &t))
    return 0;
  int soft = preset == CUI_CHAT_DAYLIGHT, compact = preset == CUI_CHAT_TILES;
  cui_chat_layout_style layout = compact ? CUI_CHAT_COMPACT
                                 : soft  ? CUI_CHAT_SOFT
                                         : CUI_CHAT_STANDARD;
  *p = (cui_chat_presentation){
      .messages = layout,
      .rooms = layout,
      .header = layout,
      .composer = layout,
      .spaces = preset == CUI_CHAT_NEBULA ? CUI_VERTICAL : CUI_HORIZONTAL,
      .inspector = CUI_CHAT_PROFILE,
      .show_sender = 1,
      .show_room_previews = 1,
      .room_height = compact ? 34
                     : soft  ? 58
                             : 48,
      .bubble_radius = 18,
      .surface_radius = soft ? 20 : 0,
      .avatar_border_width = compact ? 1.5 : 0,
      .composer_padding = compact ? 10 : 16,
      .composer_radius = compact ? 9
                         : soft  ? 18
                                 : 14,
      .mention_background = compact ? t.accent : soft ? cui_chat_color(.88, .12, 90, 1)
                                 : (t.accent & 0xffffff00u) | 40,
      .mention_foreground = compact ? t.on_accent : soft ? cui_chat_color(.25, .05, 80, 1) : t.foreground,
      .attachment_background = cui_chat_color(soft ? .86 : .87, soft ? .1 : .08,
                                              soft ? 200 : 235, 1),
      .media_columns = 3,
      .composer_tools =
          compact ? 0 : (1u << 5) | (1u << 6) | (soft ? 0 : 1u << 7)};
  return 1;
}
static void commands_free(cui_chat_command *p, size_t n) {
  if (!p)
    return;
  for (size_t i = 0; i < n; ++i) {
    free((char *)p[i].label);
    free((char *)p[i].text);
  }
  free(p);
}
static char *copy(const char *p, size_t limit) {
  if (!p)
    p = "";
  size_t n = strlen(p);
  if (n > limit)
    return NULL;
  char *q = malloc(n + 1);
  if (q)
    memcpy(q, p, n + 1);
  return q;
}
void cui__chat_details_free(cui_chat_detail *p, size_t n) {
  if (p) {
    for (size_t i = 0; i < n; ++i) {
      free((char *)p[i].text);
      free((char *)p[i].detail);
    }
    free(p);
  }
}
static cui_chat_detail *details(const cui_chat_detail *p, size_t n,
                                size_t limit) {
  if (n > limit || (n && !p))
    return NULL;
  cui_chat_detail *q = calloc(n ? n : 1, sizeof(*q));
  if (!q)
    return NULL;
  for (size_t i = 0; i < n; ++i) {
    q[i] = p[i];
    q[i].text = copy(p[i].text, 4096);
    q[i].detail = copy(p[i].detail, 4096);
    if (!q[i].text || !q[i].detail) {
      cui__chat_details_free(q, n);
      return NULL;
    }
  }
  return q;
}
void cui__chat_messages_free(cui_chat_message *p, size_t n) {
  if (!p)
    return;
  for (size_t i = 0; i < n; ++i) {
    free((char *)p[i].author);
    free((char *)p[i].time);
    free((char *)p[i].date);
    free((char *)p[i].reply_author);
    free((char *)p[i].reply_text);
    free((char *)p[i].poll_question);
    free((char *)p[i].thread_preview);
    cui__chat_rooms_free((cui_chat_room *)p[i].thread_participants,
                         p[i].thread_participant_count);
    cui_chat_span *sp = (cui_chat_span *)p[i].spans;
    if (sp) {
      for (size_t j = 0; j < p[i].span_count; ++j) {
        free((char *)sp[j].text);
        free((char *)sp[j].link);
      }
      free(sp);
    }
    cui__chat_details_free((cui_chat_detail *)p[i].attachments,
                           p[i].attachment_count);
    cui__chat_details_free((cui_chat_detail *)p[i].reactions,
                           p[i].reaction_count);
    cui__chat_details_free((cui_chat_detail *)p[i].options, p[i].option_count);
  }
  free(p);
}
void cui__chat_rooms_free(cui_chat_room *p, size_t n) {
  if (p) {
    for (size_t i = 0; i < n; ++i) {
      free((char *)p[i].group);
      free((char *)p[i].title);
      free((char *)p[i].detail);
      free((char *)p[i].trailing);
    }
    free(p);
  }
}
static int room_copy(cui_chat_room *next, const cui_chat_room *item) {
  if (item->symbol < CUI_SYMBOL_NONE || item->symbol >= CUI_SYMBOL_COUNT)
    return 0;
  *next = *item;
  next->group = copy(item->group, 4096);
  next->title = copy(item->title, 4096);
  next->detail = copy(item->detail, 4096);
  next->trailing = copy(item->trailing, 4096);
  return next->id && next->group && next->title && next->detail &&
         next->trailing;
}
static int message_copy(cui_chat_message *q, const cui_chat_message *p) {
  if (!p->id || p->thread_participant_count > 8 ||
      (p->thread_participant_count && !p->thread_participants) ||
      p->span_count > 128 || (p->span_count && !p->spans) ||
      p->selected_option < -1 ||
      (p->option_count && p->selected_option >= (int)p->option_count))
    return 0;
  q->id = p->id;
  q->avatar_color = p->avatar_color;
  q->author_color = p->author_color;
  q->flags = p->flags;
  q->reply_id = p->reply_id;
  q->selected_option = p->selected_option;
  q->thread_count = p->thread_count;
  q->thread_participant_count = p->thread_participant_count;
  cui_chat_room *participants =
      calloc(p->thread_participant_count ? p->thread_participant_count : 1,
             sizeof(*participants));
  q->thread_participants = participants;
  if (!participants)
    return 0;
  for (size_t i = 0; i < p->thread_participant_count; ++i)
    if (!room_copy(participants + i, p->thread_participants + i))
      return 0;
  q->span_count = p->span_count;
  q->attachment_count = p->attachment_count;
  q->reaction_count = p->reaction_count;
  q->option_count = p->option_count;
  q->author = copy(p->author, 4096);
  q->time = copy(p->time, 4096);
  q->date = copy(p->date, 4096);
  q->reply_author = copy(p->reply_author, 4096);
  q->reply_text = copy(p->reply_text, 4096);
  q->poll_question = copy(p->poll_question, 4096);
  q->thread_preview = copy(p->thread_preview, 4096);
  if (!q->author || !q->time || !q->date || !q->reply_author ||
      !q->reply_text || !q->poll_question || !q->thread_preview)
    return 0;
  q->attachments = details(p->attachments, p->attachment_count, 16);
  q->reactions = details(p->reactions, p->reaction_count, 32);
  q->options = details(p->options, p->option_count, 16);
  if (!q->attachments || !q->reactions || !q->options)
    return 0;
  cui_chat_span *sp = calloc(p->span_count ? p->span_count : 1, sizeof(*sp));
  q->spans = sp;
  if (!sp)
    return 0;
  size_t total = 0;
  for (size_t i = 0; i < p->span_count; ++i) {
    sp[i].text = copy(p->spans[i].text, 65536);
    sp[i].link = copy(p->spans[i].link, 4096);
    sp[i].style = p->spans[i].style;
    if (!sp[i].text || !sp[i].link || sp[i].style < 0 ||
        sp[i].style > CUI_CHAT_CODE)
      return 0;
    total += strlen(sp[i].text);
    if (total > 65536)
      return 0;
  }
  return 1;
}
static int compare_ids(const void *a, const void *b) {
  cui_item_id x = *(const cui_item_id *)a, y = *(const cui_item_id *)b;
  return (x > y) - (x < y);
}
static int unique_ids(cui_item_id *ids, size_t n) {
  qsort(ids, n, sizeof(*ids), compare_ids);
  for (size_t i = 0; i < n; ++i)
    if (!ids[i] || (i && ids[i] == ids[i - 1]))
      return 0;
  return 1;
}
static void dispose(void *p) {
  chat_state *s = p;
  cui__chat_messages_free(s->messages, s->count);
  cui__chat_rooms_free(s->rooms, s->count);
  cui__chat_details_free(s->files, s->file_count);
  commands_free(s->commands, s->command_count);
  cui__chat_scene_free(&s->scene);
  if (s->metrics) {
    for (size_t i = 0; i < 8192; ++i)
      free(s->metrics[i].text);
    free(s->metrics);
  }
  free(s->tops);
  free(s->heights);
  free(s->query);
  free(s->status);
  free(s->event_text);
  for (int i = 0; i < CUI_SYMBOL_COUNT; ++i)
    cui_icon_release(s->icons[i]);
  free(s);
}
chat_state *cui__chat(const cui_widget *w) {
  return w && w->destroy_payload == dispose ? w->payload : NULL;
}
void cui__chat_emit(chat_state *s, cui_chat_event e) {
  char *text = copy(e.text, 65536);
  if (!text)
    return;
  free(s->event_text);
  s->event_text = text;
  e.text = text;
  s->event = e;
  cui__emit(s->root);
}
static void canvas_event(cui_widget *w, const cui_canvas_event *e, void *p) {
  (void)w;
  chat_state *s = p;
  if (e->kind == CUI_CANVAS_SCROLL) {
#ifdef __APPLE__
    double delta = -e->dy;
#else
    double delta = e->dy * 36.;
#endif
    cui_chat_scroll(s->root, s->offset + delta);
    return;
  }
  if (e->kind == CUI_CANVAS_MOVE) {
    cui_item_id hover = 0;
    if (s->kind == CUI_CHAT_TIMELINE && e->x >= 0 && e->y >= 0 &&
        e->x < s->width && e->y < s->height)
      for (size_t i = 0; i < s->count; ++i)
        if (e->y + s->offset >= s->tops[i] &&
            e->y + s->offset < s->tops[i] + s->heights[i]) {
          hover = s->messages[i].id;
          break;
        }
    if (hover != s->hovered || e->id != s->hover_region) {
      s->hovered = hover;
      s->hover_region = e->id;
      s->dirty = 1;
    }
    return;
  }
  if (e->kind == CUI_CANVAS_FOCUS) {
    s->focus_region = e->id;
    s->dirty = 1;
    return;
  }
  if (e->kind == CUI_CANVAS_PRESS) {
    cui__chat_emit(s, (cui_chat_event){.action = CUI_CHAT_FOCUS});
    return;
  }
  if (e->kind != CUI_CANVAS_ACTIVATE)
    return;
  for (size_t i = 0; i < s->scene.region_count; ++i)
    if (s->scene.regions[i].id == e->id) {
      cui_chat_event action = s->scene.actions[i];
      action.modifiers = e->modifiers;
      cui__chat_emit(s, action);
      return;
    }
}
static int key(cui_widget *w, cui_key k, unsigned mods, void *p) {
  (void)w;
  (void)mods;
  chat_state *s = p;
  double offset = s->offset;
  switch (k) {
  case CUI_KEY_PAGE_UP:
    offset -= s->height * .85;
    break;
  case CUI_KEY_PAGE_DOWN:
    offset += s->height * .85;
    break;
  case CUI_KEY_HOME:
    offset = 0;
    break;
  case CUI_KEY_END:
    offset = -1;
    break;
  case CUI_KEY_UP:
    offset -= 36;
    break;
  case CUI_KEY_DOWN:
    offset += 36;
    break;
  default:
    return 0;
  }
  if (offset < 0 && k != CUI_KEY_END)
    offset = 0;
  cui_chat_scroll(s->root, offset);
  return 1;
}
int cui__chat_message_kind(cui_chat_kind kind) {
  return kind == CUI_CHAT_TIMELINE ||
         (kind >= CUI_CHAT_MESSAGE && kind <= CUI_CHAT_THREAD_SUMMARY);
}
static int room_kind(cui_chat_kind kind) {
  return kind == CUI_CHAT_ROOMS || kind == CUI_CHAT_HEADER ||
         kind == CUI_CHAT_SPACES || kind == CUI_CHAT_INSPECTOR ||
         kind == CUI_CHAT_AVATAR;
}
static int commands_default(chat_state *s);
cui_widget *cui_chat_create(cui_widget *parent, cui_chat_kind kind,
                            cui_chat_appearance appearance) {
  cui_chat_theme theme;
  if (kind < 0 || kind > CUI_CHAT_AVATAR ||
      !cui_chat_theme_get(appearance, &theme))
    return NULL;
  chat_state *s = calloc(1, sizeof(*s));
  if (!s)
    return NULL;
  s->query = copy("", 0);
  s->status = copy("", 0);
  s->event_text = copy("", 0);
  if (!s->query || !s->status || !s->event_text) {
    dispose(s);
    return NULL;
  }
  s->root = cui_box(parent, CUI_VERTICAL, 0);
  if (!s->root) {
    dispose(s);
    return NULL;
  }
  s->root->payload = s;
  s->root->destroy_payload = dispose;
  s->kind = kind;
  s->theme = theme;
  cui_chat_presentation_preset(appearance, &s->presentation);
  s->dirty = 1;
  s->scale = 1.;
  cui_box_set_padding(s->root, 0);
  if (kind == CUI_CHAT_COMPOSER) {
    if (!cui__chat_composer(s))
      return NULL;
  } else if (kind == CUI_CHAT_WORKSPACE) {
    if (!cui__chat_workspace(s))
      return NULL;
  } else {
    s->parts[0] = cui_canvas(s->root);
    if (!s->parts[0])
      return NULL;
    cui_expand(s->root, 1);
    cui_expand(s->parts[0], 1);
    cui_set_min_size(s->parts[0], 80, 80);
    cui_canvas_on_event(s->parts[0], canvas_event, s);
    cui_on_key(s->parts[0], key, s);
  }
  if (!commands_default(s))
    return NULL;
  return s->root;
}
int cui_chat_set_theme(cui_widget *w, const cui_chat_theme *t) {
  chat_state *s = cui__chat(w);
  if (!s || !t || t->appearance < 0 || t->appearance > CUI_CHAT_TILES ||
      !isfinite(t->font_size) || t->font_size < 8 || t->font_size > 48)
    return 0;
  s->theme = *t;
  s->width = 0;
  s->dirty = 1;
  return 1;
}
int cui_chat_presentation_get(const cui_widget *w, cui_chat_presentation *p) {
  chat_state *s = cui__chat(w);
  if (!s || !p)
    return 0;
  *p = s->presentation;
  return 1;
}
int cui_chat_set_presentation(cui_widget *w, const cui_chat_presentation *p) {
  chat_state *s = cui__chat(w);
  if (!s || !p || p->messages < 0 || p->messages > CUI_CHAT_COMPACT ||
      p->rooms < 0 || p->rooms > CUI_CHAT_COMPACT || p->header < 0 ||
      p->header > CUI_CHAT_COMPACT || p->composer < 0 ||
      p->composer > CUI_CHAT_COMPACT ||
      (p->spaces != CUI_VERTICAL && p->spaces != CUI_HORIZONTAL) ||
      p->media_columns < 1 || p->media_columns > 8 || p->inspector < 0 ||
      p->inspector > CUI_CHAT_MEDIA_GRID ||
      (p->show_sender != 0 && p->show_sender != 1) ||
      (p->show_room_previews != 0 && p->show_room_previews != 1) ||
      (p->composer_tools & ~((1u << 5) | (1u << 6) | (1u << 7))))
    return 0;
  const double values[] = {p->room_height,      p->bubble_radius,
                           p->surface_radius,   p->avatar_border_width,
                           p->composer_padding, p->composer_radius};
  for (size_t i = 0; i < sizeof(values) / sizeof(*values); ++i)
    if (!isfinite(values[i]) || values[i] < 0 || values[i] > 256)
      return 0;
  if (p->room_height < 24)
    return 0;
  s->presentation = *p;
  s->width = 0;
  s->dirty = 1;
  return 1;
}
int cui_chat_set_commands(cui_widget *w, const cui_chat_command *items,
                          size_t n) {
  chat_state *s = cui__chat(w);
  if (!s ||
      (s->kind != CUI_CHAT_HEADER && s->kind != CUI_CHAT_INSPECTOR &&
       s->kind != CUI_CHAT_TIMELINE && s->kind != CUI_CHAT_MESSAGE) ||
      n > 16 || (n && !items))
    return 0;
  cui_chat_command *next = calloc(n ? n : 1, sizeof(*next));
  if (!next)
    return 0;
  for (size_t i = 0; i < n; ++i) {
    const cui_chat_command *v = items + i;
    int valid = v->id && v->label && *v->label &&
                v->symbol >= CUI_SYMBOL_NONE && v->symbol < CUI_SYMBOL_COUNT &&
                v->action > CUI_CHAT_NONE && v->action <= CUI_CHAT_LOAD_OLDER &&
                !(v->flags & ~(CUI_CHAT_MINE | CUI_CHAT_DISABLED));
    for (size_t j = 0; j < i; ++j)
      if (next[j].id == v->id)
        valid = 0;
    if (!valid) {
      commands_free(next, n);
      return 0;
    }
    next[i] = *v;
    next[i].label = copy(v->label, 4096);
    next[i].text = copy(v->text, 4096);
    if (!next[i].label || !next[i].text) {
      commands_free(next, n);
      return 0;
    }
  }
  commands_free(s->commands, s->command_count);
  s->commands = next;
  s->command_count = n;
  s->dirty = 1;
  return 1;
}
static int commands_default(chat_state *s) {
  int day = s->theme.appearance == CUI_CHAT_DAYLIGHT;
  int tiles = s->theme.appearance == CUI_CHAT_TILES;
  const cui_chat_command day_header[] = {
      {1, "Video call", "", CUI_SYMBOL_VIDEO, CUI_CHAT_MORE, 0},
      {2, "Search", "", CUI_SYMBOL_SEARCH, CUI_CHAT_MORE, 0},
      {3, "Room info", "", CUI_SYMBOL_PANEL, CUI_CHAT_MORE, CUI_CHAT_MINE}};
  const cui_chat_command row_header[] = {
      {1, "Voice call", "", CUI_SYMBOL_PHONE, CUI_CHAT_MORE, 0},
      {2, "Video call", "", CUI_SYMBOL_VIDEO, CUI_CHAT_MORE, 0},
      {3, "Thread", "", CUI_SYMBOL_THREAD, CUI_CHAT_THREAD, 0},
      {4, "Members", "", CUI_SYMBOL_PEOPLE, CUI_CHAT_MORE, 0},
      {5, "Room info", "", CUI_SYMBOL_INFO, CUI_CHAT_MORE, 0}};
  const cui_chat_command tabs[] = {
      {1, "About", "", CUI_SYMBOL_NONE, CUI_CHAT_OPEN_ROOM, 0},
      {2, "People", "", CUI_SYMBOL_NONE, CUI_CHAT_OPEN_ROOM, 0},
      {3, "Media", "", CUI_SYMBOL_NONE, CUI_CHAT_OPEN_ROOM, 0}};
  const cui_chat_command hover[] = {
      {1, "React", day ? "❤️" : "👍", CUI_SYMBOL_NONE, CUI_CHAT_REACT, 0},
      {2, "Reply", "", CUI_SYMBOL_REPLY, CUI_CHAT_REPLY, 0},
      {3, "Reply in thread", "", CUI_SYMBOL_THREAD, CUI_CHAT_THREAD, 0},
      {4, "More actions", "", CUI_SYMBOL_MORE, CUI_CHAT_MORE, 0}};
  if (s->kind == CUI_CHAT_HEADER)
    return cui_chat_set_commands(s->root, day ? day_header : row_header,
                                 day ? 3 : 5);
  if (s->kind == CUI_CHAT_INSPECTOR)
    return cui_chat_set_commands(s->root, tabs, 3);
  if (s->kind == CUI_CHAT_TIMELINE || s->kind == CUI_CHAT_MESSAGE) {
    if (!day && !tiles)
      return cui_chat_set_commands(s->root, hover, 4);
    cui_chat_command short_hover[] = {
        hover[0],
        {5, "React", day ? "😂" : "👀", CUI_SYMBOL_NONE, CUI_CHAT_REACT, 0},
        hover[1]};
    return cui_chat_set_commands(s->root, short_hover, 3);
  }
  return 1;
}
int cui_chat_set_messages(cui_widget *w, const cui_chat_message *items,
                          size_t n) {
  chat_state *s = cui__chat(w);
  if (!s || !cui__chat_message_kind(s->kind) ||
      (s->kind != CUI_CHAT_TIMELINE && n > 1) || n > 100000 || (n && !items))
    return 0;
  cui_chat_message *next = calloc(n ? n : 1, sizeof(*next));
  cui_item_id *ids = calloc(n ? n : 1, sizeof(*ids));
  float *tops = calloc(n ? n : 1, sizeof(*tops)),
        *heights = calloc(n ? n : 1, sizeof(*heights));
  if (!next || !ids || !tops || !heights) {
    free(next);
    free(ids);
    free(tops);
    free(heights);
    return 0;
  }
  int ok = 1;
  for (size_t i = 0; i < n && ok; ++i) {
    ids[i] = items[i].id;
    ok = message_copy(next + i, items + i);
  }
  ok = ok && unique_ids(ids, n);
  free(ids);
  if (!ok) {
    cui__chat_messages_free(next, n);
    free(tops);
    free(heights);
    return 0;
  }
  /* Keep the reading anchor by ID; follow incoming messages only at bottom. */
  cui_item_id anchor = 0;
  double delta = 0;
  int bottom = s->offset + s->height >= s->total - 2;
  for (size_t i = 0; i < s->count; ++i)
    if (s->tops[i] <= s->offset) {
      anchor = s->messages[i].id;
      delta = s->offset - s->tops[i];
    }
  cui__chat_messages_free(s->messages, s->count);
  free(s->tops);
  free(s->heights);
  s->messages = next;
  s->count = n;
  s->tops = tops;
  s->heights = heights;
  s->dirty = 1;
  s->hovered = 0;
  if (!cui__chat_layout(s))
    return 0;
  if (bottom)
    s->offset = fmax(0., s->total - s->height);
  else
    for (size_t i = 0; i < n; ++i)
      if (next[i].id == anchor) {
        s->offset = tops[i] + delta;
        break;
      }
  s->offset = fmax(0., fmin(s->offset, fmax(0., s->total - s->height)));
  return 1;
}
int cui_chat_set_rooms(cui_widget *w, const cui_chat_room *items, size_t n) {
  chat_state *s = cui__chat(w);
  if (!s || !room_kind(s->kind) || (s->kind == CUI_CHAT_AVATAR && n > 1) ||
      n > 100000 || (n && !items))
    return 0;
  cui_chat_room *next = calloc(n ? n : 1, sizeof(*next));
  cui_item_id *ids = calloc(n ? n : 1, sizeof(*ids));
  if (!next || !ids) {
    free(next);
    free(ids);
    return 0;
  }
  int ok = 1;
  for (size_t i = 0; i < n && ok; ++i) {
    ids[i] = items[i].id;
    ok = room_copy(next + i, items + i);
  }
  ok = ok && unique_ids(ids, n);
  free(ids);
  if (!ok) {
    cui__chat_rooms_free(next, n);
    return 0;
  }
  cui__chat_rooms_free(s->rooms, s->count);
  s->rooms = next;
  s->count = n;
  s->dirty = 1;
  return 1;
}
int cui_chat_select(cui_widget *w, cui_item_id id) {
  chat_state *s = cui__chat(w);
  if (!s || !room_kind(s->kind))
    return 0;
  if (id && s->kind == CUI_CHAT_INSPECTOR) {
    size_t i = 0;
    for (; i < s->command_count && s->commands[i].id != id; ++i) {
    }
    if (i == s->command_count || (s->commands[i].flags & CUI_CHAT_DISABLED))
      return 0;
  }
  if (id && s->kind != CUI_CHAT_INSPECTOR) {
    size_t i = 0;
    for (; i < s->count && s->rooms[i].id != id; ++i) {
    }
    if (i == s->count || (s->rooms[i].flags & CUI_CHAT_DISABLED))
      return 0;
  }
  s->selected = id;
  s->dirty = 1;
  return 1;
}
static int set_string(chat_state *s, char **field, const char *value) {
  char *q = copy(value, 4096);
  if (!q)
    return 0;
  free(*field);
  *field = q;
  s->dirty = 1;
  return 1;
}
int cui_chat_set_query(cui_widget *w, const char *q) {
  chat_state *s = cui__chat(w);
  if (!s || !room_kind(s->kind))
    return 0;
  int ok = set_string(s, &s->query, q);
  if (ok)
    s->offset = 0;
  return ok;
}
int cui_chat_set_status(cui_widget *w, const char *q) {
  chat_state *s = cui__chat(w);
  return s ? set_string(s, &s->status, q) : 0;
}
static int refresh_font(chat_state *s) {
  const char *family;
  double points;
  int weight;
  cui__font_resolve(s->root, &family, &points, &weight);
  if (!family) family = "";
  if (!strcmp(family, s->font_family)) return 0;
  memcpy(s->font_family, family, strlen(family) + 1);
  if (s->metrics) {
    for (size_t i = 0; i < 8192; ++i) free(s->metrics[i].text);
    memset(s->metrics, 0, 8192 * sizeof(*s->metrics));
  }
  s->dirty = 1;
  return 1;
}
int cui_chat_refresh(cui_widget *w, double scale) {
  chat_state *s = cui__chat(w);
  if (!s || !isfinite(scale) || scale < .25 || scale > 8)
    return 0;
  if (s->kind == CUI_CHAT_COMPOSER) {
    cui__chat_compose_update(s);
    return 1;
  }
  if (s->kind == CUI_CHAT_WORKSPACE)
    return 1;
  int width = 0, height = 0;
  if (!cui_widget_get_size(s->parts[0], &width, &height) || width < 1 ||
      height < 1)
    return 1;
  int font_changed = refresh_font(s);
  if (width != s->width || height != s->height || scale != s->scale || font_changed) {
    int bottom = s->offset + s->height >= s->total - 2;
    s->width = width;
    s->height = height;
    s->scale = scale;
    s->dirty = 1;
    if (!cui__chat_layout(s))
      return 0;
    if (bottom)
      s->offset = fmax(0., s->total - height);
  }
  if (!s->dirty)
    return 1;
  if (!cui__chat_paint(s))
    return 0;
  s->dirty = 0;
  return 1;
}
int cui_chat_event_get(const cui_widget *w, cui_chat_event *event) {
  chat_state *s = cui__chat(w);
  if (!s || !event)
    return 0;
  *event = s->event;
  return 1;
}
cui_widget *cui_chat_part(cui_widget *w, unsigned part) {
  chat_state *s = cui__chat(w);
  return s && part < CHAT_PARTS ? s->parts[part] : NULL;
}
int cui_chat_scroll(cui_widget *w, double offset) {
  chat_state *s = cui__chat(w);
  if (!s || s->kind > CUI_CHAT_TIMELINE || !isfinite(offset))
    return 0;
  double end = fmax(0., s->total - s->height);
  s->offset = offset < 0 ? end : fmax(0., fmin(offset, end));
  s->dirty = 1;
  return 1;
}
double cui_chat_scroll_offset(const cui_widget *w) {
  chat_state *s = cui__chat(w);
  return s ? s->offset : 0.;
}
/* Composer uses the same transactional detail copier. */
int cui_chat_compose_files(cui_widget *w, const cui_chat_detail *files,
                           size_t n) {
  chat_state *s = cui__chat(w);
  if (!s || s->kind != CUI_CHAT_COMPOSER)
    return 0;
  cui_chat_detail *next = details(files, n, 16);
  if (!next)
    return 0;
  cui__chat_details_free(s->files, s->file_count);
  s->files = next;
  s->file_count = n;
  cui__chat_compose_update(s);
  return 1;
}

int cui_chat_scroll_to(cui_widget *w, cui_item_id id) {
  chat_state *s = cui__chat(w);
  if (!s || s->kind != CUI_CHAT_TIMELINE)
    return 0;
  for (size_t i = 0; i < s->count; ++i)
    if (s->messages[i].id == id)
      return cui_chat_scroll(w, s->tops[i]);
  return 0;
}
unsigned cui_chat_action_region(const cui_widget *w, const cui_chat_event *a) {
  chat_state *s = cui__chat(w);
  if (!s || !a)
    return 0;
  for (size_t i = 0; i < s->scene.region_count; ++i) {
    const cui_chat_event *b = s->scene.actions + i;
    if (a->action == b->action && a->id == b->id &&
        a->detail_id == b->detail_id && a->index == b->index &&
        (!a->text || !strcmp(a->text, b->text ? b->text : "")))
      return s->scene.regions[i].id;
  }
  return 0;
}
