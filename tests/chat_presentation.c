#include "cui_chat_internal.h"
#include "cui_draw_internal.h"
#include <assert.h>
#include <math.h>
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
  cui_chat_event body={.action=CUI_CHAT_NONE,.id=1};
  cui_widget *canvas=cui_chat_part(message,0);
  assert(cui_canvas_focus_region(canvas,cui_chat_action_region(message,&body)));
  assert(cui_chat_refresh(message,1));
  /* Left activation selects without opening a menu; secondary activation carries
   * its pointer position, independently of the toolbar's anchor rectangle. */
  unsigned body_region=cui_chat_action_region(message,&body);
  assert(cui_canvas_activate_region(canvas,body_region));
  cui_chat_event clicked; assert(cui_chat_event_get(message,&clicked));
  assert(clicked.action==CUI_CHAT_NONE);
  double px=0,py=0; assert(!cui_chat_event_position(message,&px,&py));
  chat_state *state=cui__chat(message);
  for(size_t i=0;i<state->scene.region_count;++i) if(state->scene.regions[i].id==body_region) {
    cui_canvas_region r=state->scene.regions[i];
    cui__canvas_event(canvas,CUI_CANVAS_CONTEXT,r.x+3,r.y+3,0,0,0);
    assert(cui_chat_event_get(message,&clicked)&&clicked.action==CUI_CHAT_MORE);
    assert(cui_chat_event_position(message,&px,&py)&&px==r.x+3&&py==r.y+3);
    break;
  }
  cui_app_set_focus_indicators(app,0);
  assert(app->hide_focus);
  cui_app_set_focus_indicators(app,1);
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
  assert(cui_chat_scroll(chat, 0));
#ifdef __APPLE__
  cui__canvas_event(canvas, CUI_CANVAS_SCROLL, 20, 40, 0, 1, 0);
#else
  cui__canvas_event(canvas, CUI_CANVAS_SCROLL, 20, 40, 0, -1, 0);
