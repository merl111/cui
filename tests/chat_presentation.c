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
static void verify(void *data) {
  (void)data;
  check_font(); check_hover(); check_focus_contents(); check_single_line_alignment();
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
