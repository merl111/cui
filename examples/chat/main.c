/* Native reconstructions of the HTML concepts using shared CUI components.
 * Local fixture interactions only; Archaic supplies the Matrix integration. */
#include "cui_chat.h"
#include "cui_desktop.h"
#include "cui_layouts.h"
#include "cui_search.h"
#include "fixture.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define ROOM_COUNT (sizeof(sample_rooms) / sizeof(*sample_rooms))
static cui_app *app;
static cui_window *window;
static cui_chat_appearance appearance;
static cui_chat_theme theme;
static cui_widget *navigation, *space_tabs, *workspace, *sidebar, *inspector,
    *thread_box, *thread_view, *thread_composer;
static cui_widget *headers[4], *timelines[4], *composers[4], *space_title,
    *room_filter, *workspace_title, *layout_buttons[4], *space_buttons[5],
    *filter_buttons[4];
static unsigned pane_room[4] = {0, 6, 1, 3}, focused, space = 1;
static int thread_open, inspector_open = 1, filter, nebula_panel;
static unsigned inspector_tab = 1;
static cui_chat_message messages[ROOM_COUNT][128];
static size_t counts[ROOM_COUNT];
static cui_chat_message replies[128];
static size_t reply_count = 3;
static char previews[ROOM_COUNT][4096];
static cui_chat_detail reaction_store[ROOM_COUNT][128][32];
static char reaction_keys[ROOM_COUNT][128][32][32];
static cui_widget *refreshes[32];
static size_t refresh_count;
static cui_widget *layers, *base_layer, *scrim_layer, *scrim_canvas,
    *verify_layer, *verify_button, *verify_accept, *palette_layer, *palette,
    *toast_layer, *toast_label, *call_layer, *call_clock, *call_title;
static cui_widget *backdrop;
static cui_command *escape_command;
static int modal_kind, session_verified, call_active;
static double toast_until, call_started;
static void notice(const char *text);
static void open_palette(void *data);
static void start_call(void);
static void open_room(unsigned pane, unsigned room);
static void room_list(void);
static void layout_click(cui_widget *w, void *data);
static void focus_pane(void *data);

static int message_preference = -1, room_preference = -1;
static cui_window *preferences_window;
static cui_widget *message_choice, *room_choice;
static void header_commands(unsigned pane) {
  cui_chat_command commands[] = {
      {1, "Voice call", "", CUI_SYMBOL_PHONE, CUI_CHAT_MORE, 0},
      {2, "Video call", "", CUI_SYMBOL_VIDEO, CUI_CHAT_MORE, 0},
      {3, "Thread", "", CUI_SYMBOL_THREAD, CUI_CHAT_THREAD,
       thread_open ? CUI_CHAT_MINE : 0},
      {4, "Members", "", CUI_SYMBOL_PEOPLE, CUI_CHAT_MORE,
       nebula_panel == 4 ? CUI_CHAT_MINE : 0},
      {5, "Room info", "", CUI_SYMBOL_INFO, CUI_CHAT_MORE,
       nebula_panel == 5 ? CUI_CHAT_MINE : 0}};
  if (appearance == CUI_CHAT_DAYLIGHT) {
    const cui_chat_command day[] = {
        {2, "Video call", "", CUI_SYMBOL_VIDEO, CUI_CHAT_MORE, 0},
        {6, "Search", "", CUI_SYMBOL_SEARCH, CUI_CHAT_MORE, 0},
        {5, "Room info", "", CUI_SYMBOL_PANEL, CUI_CHAT_MORE,
         inspector_open && !thread_open ? CUI_CHAT_MINE : 0}};
    cui_chat_set_commands(headers[pane], day, 3);
  } else if (appearance == CUI_CHAT_TILES) {
    const cui_chat_command tiles[] = {
        {2, "Call", "", CUI_SYMBOL_VIDEO, CUI_CHAT_MORE, 0},
        {7, "Maximize", "↗", CUI_SYMBOL_NONE, CUI_CHAT_MORE, 0},
        {8, "Close", "", CUI_SYMBOL_CLOSE, CUI_CHAT_MORE, 0}};
    cui_chat_set_commands(headers[pane], tiles, 3);
  } else
    cui_chat_set_commands(headers[pane], commands, 5);
}