#endif
  assert(s->event.action == CUI_CHAT_LOAD_OLDER);
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
static void check_links_and_profile(void) {
  cui_chat_span span={"https://example.org", "https://example.org", CUI_CHAT_BODY};
  cui_chat_message item={.id=7,.author="Sender",.spans=&span,.span_count=1,.selected_option=-1};
  assert(cui_chat_set_messages(message,&item,1));
  assert(cui_chat_refresh(message,1));
  cui_chat_event profile={.action=CUI_CHAT_OPEN_PROFILE,.id=7};
  unsigned region=cui_chat_action_region(message,&profile);assert(region);
  assert(cui_canvas_activate_region(cui_chat_part(message,0),region));
  cui_chat_event result;assert(cui_chat_event_get(message,&result));
  assert(result.action==CUI_CHAT_OPEN_PROFILE&&result.id==7);
  cui_chat_event link={.action=CUI_CHAT_LINK,.id=7,.text="https://example.org"};
  region=cui_chat_action_region(message,&link);assert(region);
  assert(cui_canvas_activate_region(cui_chat_part(message,0),region));
  assert(cui_chat_event_get(message,&result));
  assert(result.action==CUI_CHAT_LINK&&!strcmp(result.text,span.link));
  item.flags=CUI_CHAT_OUTGOING;
  assert(cui_chat_set_messages(message,&item,1));
  assert(cui_chat_refresh(message,1));
  chat_state *state=cui__chat(message);
  int found=0;
  for(size_t i=0;i<state->scene.count;++i) {
    const cui_draw_command *c=state->scene.commands+i;
    if(c->op==CUI_DRAW_TEXT&&!strcmp(c->text,span.text)) {
      assert(c->color==state->theme.on_accent);
      assert(c->color!=state->theme.accent);
      found=1;
    }
  }
  assert(found);
}
static void check_dark_unread(void) {
  cui_chat_theme dark;
  assert(cui_chat_theme_get(CUI_CHAT_DAYLIGHT, &dark));
  dark.surface = 0x1b2932ff; dark.foreground = 0xe7f0f5ff;
  dark.hover = 0x314754ff; dark.accent = 0x99dbecff; dark.on_accent = 0x112631ff;
  assert(cui_chat_set_theme(rooms, &dark));
  const cui_chat_room row = {.id=1,.title="Unread room",.unread=1};
  assert(cui_chat_set_rooms(rooms,&row,1));
  assert(cui_chat_select(rooms,1));
  assert(cui_chat_refresh(rooms,1));
  chat_state *s=cui__chat(rooms);
  const cui_draw_command *badge=NULL,*label=NULL;
  int selected=0;
  for(size_t i=0;i<s->scene.count;++i) {
    const cui_draw_command *c=s->scene.commands+i;
    if(c->op==CUI_DRAW_RECT&&c->color==dark.hover) selected=1;
    if(c->op==CUI_DRAW_RECT&&c->color==dark.accent&&c->p[3]==22) badge=c;
    if(c->op==CUI_DRAW_TEXT&&!strcmp(c->text,"1")) label=c;
  }
  assert(selected&&badge&&label);
  assert(badge->p[2]>=22 && label->color==dark.on_accent);
  assert(label->p[1]==badge->p[1] && label->p[6]==badge->p[3]);
  float tw=word_width(s,"1");
  assert(fabsf(label->p[0]+tw/2-(badge->p[0]+badge->p[2]/2))<1);
}
static void check_avatar_rail(void) {
  chat_state *s=cui__chat(rooms);
  const cui_chat_room rows[]={
    {.id=1,.title="Room alpha",.group="Rooms",.unread=2},
    {.id=2,.title="Room beta",.group="Direct messages"}};
  assert(cui_chat_set_rooms(rooms,rows,2));
  s->width=80; s->height=200; s->offset=0;
  assert(cui__chat_layout(s)&&cui__chat_paint(s));
  assert(s->scene.region_count==2);
  assert(s->scene.actions[1].id==2);
  for(size_t i=0;i<s->scene.count;++i) {
    const cui_draw_command *c=s->scene.commands+i;
    if(c->op==CUI_DRAW_TEXT)
      assert(strcmp(c->text,"Room alpha")&&strcmp(c->text,"Room beta")&&strcmp(c->text,"Rooms")&&strcmp(c->text,"Direct messages"));
  }
  cui_canvas_region region=s->scene.regions[1];
  assert(cui_canvas_activate_region(cui_chat_part(rooms,0),region.id));
  cui_chat_event event; assert(cui_chat_event_get(rooms,&event));
  assert(event.action==CUI_CHAT_OPEN_ROOM&&event.id==2);
}
static void check_delivery(void) {
  cui_widget *chat=message;
  char tooltip[]="Sending";
  cui_chat_message m={.id=88,.author="Alice",.selected_option=-1,.delivery=CUI_CHAT_SENDING,.delivery_label=tooltip};
  assert(cui_chat_set_messages(chat,&m,1)); tooltip[0]='X';
  assert(!strcmp(cui__chat(chat)->messages[0].delivery_label,"Sending"));
  for(int layout=0;layout<3;++layout) {
    cui_chat_presentation p; assert(cui_chat_presentation_get(chat,&p)); p.messages=layout;
    assert(cui_chat_set_presentation(chat,&p));
    for(int state=CUI_CHAT_SENDING;state<=CUI_CHAT_SEND_FAILED;++state) {
      m.delivery=state; assert(cui_chat_set_messages(chat,&m,1)); assert(cui_chat_refresh(chat,1));
      cui_chat_event a={.action=CUI_CHAT_DELIVERY,.id=88};
      unsigned region=cui_chat_action_region(chat,&a); assert(region);
      assert(cui_canvas_activate_region(cui_chat_part(chat,0),region));
      cui_chat_event e; assert(cui_chat_event_get(chat,&e)); assert(e.action==CUI_CHAT_DELIVERY&&e.id==88);
    }
  }
  cui_chat_room reader={.id=1,.title="Bob"}; m.read_by=&reader; m.read_by_count=1;
  assert(cui_chat_set_messages(chat,&m,1)); assert(cui_chat_refresh(chat,1));
  cui_chat_event a={.action=CUI_CHAT_DELIVERY,.id=88}; assert(!cui_chat_action_region(chat,&a));
  m.delivery=99; assert(!cui_chat_set_messages(chat,&m,1));
}
static void check_reply_navigation(void) {
  cui_widget *chat=message;
  cui_chat_span span={"Only the reply body","",CUI_CHAT_BODY};
  cui_chat_message m={.id=88,.author="Bob",.spans=&span,.span_count=1,
    .reply_id=42,.reply_author="Alice",.reply_text="Original",.selected_option=-1};
  assert(cui_chat_set_messages(chat,&m,1));
  chat_state *s=cui__chat(chat);
  for(int layout=0;layout<3;++layout) {
    s->presentation.messages=(cui_chat_layout_style)layout;
    s->width=360; s->height=500; assert(cui__chat_layout(s)); s->offset=0;
    assert(cui_chat_refresh(chat,1));
    cui_chat_event action={.action=CUI_CHAT_OPEN_REPLY,.id=88,.detail_id=42};
    unsigned region=cui_chat_action_region(chat,&action); assert(region);
    assert(cui_canvas_activate_region(cui_chat_part(chat,0),region));
    cui_chat_event result; assert(cui_chat_event_get(chat,&result));
    assert(result.action==CUI_CHAT_OPEN_REPLY&&result.id==88&&result.detail_id==42);
  }
  m.reply_id=0; assert(cui_chat_set_messages(chat,&m,1));
  assert(cui_chat_refresh(chat,1));
  cui_chat_event action={.action=CUI_CHAT_OPEN_REPLY,.id=88};
  assert(!cui_chat_action_region(chat,&action));
}
static void check_readers(void) {
  cui_widget *chat=cui_chat_create(parent,CUI_CHAT_TIMELINE,CUI_CHAT_DAYLIGHT);
  unsigned char pixel[4]={220,45,80,255};
  cui_icon_asset *image=cui_icon_rgba(pixel,1,1); assert(image);
  char name[]="Bob";
  cui_chat_room readers[4]={
    {.id=1,.title=name,.detail="Read by Bob",.trailing="@bob:local",.avatar=image},
    {.id=2,.title="Carol",.detail="Read by Carol",.avatar_color=0x55aabbff},
    {.id=3,.title="Dana",.avatar_color=0x88aaffff},
    {.id=4,.title="Erin",.avatar_color=0xff8844ff}};
  cui_chat_span span={"Message with readers","",CUI_CHAT_BODY};
  cui_chat_message m={.id=88,.author="Alice",.spans=&span,.span_count=1,.selected_option=-1,.read_by=readers,.read_by_count=4};
  assert(cui_chat_set_messages(chat,&m,1)); cui_icon_release(image); name[0]='X';
  chat_state *s=cui__chat(chat);
  assert(!strcmp(s->messages[0].read_by[0].title,"Bob"));
  for(int layout=0;layout<3;++layout) {
    s->presentation.messages=(cui_chat_layout_style)layout;
    s->width=360; s->height=500; assert(cui__chat_layout(s)); s->offset=0;
    cui__chat_paint(s);
    int faces=0, overflow=0;
    for(size_t i=0;i<s->scene.region_count;++i) {
      cui_canvas_region r=s->scene.regions[i];
      cui_chat_event e=s->scene.actions[i];
      if(e.action==CUI_CHAT_OPEN_PROFILE&&e.detail_id) {
        assert(r.x>=40&&r.x+r.width<=180&&r.y+r.height<=s->tops[0]+s->heights[0]);
        if(e.detail_id==1) assert(!strcmp(e.text,"@bob:local"));
        ++faces;
      }
      if(r.id%256==131) ++overflow;
    }
    assert(faces==3&&overflow==1);
  }
  m.read_by_count=129; assert(!cui_chat_set_messages(chat,&m,1));
  assert(s->messages[0].read_by_count==4);
  m.read_by_count=0; m.read_by=NULL; assert(cui_chat_set_messages(chat,&m,1));
}
static void verify(void *data) {
  check_readers();
  (void)data;
  check_font(); check_hover(); check_focus_contents(); check_single_line_alignment();
  check_toolbar_focus();
  check_timeline_hover();
  check_room_scroll_and_avatar();
  check_links_and_profile();
  check_dark_unread();
  check_avatar_rail();
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
  check_reply_navigation();
  check_delivery();
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
