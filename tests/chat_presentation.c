#include "cui_chat_internal.h"
#include "cui_draw_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static cui_app *app;
static cui_widget *parent, *message, *rooms, *inspector;
static float word_width(chat_state *s, const char *word) {
  for (size_t i=0;i<8192;++i)
    if(s->metrics[i].text && !strcmp(s->metrics[i].text,word)) return s->metrics[i].width;
  assert(0); return 0;
}
static void check_font(void) {
  assert(cui_chat_refresh(message,1));
  chat_state *s=cui__chat(message);
  float before=word_width(s,"iiii");
  assert(cui_set_font(parent,"monospace",10.5,400));
  assert(cui_chat_refresh(message,1));
  assert(word_width(s,"iiii")>before);
  for(size_t i=0;i<s->scene.count;++i)
    if(s->scene.commands[i].op==CUI_DRAW_TEXT)
      assert(!strcmp(s->scene.commands[i].font,"monospace"));
  assert(cui_set_font(parent,"sans",10.5,400));
  assert(cui_chat_refresh(message,1));
  assert(word_width(s,"iiii")==before);
}
static void check_hover(void) {
  assert(cui_chat_refresh(rooms,1));
  chat_state *s=cui__chat(rooms);
  cui_canvas_region r=s->scene.regions[0];
  cui__canvas_event(cui_chat_part(rooms,0),CUI_CANVAS_MOVE,r.x+2,r.y+2,0,0,0);
  assert(s->hover_region==r.id&&s->dirty);
  assert(cui_chat_refresh(rooms,1));
  cui__canvas_event(cui_chat_part(rooms,0),CUI_CANVAS_MOVE,-1,-1,0,0,0);
  assert(!s->hover_region&&s->dirty);
}
static void check_focus_contents(void) {
  assert(cui_chat_refresh(rooms,1));
  cui_widget *canvas=cui_chat_part(rooms,0);
  chat_state *s=cui__chat(rooms);
  cui_canvas_region r=s->scene.regions[0];
  int x=(int)(r.x+r.width/2), y=(int)(r.y+r.height/2);
  cui_surface *surface=cui__canvas_state(canvas)->surface;
  uint32_t before=surface->pixels[y*surface->width+x];
  cui__canvas_focus(canvas,r.id);
  assert(cui_chat_refresh(rooms,1));
  surface=cui__canvas_state(canvas)->surface;
  assert(surface->pixels[y*surface->width+x]==before);
}
static void check_single_line_alignment(void) {
  chat_state *s=cui__chat(rooms);
  int found=0;
  for(size_t i=0;i<s->scene.count;++i) {
    const cui_draw_command *c=s->scene.commands+i;
    if(c->op==CUI_DRAW_TEXT&&!strcmp(c->text,"Room")) {
      /* The draw API's vertical text box is p[6], after alignment p[5]. */
      assert(c->p[6]==s->presentation.room_height);
      found=1;
    }
  }
  assert(found);
  const cui_chat_room row={.id=1,.title="Room",.detail="Preview"};
  assert(cui_chat_set_rooms(rooms,&row,1));
  assert(cui_chat_refresh(rooms,1));
  for(size_t i=0;i<s->scene.count;++i)
    if(s->scene.commands[i].op==CUI_DRAW_TEXT&&!strcmp(s->scene.commands[i].text,"Room"))
      assert(s->scene.commands[i].p[6]==0);
}
static void check_toolbar_focus(void) {
  const cui_chat_command more={.id=17,.label="More",.symbol=CUI_SYMBOL_MORE,.action=CUI_CHAT_MORE};
  assert(cui_chat_set_commands(message,&more,1));
  assert(cui_chat_refresh(message,1));
  cui_chat_event body={.action=CUI_CHAT_MORE,.id=1};
  cui_widget *canvas=cui_chat_part(message,0);
  assert(cui_canvas_focus_region(canvas,cui_chat_action_region(message,&body)));
  assert(cui_chat_refresh(message,1));
  cui_chat_event action={.action=CUI_CHAT_MORE,.id=1,.detail_id=17,.index=UINT32_MAX};
  unsigned region=cui_chat_action_region(message,&action);assert(region);
  assert(cui_canvas_focus_region(canvas,region));
  cui__canvas_event(canvas,CUI_CANVAS_MOVE,-1,-1,0,0,0);
  assert(cui_chat_refresh(message,1));
  assert(cui_chat_action_region(message,&action)==region);
}
static void check_timeline_hover(void) {
  cui_window *window = cui_window_create(app, "Hover regression", 620, 420);
  cui_widget *chat = cui_chat_create(cui_window_root(window), CUI_CHAT_TIMELINE,
                                     CUI_CHAT_DAYLIGHT);
  cui_chat_span span = {"Message body", "", CUI_CHAT_BODY};
  cui_chat_message items[4] = {0};
  for (unsigned i = 0; i < 4; ++i)
    items[i] = (cui_chat_message){.id = i + 1,
                                  .author = "Author",
                                  .spans = &span,
                                  .span_count = 1,
                                  .selected_option = -1};
  assert(cui_chat_set_messages(chat, items, 4));
  chat_state *s = cui__chat(chat);
  s->width = 620;
  s->height = 420;
  s->offset = 0;
  assert(cui__chat_layout(s));
  assert(cui__chat_paint(s));
  cui_widget *canvas = cui_chat_part(chat, 0);
  cui__canvas_focus(canvas, 513); /* Focus message 2, then hover message 3. */
  cui__canvas_event(canvas, CUI_CANVAS_MOVE, 100, s->tops[2] + 35, 0, 0, 0);
  assert(cui__chat_paint(s));
  unsigned toolbar = 0;
  for (size_t i = 0; i < s->scene.region_count; ++i) {
    cui_canvas_region r = s->scene.regions[i];
    if (r.id % 256 >= 200) {
      assert(s->scene.actions[i].id == 3);
      assert(r.y + r.height < s->tops[2]);
      toolbar = r.id;
    }
  }
  assert(toolbar);
  cui_canvas_region r = {0};
  for (size_t i = 0; i < s->scene.region_count; ++i)
    if (s->scene.regions[i].id == toolbar)
      r = s->scene.regions[i];
  cui__canvas_event(canvas, CUI_CANVAS_MOVE, r.x + 8, r.y + 8, 0, 0, 0);
  assert(s->hovered == 3); /* Raised actions must not hover the previous row. */
  assert(cui_canvas_focus_region(canvas, toolbar));
  cui__canvas_event(canvas, CUI_CANVAS_MOVE, -1, -1, 0, 0, 0);
  cui__canvas_event(canvas, CUI_CANVAS_PRESS, r.x + 8, r.y + 8, 0, 0, 0);
  assert(cui__chat_paint(s));
  cui__canvas_event(canvas, CUI_CANVAS_RELEASE, r.x + 8, r.y + 8, 0, 0, 0);
  assert(s->event.id == 3 && s->event.action != CUI_CHAT_FOCUS);
  cui__canvas_event(canvas, CUI_CANVAS_MOVE, 100, s->tops[2] + 35, 0, 0, 0);
  cui__canvas_event(canvas, CUI_CANVAS_MOVE, -1, -1, 0, 0, 0);
  assert(cui__chat_paint(s));
  for (size_t i = 0; i < s->scene.region_count; ++i)
    assert(s->scene.regions[i].id % 256 < 200);
  cui__canvas_event(canvas, CUI_CANVAS_CONTEXT, 100, s->tops[2] + 35, 0, 0, 0);
  assert(s->event.action == CUI_CHAT_MORE && s->event.id == 3);
}
static void check_room_scroll_and_avatar(void) {
  cui_chat_room rows[30] = {0};
  for (unsigned i = 0; i < 30; ++i)
    rows[i] = (cui_chat_room){
        .id = i + 1, .title = "Room", .group = i < 10 ? "Favorites" : "Rooms"};
  unsigned char pixels[8] = {255, 0, 0, 255, 0, 255, 0, 255};
  cui_icon_asset *asset = cui_icon_rgba(pixels, 2, 1);
  assert(asset);
  rows[0].avatar = asset;
  assert(cui_chat_set_rooms(rooms, rows, 30));
  cui_icon_release(asset); /* The native model must retain the image. */
  assert(cui_chat_refresh(rooms, 1));
  chat_state *s = cui__chat(rooms);
  int found = 0;
  for (size_t i = 0; i < s->scene.count; ++i)
    if (s->scene.commands[i].op == CUI_DRAW_ICON &&
        s->scene.commands[i].icon == s->rooms[0].avatar)
      found = 1;
  assert(found);
  cui_widget *canvas = cui_chat_part(rooms, 0);
  assert(cui_chat_scroll(rooms, 0));
  cui__canvas_event(canvas, CUI_CANVAS_SCROLL, 20, 40, 0, -1, 0);
  assert(s->offset ==
         0); /* Up at the top must not use the API's bottom sentinel. */
  cui__canvas_event(canvas, CUI_CANVAS_SCROLL, 20, 40, 0, 4, 0);
  assert(s->offset > 0);
  double offset = s->offset;
  assert(cui_chat_refresh(rooms, 1));
  assert(s->offset == offset);
  assert(cui_chat_set_rooms(rooms, NULL, 0));
  assert(cui_chat_refresh(rooms, 1));
  assert(s->offset ==
         0); /* Filter/model shrink cannot strand an empty viewport. */
}
static void verify(void *data) {
  (void)data;
  check_font(); check_hover(); check_focus_contents(); check_single_line_alignment();
  check_toolbar_focus();
  check_timeline_hover();
  check_room_scroll_and_avatar();
  cui_chat_presentation p;
  assert(cui_chat_presentation_get(inspector,&p));
  p.inspector=CUI_CHAT_PEOPLE_LIST;
  assert(cui_chat_set_presentation(inspector,&p));
  assert(cui_chat_refresh(inspector,1));
  cui_chat_event invite={.action=CUI_CHAT_MORE,.id=2000};
  assert(cui_chat_action_region(inspector,&invite));
  p.inspector=CUI_CHAT_MEDIA_GRID;
  assert(cui_chat_set_presentation(inspector,&p));
  assert(cui_chat_set_status(inspector,"Shared files in this room"));
  assert(cui_chat_refresh(inspector,1));
  chat_state *s=cui__chat(inspector);
  int caption=0;
  for(size_t i=0;i<s->scene.count;++i)
    if(s->scene.commands[i].op==CUI_DRAW_TEXT&&!strcmp(s->scene.commands[i].text,"Shared")) caption=1;
  assert(caption);
  cui_app_quit(app);
}
int main(void) {
  app=cui_app_create();assert(app);
  cui_window *w=cui_window_create(app,"Chat presentation",800,900);
  parent=cui_window_root(w);
  assert(cui_set_font(parent,"sans",10.5,400));
  message=cui_chat_create(parent,CUI_CHAT_MESSAGE,CUI_CHAT_DAYLIGHT);
  rooms=cui_chat_create(parent,CUI_CHAT_ROOMS,CUI_CHAT_DAYLIGHT);
  inspector=cui_chat_create(parent,CUI_CHAT_INSPECTOR,CUI_CHAT_DAYLIGHT);
  const cui_chat_span span={"iiii","",CUI_CHAT_BODY};
  const cui_chat_message m={.id=1,.author="Author",.spans=&span,.span_count=1,.selected_option=-1};
  assert(cui_chat_set_messages(message,&m,1));
  const cui_chat_room rows[]={{.id=1,.title="Room"},{.id=2000,.title="Invite people",.symbol=CUI_SYMBOL_PLUS}};
  assert(cui_chat_set_rooms(rooms,rows,1));
  assert(cui_chat_set_rooms(inspector,rows,2));
  cui_every(app,300,verify,NULL);cui_window_show(w);cui_app_run(app);cui_app_destroy(app);
  puts("chat presentation: inherited font relayout, hover exit, inspector caption and action row passed");
}