static unsigned avatar_color(const char *name) {
  unsigned hue = 0;
  while (*name == '#' || *name == ' ')
    ++name;
  for (const unsigned char *p = (const unsigned char *)name; *p; ++p)
    hue += *p * 7;
  return cui_chat_color(appearance == CUI_CHAT_NEBULA ? .80 : .84,
                        appearance == CUI_CHAT_NEBULA ? .14 : .12, hue % 360,
                        1);
}
static unsigned sender_color(const char *name) {
  unsigned hue = 0;
  for (const unsigned char *p = (const unsigned char *)name; *p; ++p)
    hue += *p * 7;
  return appearance == CUI_CHAT_TILES ? theme.foreground
         : appearance == CUI_CHAT_NEBULA
             ? avatar_color(name)
             : cui_chat_color(.45, .14, hue % 360, 1);
}
static unsigned room_color(const sample_room *room) {
  if (appearance != CUI_CHAT_TILES) return avatar_color(room->room.title);
  if (!(room->room.flags & CUI_CHAT_SQUARE)) {
    unsigned hue=0;
    for (const unsigned char *p=(const unsigned char *)room->room.title;*p;++p) hue+=*p*7;
    return cui_chat_color(.82,.13,hue%360,1);
  }
  unsigned hue=!strcmp(room->space,"work")?40:!strcmp(room->space,"matrix")?165:
               !strcmp(room->space,"fosdem")?300:!strcmp(room->space,"games")?95:250;
  return cui_chat_color(.82,.13,hue,1);
}
#include "artwork.h"
static void refresh_backdrop(void) {
  if (!backdrop) return;
  int width=0,height=0;
  if(!cui_widget_get_size(backdrop,&width,&height) || width<1 || height<1) return;
  int panel=thread_open?340:inspector_open?300:0;
  double scale=cui_window_scale(window);
  static int last_width,last_height,last_panel=-1;
  static double last_scale;
  if(width==last_width && height==last_height && panel==last_panel && scale==last_scale) return;
  cui_draw_command scene[7]={{.op=CUI_DRAW_CLEAR,.color=theme.background}};
  float xs[]={12,324,width-12-panel}, widths[]={300,width-336-(panel?panel+12:0),panel};
  unsigned count=1,ink=cui_chat_color(.30,.05,250,.06);
  for(unsigned i=0;i<(panel?3u:2u);++i) for(unsigned shadow=0;shadow<2;++shadow)
    scene[count++]=(cui_draw_command){.op=CUI_DRAW_SHADOW,
      .p={xs[i],56+(shadow?8:1),widths[i],height-68,20,shadow?24:2},.color=ink};
  cui_surface *surface=cui_surface_create(width,height,scale);
  if(surface && cui_surface_render(surface,scene,count) && cui_canvas_set_surface(backdrop,surface)) {
    last_width=width;last_height=height;last_panel=panel;last_scale=scale;
  }
  cui_surface_release(surface);
}
static void style(cui_widget *w, unsigned bg, double radius, unsigned border,
                  double width, int padding) {
  cui_widget_style s = {bg, theme.foreground, border, radius, width, padding};
  cui_set_style(w, &s);
}
static cui_widget *chat(cui_widget *parent, cui_chat_kind kind) {
  cui_widget *w = cui_chat_create(parent, kind, appearance);
  assert(w);
  refreshes[refresh_count++] = w;
  if (kind == CUI_CHAT_HEADER) {
    cui_expand(w, 0);
    cui_set_min_size(cui_chat_part(w, 0), 1,
                     appearance == CUI_CHAT_TILES      ? 44
                     : appearance == CUI_CHAT_DAYLIGHT ? 72
                                                       : 62);
  }
  if (getenv("CUI_LARGE_TEXT")) {
    cui_chat_theme large = theme;
    large.font_size *= 1.5;
    cui_chat_set_theme(w, &large);
  }
  return w;
}
static cui_widget *label(cui_widget *parent, const char *value, double size,
                         int weight) {
  cui_widget *w = cui_label(parent, value);
  cui_set_font(w, "sans", size * .75, weight);
  return w;
}
static cui_widget *button(cui_widget *parent, const char *value,
                          cui_callback callback, void *data) {
  cui_widget *w = cui_button(parent, value);
  style(w, theme.soft, 12, theme.border, 0, 8);
  cui_on_action(w, callback, data);
  return w;
}
#include "overlays.h"
static void preference_changed(cui_widget *w, void *data) {
  (void)w;
  (void)data;
  message_preference = cui_get_selected(message_choice);
  room_preference = cui_get_selected(room_choice);
  for (unsigned i = 0; i < (appearance == CUI_CHAT_TILES ? 4u : 1u); ++i) {
    cui_chat_presentation p;
    cui_chat_presentation_get(timelines[i], &p);
    p.messages = (cui_chat_layout_style)message_preference;
    cui_chat_set_presentation(timelines[i], &p);
  }
  cui_chat_presentation p;
  cui_chat_presentation_get(navigation, &p);
  p.rooms = (cui_chat_layout_style)room_preference;
  p.room_height = room_preference == CUI_CHAT_COMPACT ? 34
                  : room_preference == CUI_CHAT_SOFT  ? 58
                                                      : 48;
  cui_chat_set_presentation(navigation, &p);
}
static void preferences(cui_widget *w, void *data) {
  (void)w;
  (void)data;
  if (!preferences_window) {
    preferences_window = cui_window_create(app, "Appearance", 360, 250);
    cui_widget *root = cui_window_root(preferences_window);
    label(root, "Message layout", 14, 650);
    const char *messages[] = {"Rows", "Bubbles", "Compact rows"};
    message_choice = cui_select(root, messages, 3);
    cui_set_selected(message_choice, message_preference < 0
                                         ? (int)appearance
                                         : message_preference);
    label(root, "Room list", 14, 650);
    const char *rooms[] = {"Standard", "Comfortable", "Compact"};
    room_choice = cui_select(root, rooms, 3);
    cui_set_selected(room_choice,
                     room_preference < 0 ? (int)appearance : room_preference);
    cui_on_action(message_choice, preference_changed, NULL);
    cui_on_action(room_choice, preference_changed, NULL);
  }
  cui_window_show(preferences_window);
}
static void room_list(void) {
  const char *keys[] = {"", "work", "matrix", "fosdem", "games"};
  cui_chat_room rows[ROOM_COUNT];
  size_t n = 0;
  for (unsigned group = 0; group < 3; ++group)
    for (size_t i = 0; i < ROOM_COUNT; ++i) {
      sample_room *r = sample_rooms + i;
      int dm = !(r->room.flags & CUI_CHAT_SQUARE),
          pinned = appearance == CUI_CHAT_DAYLIGHT && r->pinned;
      unsigned actual = pinned ? 0 : dm ? 2 : 1;
      if (actual != group)
        continue;
      if (space > 0 && *r->space && strcmp(r->space, keys[space]))
        continue;
      if (filter == 1 && !r->room.unread)
        continue;
      if (filter == 2 && !dm)
        continue;
      if (filter == 3 && !r->pinned)
        continue;
      rows[n] = r->room;
      rows[n].group =
          pinned ? "PINNED"
          : dm
              ? (appearance == CUI_CHAT_DAYLIGHT ? "PEOPLE" : "DIRECT MESSAGES")
              : "ROOMS";
      rows[n].avatar_color = room_color(r);
      if (appearance == CUI_CHAT_TILES) {
        static const char *slots[]={"T1","T2","T3","T4"};
        rows[n].trailing="";
        unsigned mask=workspace?cui_chat_workspace_mask(workspace):7;
        for(unsigned p=0;p<4;++p) if((mask&(1u<<p)) && pane_room[p]==i) {
          rows[n].flags|=CUI_CHAT_MINE;
          rows[n].trailing=slots[p];
        }
      }
      ++n;
    }
  cui_chat_set_rooms(navigation, rows, n);
  cui_chat_select(navigation, sample_rooms[pane_room[focused]].room.id);
  cui_set_text(space_title, sample_spaces[space].title);
}
static void update_inspector(void) {
  sample_room *r = sample_rooms + pane_room[focused];
  char members[32];
  if (r->members >= 1000)
    snprintf(members, sizeof(members), "%u,%03u", r->members / 1000,
             r->members % 1000);
  else
    snprintf(members, sizeof(members), "%u", r->members);
  cui_chat_room rows[] = {
      r->room,
      {.id = 20, .title = "Members", .detail = members},
      {.id = 21, .title = "Encryption", .detail = "E2EE"},
      {.id = 22, .title = "Notifications", .detail = "Mentions & keywords"},
      {.id = 23, .title = "Pinned messages", .detail = "2"},
      {.id = 24,
       .title = "Threads",
       .detail = pane_room[focused] == 0 ? "1 active" : "None"},
      {.id = 25, .title = "Room settings", .detail = ""},
      {.id = 26, .title = "Leave room", .detail = ""}};
  rows[0].detail = r->topic;
  rows[0].avatar_color = avatar_color(r->room.title);
  cui_chat_presentation p;
  cui_chat_presentation_get(inspector, &p);
  p.inspector = inspector_tab == 1   ? CUI_CHAT_PROFILE
                : inspector_tab == 2 ? CUI_CHAT_PEOPLE_LIST
                                     : CUI_CHAT_MEDIA_GRID;
  cui_chat_set_presentation(inspector, &p);
  cui_chat_set_status(inspector, inspector_tab == 3
      ? "Shared files in this room (sample swatches stand in for image thumbnails)." : "");
  if (inspector_tab == 1)
    cui_chat_set_rooms(inspector, rows, sizeof(rows) / sizeof(*rows));
  else if (inspector_tab == 2) {
    cui_chat_room people[8];
    people[0] = rows[0];
    for (unsigned i = 0; i < 6; ++i) {
      people[i + 1] = sample_people[i];
      people[i + 1].avatar_color = avatar_color(people[i + 1].title);
    }
    if (!(r->room.flags & CUI_CHAT_SQUARE)) {
      people[1] = r->room;
      people[1].detail = r->topic;
      people[1].trailing = "";
      people[1].avatar_color = avatar_color(r->room.title);
      people[2] = sample_people[5];
      people[2].avatar_color = avatar_color(people[2].title);
      people[3] = (cui_chat_room){.id=2000,.title="Invite people",.symbol=CUI_SYMBOL_PLUS};
      cui_chat_set_rooms(inspector, people, 4);
    } else {
      people[7] = (cui_chat_room){.id=2000,.title="Invite people",.symbol=CUI_SYMBOL_PLUS};
      cui_chat_set_rooms(inspector, people, 8);
    }
  } else {
    const char *names[] = {"call-pip-v3", "composer-states", "room-list",
                           "verify-flow", "tokens",          "whiteboard"};
    cui_chat_room media[7] = {rows[0]};
    for (unsigned i = 0; i < 6; ++i)
      media[i + 1] = (cui_chat_room){
          .id = 1000 + i,
          .title = names[i],
          .avatar_color = cui_chat_color(.86, .1, i * 55 + 20, 1)};
    cui_chat_set_rooms(inspector, media, 7);
  }
  cui_chat_select(inspector, inspector_tab);
}
static void update_thread(void) {
  cui_set_visible(thread_box, thread_open && appearance != CUI_CHAT_TILES);
  if (thread_open && appearance == CUI_CHAT_NEBULA) {
    nebula_panel = 0;
    cui_set_visible(inspector, 0);
  }
  if (appearance == CUI_CHAT_DAYLIGHT)
    cui_set_visible(inspector, inspector_open && !thread_open);
  if (headers[focused])
    header_commands(focused);
  if (!thread_open)
    return;
  if (pane_room[focused] != 0) {
    cui_chat_set_messages(thread_view, NULL, 0);
    cui_chat_set_status(thread_view, "No threads in this room yet. Hover a "
                                     "message\nand choose “Reply in thread”.");
    cui_set_visible(thread_composer, 0);
    return;
  }
  cui_chat_message rows[129];
  rows[0] = messages[0][2];
  memcpy(rows + 1, replies, reply_count * sizeof(*replies));
  for (unsigned i = 0; i <= reply_count; ++i) {
    rows[i].id = i + 100;
    rows[i].date = "";
    rows[i].thread_count = 0;
    rows[i].avatar_color = avatar_color(rows[i].author);
  }
  cui_chat_set_messages(thread_view, rows, reply_count + 1);
  cui_chat_set_status(thread_view, "");
  cui_set_visible(thread_composer, 1);
}
static void open_room(unsigned pane, unsigned room) {
  if (room >= ROOM_COUNT || pane > 3)
    return;
  if (pane_room[pane] != room) {
    cui_chat_compose_cancel(composers[pane]);
    cui_set_text(cui_chat_part(composers[pane], 0), "");
  }
  pane_room[pane] = room;
  focused = pane;
  sample_room *r = sample_rooms + room;
  char detail[512], title[128], placeholder[150];
  if (r->room.flags & CUI_CHAT_SQUARE)
    snprintf(detail, sizeof(detail), "%u members · %s%s", r->members,
             appearance == CUI_CHAT_DAYLIGHT ? "encrypted · " : "", r->topic);
  else
    snprintf(detail, sizeof(detail), "%s · %s · encrypted", r->topic,
             r->room.flags & CUI_CHAT_ONLINE ? "Online" : "Away");
  snprintf(title, sizeof(title), "%s%s", "", r->room.title);
  cui_chat_room header = r->room;
  header.title = title;
  header.detail = detail;
  char shortcut[16];
  snprintf(shortcut, sizeof(shortcut), "Alt %u", pane + 1);
  header.trailing = appearance == CUI_CHAT_NEBULA ? "Encrypted"
                    : appearance == CUI_CHAT_TILES ? shortcut : "";
  header.symbol =
      appearance == CUI_CHAT_NEBULA ? CUI_SYMBOL_LOCK : CUI_SYMBOL_NONE;
  header.avatar_color = room_color(r);
  cui_chat_set_rooms(headers[pane], &header, 1);
  cui_chat_presentation presentation;
  cui_chat_presentation_get(timelines[pane], &presentation);
  presentation.show_sender =
      appearance != CUI_CHAT_DAYLIGHT || (r->room.flags & CUI_CHAT_SQUARE) != 0;
  cui_chat_set_presentation(timelines[pane], &presentation);
  cui_chat_set_messages(timelines[pane], messages[room], counts[room]);
  cui_chat_scroll(timelines[pane], 0);
  snprintf(placeholder, sizeof(placeholder), "Message %s", title);
  cui_set_placeholder(cui_chat_part(composers[pane], 0), placeholder);
  cui_chat_set_status(
      timelines[pane],
      room == 0 && appearance == CUI_CHAT_NEBULA ? "••• Noor is typing…" : "");
  room_list();
  update_inspector();
  update_thread();
}
static void navigate(cui_widget *w, void *data) {
  (void)data;
  cui_chat_event e;
  if (!cui_chat_event_get(w, &e) || e.action != CUI_CHAT_OPEN_ROOM)
    return;
  unsigned pane = focused;
  if (appearance == CUI_CHAT_TILES) {
    unsigned mask = cui_chat_workspace_mask(workspace);
    for (unsigned i = 0; i < 4; ++i)
      if ((mask & (1u << i)) && sample_rooms[pane_room[i]].room.id == e.id) {
        sample_rooms[e.id - 1].room.unread = 0;
        focus_pane((void *)(size_t)i);
        return;
      }
  }
  if (appearance == CUI_CHAT_TILES && (e.modifiers & CUI_MOD_SHIFT)) {
    unsigned mask = cui_chat_workspace_mask(workspace);
    for (unsigned i = 0; i < 4; ++i)
      if (!(mask & (1u << i))) {
        pane = i;
        cui_chat_workspace_layout(workspace, i + 1);
        break;
      }
  }
  if (e.id > 0 && e.id <= ROOM_COUNT) {
    sample_rooms[e.id - 1].room.unread = 0;
    open_room(pane, (unsigned)e.id - 1);
    cui_focus(cui_chat_part(composers[pane], 0));
  }
}
static void select_space(cui_widget *w, void *data) {
  (void)data;
  cui_chat_event e;
  if (cui_chat_event_get(w, &e) && e.action == CUI_CHAT_OPEN_ROOM &&
      e.id >= 1 && e.id <= 5) {
    space = (unsigned)e.id - 1;
    cui_chat_select(space_tabs, e.id);
    room_list();
  }
}
static void select_space_button(cui_widget *w, void *data) {
  (void)w;
  space = (unsigned)(size_t)data;
  for (unsigned i = 0; i < 5; ++i) {
    cui_widget_style chip = {i == space ? theme.foreground : theme.background,
                             i == space ? theme.surface : theme.muted,
                             theme.border,
                             13,
                             1.5,
                             5};
    cui_set_style(space_buttons[i], &chip);
  }
  room_list();
}
static void palette_button(cui_widget *w, void *data) {
  (void)w;
  (void)data;
  open_palette(NULL);
}
static void focus_pane(void *data) {
  if (modal_kind)
    return;
  unsigned pane = (unsigned)(size_t)data;
  if (workspace && cui_chat_workspace_focus(workspace, pane)) {
    focused = pane;
    room_list();
    cui_focus(cui_chat_part(composers[pane], 0));
  }
}
static void select_filter(cui_widget *w, void *data) {
  (void)w;
  filter = (int)(size_t)data;
  for (unsigned i = 0; i < 4; ++i) {
    cui_widget_style chip = {
        i == (unsigned)filter ? theme.foreground : theme.surface,
        i == (unsigned)filter ? theme.background : theme.muted,
        theme.border,
        14,
        1,
        5};
    cui_set_style(filter_buttons[i], &chip);
  }
  room_list();
}
static void search_rooms(cui_widget *w, void *data) {
  (void)data;
  char q[4097];
  cui_get_text(w, q, sizeof(q));
  cui_chat_set_query(navigation, q);
}
static void close_thread(cui_widget *w, void *data) {
  (void)w;
  (void)data;
  thread_open = 0;
  update_thread();
}
static void header_action(cui_widget *w, void *data) {
  focused = (unsigned)(size_t)data;
  cui_chat_event e;
  cui_chat_event_get(w, &e);
  if (e.detail_id == 1 || e.detail_id == 2) {
    start_call();
    return;
  }
  if (e.detail_id == 6) {
    notice("Search this room");
    return;
  }
  if (appearance == CUI_CHAT_NEBULA && (e.detail_id == 4 || e.detail_id == 5)) {
    nebula_panel = nebula_panel == (int)e.detail_id ? 0 : (int)e.detail_id;
    thread_open = 0;
    inspector_tab = nebula_panel == 4 ? 2 : 1;
    cui_set_visible(inspector, nebula_panel != 0);
    update_inspector();
    update_thread();
    header_commands(focused);
  } else if (appearance == CUI_CHAT_DAYLIGHT && e.detail_id == 5) {
    inspector_open = !inspector_open;
    cui_set_visible(inspector, inspector_open);
    header_commands(focused);
  } else if (appearance == CUI_CHAT_TILES && e.detail_id == 7) {
    unsigned mask = cui_chat_workspace_mask(workspace);
    if (mask && !(mask & (mask - 1))) {
      cui_chat_workspace_restore(workspace);
      if (cui_chat_workspace_mask(workspace) == mask)
        layout_click(NULL, (void *)3);
      else
        cui_set_text(workspace_title, "Crit day");
    } else {
      cui_chat_workspace_maximize(workspace, focused);
      cui_set_text(workspace_title, "Focus");
    }
  } else if (appearance == CUI_CHAT_TILES && e.detail_id == 8) {
    cui_chat_workspace_close(workspace, focused);
    focused = cui_chat_workspace_focused(workspace);
    room_list();
  } else if (e.action == CUI_CHAT_THREAD) {
    thread_open = !thread_open;
    update_thread();
  }
}
static void inspector_action(cui_widget *w, void *data) {
  (void)data;
  cui_chat_event e;
  if (!cui_chat_event_get(w, &e))
    return;
  if (e.action == CUI_CHAT_MORE) {
    if (e.id == 24) {
      thread_open = 1;
      update_thread();
    } else
      notice(e.id == 22   ? "Notification settings"
             : e.id == 23 ? "Pinned messages"
             : e.id == 25 ? "Room settings"
             : e.id == 26 ? "Leave room — confirm in room settings"
                          : "Room information");
    return;
  }
  if (e.action == CUI_CHAT_OPEN_ROOM && e.id <= 3) {
    inspector_tab = (unsigned)e.id;
    update_inspector();
  }
}
static void timeline_action(cui_widget *w, void *data) {
  unsigned pane = (unsigned)(size_t)data;
  focused = pane;
  cui_chat_event e;
  if (!cui_chat_event_get(w, &e))
    return;
  if (appearance == CUI_CHAT_TILES)
    cui_chat_workspace_focus(workspace, pane);
  unsigned room = pane_room[pane];
  cui_chat_message *m = NULL;
  for (size_t i = 0; i < counts[room]; ++i)
    if (messages[room][i].id == e.id)
      m = messages[room] + i;
  if (!m)
    return;
  if (e.action == CUI_CHAT_MORE || e.action == CUI_CHAT_ATTACHMENT ||
      e.action == CUI_CHAT_LINK) {
    notice(e.action == CUI_CHAT_ATTACHMENT ? "Download file"
                                           : "Message actions");
    return;
  }
  if (e.action == CUI_CHAT_THREAD) {
    if (appearance == CUI_CHAT_TILES) {
      notice("Thread opens as a tile split");
      return;
    }
    thread_open = 1;
    update_thread();
  } else if (e.action == CUI_CHAT_REPLY) {
    cui_chat_compose_context(composers[pane], m->id, m->author,
                             m->span_count ? m->spans[0].text : "", 0);
    cui_focus(cui_chat_part(composers[pane], 0));
  } else if (e.action == CUI_CHAT_COPY && m->span_count)
    cui_clipboard_set_text(window, m->spans[0].text);
  else if (e.action == CUI_CHAT_REACT) {
    size_t mi = (size_t)(m - messages[room]), index = e.index;
    if (e.index == UINT32_MAX && *e.text) {
      for (index = 0; index < m->reaction_count; ++index)
        if (!strcmp(m->reactions[index].text, e.text))
          break;
      if (index == m->reaction_count && index < 32) {
        snprintf(reaction_keys[room][mi][index], 32, "%s", e.text);
        reaction_store[room][mi][index] =
            (cui_chat_detail){.text = reaction_keys[room][mi][index]};
        m->reactions = reaction_store[room][mi];
        ++m->reaction_count;
      }
    }
    if (index >= m->reaction_count)
      return;
    cui_chat_detail *r = (cui_chat_detail *)m->reactions + index;
    if (e.index == UINT32_MAX && (r->flags & CUI_CHAT_MINE))
      return;
    if (r->flags & CUI_CHAT_MINE) {
      r->flags &= ~CUI_CHAT_MINE;
      if (r->count)
        --r->count;
    } else {
      r->flags |= CUI_CHAT_MINE;
      ++r->count;
    }
    if (!r->count) {
      memmove(r, r + 1, (m->reaction_count - index - 1) * sizeof(*r));
      memmove(
          reaction_keys[room][mi][index], reaction_keys[room][mi][index + 1],
          (m->reaction_count - index - 1) * sizeof(reaction_keys[room][mi][0]));
      --m->reaction_count;
      for (size_t j = 0; j < m->reaction_count; ++j)
        reaction_store[room][mi][j].text = reaction_keys[room][mi][j];
    }
    cui_chat_set_messages(w, messages[room], counts[room]);
  } else if (e.action == CUI_CHAT_VOTE && e.index < m->option_count) {
    cui_chat_detail *opts = (cui_chat_detail *)m->options;
    if (m->selected_option >= 0 && opts[m->selected_option].count)
      --opts[m->selected_option].count;
    m->selected_option = (int)e.index;
    ++opts[e.index].count;
    cui_chat_set_messages(w, messages[room], counts[room]);
  }
}
static void compose(cui_widget *w, void *data) {
  unsigned pane = (unsigned)(size_t)data, room = pane_room[pane];
  cui_chat_event e;
  if (!cui_chat_event_get(w, &e))
    return;
  if (e.action == CUI_CHAT_ATTACH || e.action == CUI_CHAT_EMOJI ||
      e.action == CUI_CHAT_POLL) {
    notice(e.action == CUI_CHAT_ATTACH  ? "Attach file · Photo · Poll"
           : e.action == CUI_CHAT_EMOJI ? "Emoji picker"
                                        : "Create a poll");
    return;
  }
  if (e.action != CUI_CHAT_SEND || counts[room] >= 128)
    return;
  cui_chat_span *span = calloc(1, sizeof(*span));
  char *text = malloc(strlen(e.text) + 1);
  if (!span || !text) {
    free(span);
    free(text);
    return;
  }
  strcpy(text, e.text);
  span->text = text;
  messages[room][counts[room]] =
      (cui_chat_message){.id = 1000 + counts[room],
                         .author = "Mathias",
                         .time = "Now",
                         .avatar_color = avatar_color("Mathias"),
                         .flags = CUI_CHAT_OUTGOING,
                         .spans = span,
                         .span_count = 1,
                         .selected_option = -1};
  cui_chat_message *sent = &messages[room][counts[room]];
  sent->author_color = sender_color("Mathias");
  sent->reactions = reaction_store[room][counts[room]];
  if (counts[room] &&
      (messages[room][counts[room] - 1].flags & CUI_CHAT_OUTGOING))
    sent->flags |= CUI_CHAT_CONTINUED;
  for (size_t i = 0; i < counts[room]; ++i)
    if (messages[room][i].id == e.id) {
      sent->reply_id = e.id;
      sent->reply_author = messages[room][i].author;
      sent->reply_text =
          messages[room][i].span_count ? messages[room][i].spans[0].text : "";
      break;
    }
  ++counts[room];
  snprintf(previews[room], sizeof(previews[room]), "You: %s", text);
  sample_rooms[room].room.detail = previews[room];
  sample_rooms[room].room.trailing = "Now";
  room_list();
  cui_chat_set_messages(timelines[pane], messages[room], counts[room]);
  cui_chat_scroll(timelines[pane], -1);
  cui_set_text(cui_chat_part(w, 0), "");
  cui_chat_compose_context(w, 0, "", "", 0);
  cui_chat_refresh(w, 1);
}
static void thread_send(cui_widget *w, void *data) {
  (void)data;
  cui_chat_event e;
  if (!cui_chat_event_get(w, &e) || e.action != CUI_CHAT_SEND ||
      reply_count >= 128)
    return;
  cui_chat_span *span = calloc(1, sizeof(*span));
  char *value = malloc(strlen(e.text) + 1);
  if (!span || !value) {
    free(span);
    free(value);
    return;
  }
  strcpy(value, e.text);
  span->text = value;
  replies[reply_count] = (cui_chat_message){.id = 2000 + reply_count,
                                            .author = "Mathias",
                                            .time = "Now",
                                            .flags = CUI_CHAT_OUTGOING,
                                            .spans = span,
                                            .span_count = 1,
                                            .selected_option = -1};
  ++reply_count;
  messages[0][2].thread_count = (unsigned)reply_count;
  update_thread();
  cui_chat_set_messages(timelines[focused], messages[0], counts[0]);
  cui_set_text(cui_chat_part(w, 0), "");
  cui_chat_scroll(thread_view, -1);
}
static void layout_click(cui_widget *w, void *data) {
  (void)w;
  unsigned count = (unsigned)(size_t)data;
  cui_chat_workspace_layout(workspace, count);
  const char *names[] = {"Focus", "Pair", "Crit day", "Mission control"};
  cui_set_text(workspace_title, names[count - 1]);
  focused = cui_chat_workspace_focused(workspace);
  for (unsigned i = 0; i < 4; ++i) {
    cui_widget_style selected = {i == count - 1 ? theme.foreground
                                                : theme.surface,
                                 i == count - 1 ? theme.surface : theme.muted,
                                 theme.foreground,
                                 7,
                                 1,
                                 5};
    cui_set_style(layout_buttons[i], &selected);
  }
  room_list();
}
static void build_pane(cui_widget *parent, unsigned i) {
  style(parent, theme.surface, appearance == CUI_CHAT_TILES ? 12 : 20,
        appearance == CUI_CHAT_TILES ? theme.foreground : 0,
        appearance == CUI_CHAT_TILES ? 1.5 : 0, 0);
  headers[i] = chat(parent, CUI_CHAT_HEADER);
  cui_on_action(headers[i], header_action, (void *)(size_t)i);
  header_commands(i);
  timelines[i] = chat(parent, CUI_CHAT_TIMELINE);
  cui_on_action(timelines[i], timeline_action, (void *)(size_t)i);
  composers[i] = chat(parent, CUI_CHAT_COMPOSER);
  cui_on_action(composers[i], compose, (void *)(size_t)i);
}
static void check_interactions(void) {
  if (appearance == CUI_CHAT_DAYLIGHT) {
    assert(cui_activate(verify_button));
    assert(modal_kind == 1);
    assert(!cui_activate(verify_button));
    assert(cui_command_invoke(escape_command));
    assert(modal_kind == 0);
    assert(cui_activate(verify_button));
    assert(cui_activate(verify_accept));
    assert(session_verified && modal_kind == 0);
    inspector_tab = 2;
    update_inspector();
    inspector_tab = 3;
    update_inspector();
    inspector_tab = 1;
    update_inspector();
  } else if (appearance == CUI_CHAT_TILES) {
    open_palette(NULL);
    assert(modal_kind == 2 && cui_picker_is_open(palette));
    assert(cui_picker_set_query(palette, "engineering"));
    assert(cui_picker_accept(palette));
    assert(modal_kind == 0 && pane_room[focused] == 1);
    open_palette(NULL);
    assert(cui_command_invoke(escape_command));
    assert(modal_kind == 0);
    layout_click(NULL, (void *)4);
    focus_pane((void *)3);
    assert(focused == 3);
    layout_click(NULL, (void *)3);
    assert(focused == 0);
  } else {
    assert(!thread_open);
    cui_chat_event wanted = {.action = CUI_CHAT_THREAD,
                             .id = messages[0][2].id};
    cui_chat_scroll_to(timelines[0], wanted.id);
    cui_chat_refresh(timelines[0], cui_window_scale(window));
    unsigned region = cui_chat_action_region(timelines[0], &wanted);
    assert(region &&
           cui_canvas_activate_region(cui_chat_part(timelines[0], 0), region));
    assert(thread_open);
    open_room(0, 1);
    assert(thread_open);
    close_thread(NULL, NULL);
    assert(!thread_open);
    open_room(0, 0);
    start_call();
    assert(call_active);
    end_call(NULL, NULL);
    assert(!call_active);
  }
  size_t before = counts[pane_room[focused]];
  cui_set_text(cui_chat_part(composers[focused], 0), "Parity contract message");
  assert(cui_chat_compose_submit(composers[focused]));
  assert(counts[pane_room[focused]] == before + 1);
  assert(!cui_get_text(cui_chat_part(composers[focused], 0), NULL, 0));
}
static void refresh(void *data) {
  (void)data;
  refresh_backdrop();
  for (size_t i = 0; i < refresh_count; ++i)
    assert(cui_chat_refresh(refreshes[i], cui_window_scale(window)));
  if (toast_until && cui_time() > toast_until) {
    cui_set_visible(toast_layer, 0);
    toast_until = 0;
  }
  if (call_active) {
    unsigned seconds = (unsigned)(cui_time() - call_started);
    char value[32];
    snprintf(value, sizeof(value), "%02u:%02u", seconds / 60, seconds % 60);
    cui_set_text(call_clock, value);
  }
  if (modal_kind) {
    int width = 0, height = 0;
    cui_widget_get_size(scrim_canvas, &width, &height);
    cui_canvas_region region = {
        1, 0, 0, (float)width, (float)height, "Dismiss dialog", 1};
    cui_canvas_set_regions(scrim_canvas, &region, 1);
  }
  static unsigned ticks = 0;
  if (++ticks == 3) {
    if (workspace)
      layout_click(NULL, (void *)3);
    const char *state = getenv("CUI_CAPTURE_STATE");
    if (state && !strcmp(state, "verification"))
      modal_open(1);
    if (state && !strcmp(state, "palette"))
      open_palette(NULL);
    if (state && !strcmp(state, "call"))
      start_call();
    if (state && !strcmp(state, "thread")) {
      thread_open = 1;
      update_thread();
    }
    if (state && !strcmp(state, "people")) {
      inspector_tab = 2;
      update_inspector();
    }
    if (state && !strcmp(state, "media")) {
      inspector_tab = 3;
      update_inspector();
    }
  }
  if (ticks == (getenv("CUI_CAPTURE") ? 20u : 7u)) {
    puts("READY");
    fflush(stdout);
    if (getenv("CUI_SMOKE_TEST")) {
      check_interactions();
      puts("Native concept interaction contracts passed");
      cui_app_quit(app);
    }
  }
}
int main(int argc, char **argv) {
  appearance = argc > 1 && !strcmp(argv[1], "daylight") ? CUI_CHAT_DAYLIGHT
               : argc > 1 && !strcmp(argv[1], "tiles")  ? CUI_CHAT_TILES
                                                        : CUI_CHAT_NEBULA;
  memcpy(replies, history_thread, sizeof(history_thread));
  const char *initial = getenv("CUI_CHAT_ROOM");
  if (initial)
    for (unsigned i = 0; i < ROOM_COUNT; ++i)
      if (!strcmp(initial, sample_rooms[i].key))
        pane_room[0] = i;
  cui_chat_theme_get(appearance, &theme);
  app = cui_app_create();
  assert(app);
  cui_app_set_theme(app, appearance == CUI_CHAT_NEBULA ? CUI_THEME_DARK
                                                       : CUI_THEME_LIGHT);
  for (size_t r = 0; r < ROOM_COUNT; ++r) {
    counts[r] = sample_rooms[r].count;
    memcpy(messages[r], sample_rooms[r].messages,
           counts[r] * sizeof(cui_chat_message));
    for (size_t i = 0; i < counts[r]; ++i) {
      messages[r][i].avatar_color = avatar_color(messages[r][i].author);
      messages[r][i].author_color = sender_color(messages[r][i].author);
      for (size_t j = 0; j < messages[r][i].reaction_count; ++j) {
        reaction_store[r][i][j] = messages[r][i].reactions[j];
        snprintf(reaction_keys[r][i][j], 32, "%s",
                 messages[r][i].reactions[j].text);
        reaction_store[r][i][j].text = reaction_keys[r][i][j];
      }
      messages[r][i].reactions = reaction_store[r][i];
      for (size_t f = 0; f < messages[r][i].thread_participant_count; ++f) {
        cui_chat_room *face =
            (cui_chat_room *)messages[r][i].thread_participants + f;
        face->avatar_color = avatar_color(face->title);
      }
    }
  }
  int width = getenv("CUI_CAPTURE_WIDTH") ? atoi(getenv("CUI_CAPTURE_WIDTH"))
                                          : 1440,
      height = getenv("CUI_CAPTURE_HEIGHT") ? atoi(getenv("CUI_CAPTURE_HEIGHT"))
                                            : 900;
  window = cui_window_create(app, "CUI Chat Components", width, height);
  cui_window_set_frame(window, 0, 1, 0);
  cui_widget *root = cui_window_root(window);
  cui_box_set_padding(root, 0);
  style(root, theme.background, 0, 0, 0, 0);
  cui_set_font(root, "sans", 10.5, 400);
  layers = cui_stack(root);
  if (appearance == CUI_CHAT_DAYLIGHT) {
    cui_widget *background=cui_stack_layer(layers,CUI_LAYER_FILL,0,0,0);
    backdrop=cui_canvas(background);
    cui_expand(backdrop,1);
    cui_set_min_size(backdrop,1100,500);
  }
  base_layer = cui_stack_layer(layers, CUI_LAYER_FILL, 0, 0, 0);
  cui_widget *page = cui_box(base_layer, CUI_VERTICAL, 0);
  cui_expand(page, 1);
  cui_widget *top = cui_box(page, CUI_HORIZONTAL, 12);
  style(top, appearance == CUI_CHAT_NEBULA ? theme.rail : theme.background, 0,
        0, 0, 6);
  cui_set_min_size(top, 1, appearance == CUI_CHAT_NEBULA ? 38 : 56);
  if (appearance == CUI_CHAT_DAYLIGHT) {
    cui_icon_asset *logo = brand_art();
    cui_widget *mark = cui_icon(top, logo);
    cui_icon_release(logo);
    cui_set_icon_size(mark, 22);
    label(top, "Daylight", 18, 800);
  } else
    label(top, appearance == CUI_CHAT_NEBULA ? "◆ Nebula" : "▦ Tiles",
          appearance == CUI_CHAT_NEBULA ? 13 : 18, 700);
  for (unsigned i = 0; i < 5; ++i) {
    static const unsigned hues[] = {0, 40, 165, 300, 95};
    sample_spaces[i].avatar_color =
        i ? cui_chat_color(appearance == CUI_CHAT_NEBULA ? .80 : .84,
                           appearance == CUI_CHAT_NEBULA ? .14 : .12, hues[i],
                           1)
          : theme.soft;
  }
  if (appearance == CUI_CHAT_DAYLIGHT) {
    space_tabs = chat(top, CUI_CHAT_SPACES);
    cui_set_min_size(cui_chat_part(space_tabs, 0), 760, 42);
    cui_expand(space_tabs, 0);
    cui_widget *space_spacer = label(top, "", 12, 400);
    cui_expand(space_spacer, 1);
    cui_widget *verify = button(top, "Verify this session", verify_show, NULL);
    verify_button = verify;
    style(verify, theme.surface, 18, cui_chat_color(.76, .14, 75, 1), 1.5, 8);
    artwork(verify, shield_art(), 16);
    cui_set_font(verify, NULL, 9.75, 600);
    cui_widget *profile = button(top, "Mathias", preferences, NULL);
    artwork(profile, profile_art(), 28);
    style(profile, theme.surface, 18, 0, 0, 4);
    cui_set_font(profile, NULL, 9.75, 600);
    space = 0;
  } else {
    cui_widget *spacer = label(top, "", 12, 400);
    cui_expand(spacer, 1);
    cui_widget *search = cui_search(top, "Search rooms, people, messages");
    cui_set_min_size(search, 440, 26);
    style(search, theme.surface, 7, theme.border, 1, 3);
    spacer = label(top, "", 12, 400);
    cui_expand(spacer, 1);
  }
  if (appearance == CUI_CHAT_TILES)
    cui_set_visible(top, 0);
  cui_widget *body_parent = page;
  if (appearance == CUI_CHAT_DAYLIGHT) {
    body_parent = cui_box(page, CUI_HORIZONTAL, 0);
    cui_expand(body_parent, 1);
    cui_widget *margin = cui_box(body_parent, CUI_VERTICAL, 0);
    cui_set_min_size(margin, 12, 1);
  }
  cui_widget *body = cui_box(body_parent, CUI_HORIZONTAL,
                             appearance == CUI_CHAT_DAYLIGHT ? 12 : 0);
  cui_expand(body, 1);
  if (appearance == CUI_CHAT_DAYLIGHT) {
    cui_widget *margin = cui_box(body_parent, CUI_VERTICAL, 0);
    cui_set_min_size(margin, 12, 1);
    cui_widget *bottom = cui_box(page, CUI_HORIZONTAL, 0);
    cui_set_min_size(bottom, 1, 12);
  }
  if (appearance == CUI_CHAT_NEBULA) {
    cui_widget *rail_box = cui_box(body, CUI_VERTICAL, 0);
    style(rail_box, theme.rail, 0, theme.border, 1, 0);
    space_tabs = chat(rail_box, CUI_CHAT_SPACES);
    cui_expand(space_tabs, 0);
    cui_set_min_size(cui_chat_part(space_tabs, 0), 70, 304);
    cui_set_min_size(space_tabs, 70, 304);
    cui_chat_theme rail = theme;
    rail.surface = theme.rail;
    cui_chat_set_theme(space_tabs, &rail);
    cui_widget *row = cui_box(rail_box, CUI_HORIZONTAL, 0);
    cui_box_set_padding(row, 12);
    cui_widget *add =
        button(row, "Add space", show_notice, "Create or join a space");
    artwork(add, cui_icon_symbol(CUI_SYMBOL_PLUS), 20);
    cui_set_icon_only(add, 1);
    style(add, theme.rail, 15, theme.border, 1, 12);
    cui_widget *spacer = cui_box(rail_box, CUI_VERTICAL, 0);
    cui_expand(spacer, 1);
    row = cui_box(rail_box, CUI_HORIZONTAL, 0);
    cui_box_set_padding(row, 12);
    cui_widget *profile = button(row, "Profile", preferences, NULL);
    artwork(profile, profile_art(), 46);
    cui_set_icon_only(profile, 1);
    style(profile, theme.rail, 23, 0, 0, 0);
  }
  sidebar = cui_box(body, CUI_VERTICAL, 0);
  cui_set_min_size(sidebar,
                   appearance == CUI_CHAT_NEBULA     ? 280
                   : appearance == CUI_CHAT_DAYLIGHT ? 300
                                                     : 248,
                   1);
  style(sidebar, theme.surface, appearance == CUI_CHAT_DAYLIGHT ? 20 : 0, 0, 0,
        0);
  if (appearance == CUI_CHAT_TILES) {
    style(sidebar, theme.background, 0, theme.foreground, 1, 0);
    cui_widget *brand = cui_box(sidebar, CUI_HORIZONTAL, 10);
    cui_box_set_padding(brand, 16);
    label(brand, "▦ Tiles", 17, 700);
  }
  cui_widget *sidehead = cui_box(sidebar, CUI_VERTICAL, 10);
  cui_box_set_padding(sidehead, 16);
  space_title = label(sidehead, "Lumen Labs",
                      appearance == CUI_CHAT_DAYLIGHT ? 22 : 18, 700);
  if (appearance == CUI_CHAT_TILES)
    cui_set_visible(space_title, 0);
  if (appearance == CUI_CHAT_NEBULA) {
    cui_widget *filters = cui_box(sidehead, CUI_HORIZONTAL, 6);
    const char *names[] = {"All", "Unread", "People", "Favourites"};
    for (unsigned i = 0; i < 3; ++i) {
      filter_buttons[i] =
          button(filters, names[i], select_filter, (void *)(size_t)i);
      cui_set_font(filter_buttons[i], NULL, 9, 400);
    }
    cui_widget *second = cui_box(sidehead, CUI_HORIZONTAL, 0);
    filter_buttons[3] = button(second, "Favourites", select_filter, (void *)3);
    cui_set_font(filter_buttons[3], NULL, 9, 400);
  } else if (appearance == CUI_CHAT_TILES) {
    cui_widget *launcher =
        button(sidehead, "Jump or run…     Ctrl K", palette_button, NULL);
    style(launcher, theme.surface, 8, theme.foreground, 1.5, 8);
  } else {
    room_filter = cui_search(sidehead, "Filter rooms and people");
    style(room_filter, theme.soft, 12, 0, 0, 8);
    cui_on_action(room_filter, search_rooms, NULL);
  }
  if (appearance == CUI_CHAT_TILES) {
    space_tabs = NULL;
    space = 0;
    cui_widget *chips = NULL;
    for (unsigned i = 0; i < 5; ++i) {
      if (i != 1)
        chips = cui_box(sidehead, CUI_HORIZONTAL, 6);
      space_buttons[i] = button(chips, i ? sample_spaces[i].title : "All",
                                select_space_button, (void *)(size_t)i);
      cui_set_font(space_buttons[i], NULL, 9, 600);
      style(space_buttons[i], theme.background, 13, theme.border, 1.5, 5);
    }
  } else {
    sample_spaces[0].symbol = CUI_SYMBOL_HOME;
    if (appearance == CUI_CHAT_NEBULA)
      sample_spaces[1].group = "Spaces";
    cui_chat_set_rooms(space_tabs, sample_spaces, 5);
    cui_chat_select(space_tabs, space + 1);
    cui_on_action(space_tabs, select_space, NULL);
  }
  navigation = chat(sidebar, CUI_CHAT_ROOMS);
  cui_on_action(navigation, navigate, NULL);
  if (appearance == CUI_CHAT_TILES) {
    cui_widget *account = cui_box(sidebar, CUI_HORIZONTAL, 10);
    style(account, theme.background, 0, theme.foreground, 1.5, 14);
    cui_widget *avatar = cui_icon(account, NULL);
    artwork(avatar, profile_art(), 26);
    cui_widget *identity = cui_box(account, CUI_VERTICAL, 0);
    cui_expand(identity, 1);
    label(identity, "Mathias", 13, 700);
    label(identity, "@mathias:lumen.chat", 11.5, 400);
    cui_widget *settings = button(account, "Settings", preferences, NULL);
    artwork(settings, cui_icon_symbol(CUI_SYMBOL_MORE), 18);
    cui_set_icon_only(settings, 1);
    style(settings, theme.background, 6, 0, 0, 4);
  }
  cui_widget *main = cui_box(body, CUI_VERTICAL, 0);
  cui_expand(main, 1);
  if (appearance == CUI_CHAT_TILES) {
    cui_widget *bar = cui_box(main, CUI_HORIZONTAL, 8);
    cui_box_set_padding(bar, 12);
    cui_set_min_size(bar, 1, 54);
    workspace_title = label(bar, "Crit day", 15, 700);
    label(bar, "Alt 1–4  focus tile · Ctrl K  commands", 12.5, 400);
    cui_widget *spacer = cui_box(bar, CUI_HORIZONTAL, 0);
    cui_expand(spacer, 1);
    const char *names[] = {"Single layout", "Split layout", "Main and two layout", "Quad layout"};
    cui_widget *segments = cui_box(bar, CUI_HORIZONTAL, 0);
    style(segments, theme.surface, 8, theme.foreground, 1.5, 0);
    for (unsigned i = 0; i < 4; ++i) {
      layout_buttons[i] = button(segments, names[i], layout_click, (void *)(size_t)(i + 1));
      artwork(layout_buttons[i], layout_art(i + 1), 18);
      cui_set_icon_only(layout_buttons[i], 1);
      cui_set_min_size(layout_buttons[i], 38, 30);
    }
    workspace = chat(main, CUI_CHAT_WORKSPACE);
    style(workspace, theme.background, 0, 0, 0, 12);
    for (unsigned i = 0; i < 4; ++i)
      build_pane(cui_chat_part(workspace, i), i);
  } else
    build_pane(main, 0);
  inspector = chat(body, CUI_CHAT_INSPECTOR);
  cui_expand(inspector, 0);
  cui_set_min_size(inspector, 300, 1);
  cui_on_action(inspector, inspector_action, NULL);
  cui_set_visible(inspector, appearance == CUI_CHAT_DAYLIGHT);
  thread_box = cui_box(body, CUI_VERTICAL, 0);
  cui_set_min_size(thread_box, 340, 1);
  style(thread_box, theme.surface, 0, theme.border, 1, 0);
  cui_widget *threadhead = cui_box(thread_box, CUI_HORIZONTAL, 12);
  cui_box_set_padding(threadhead, 18);
  cui_set_min_size(threadhead, 1, 62);
  cui_widget *title = label(threadhead, "Thread", 15, 700);
  cui_expand(title, 1);
  button(threadhead, "×", close_thread, NULL);
  thread_view = chat(thread_box, CUI_CHAT_TIMELINE);
  cui_chat_theme tt = theme;
  tt.background = tt.surface;
  cui_chat_set_theme(thread_view, &tt);
  thread_composer = chat(thread_box, CUI_CHAT_COMPOSER);
  cui_on_action(thread_composer, thread_send, NULL);
  cui_chat_presentation thread_presentation;
  cui_chat_presentation_get(thread_composer, &thread_presentation);
  thread_presentation.composer_tools = 0;
  thread_presentation.composer_padding = 14;
  cui_chat_set_presentation(thread_composer, &thread_presentation);
  cui_set_placeholder(cui_chat_part(thread_composer, 0), "Reply in thread…");
  cui_set_visible(thread_box, 0);
  for (unsigned i = 0; i < (appearance == CUI_CHAT_TILES ? 4u : 1u); ++i)
    open_room(i, pane_room[i]);
  focused = 0;
  room_list();
  update_inspector();
  if (appearance == CUI_CHAT_TILES) {
    select_space_button(NULL, NULL);
    layout_click(NULL, (void *)3);
    for (unsigned i = 0; i < 4; ++i)
      cui_command_create(app, "Focus pane", '1' + i, CUI_MOD_ALT, focus_pane,
                         (void *)(size_t)i);
  }
  if (appearance == CUI_CHAT_NEBULA)
    select_filter(NULL, NULL);
  build_overlays();
  cui_every(app, 100, refresh, NULL);
  cui_window_show(window);
  cui_focus(cui_chat_part(composers[0], 0));
  cui_app_run(app);
  cui_app_destroy(app);
  for (size_t r = 0; r < ROOM_COUNT; ++r)
    for (size_t i = sample_rooms[r].count; i < counts[r]; ++i) {
      free((char *)messages[r][i].spans[0].text);
      free((void *)messages[r][i].spans);
    }
  for (size_t i = 3; i < reply_count; ++i) {
    free((char *)replies[i].spans[0].text);
    free((void *)replies[i].spans);
  }
  return 0;
}
