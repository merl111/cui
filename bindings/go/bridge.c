#include "bridge.h"
extern void cuiGoAction(uintptr_t handle);
static void action(cui_widget *sender, void *data) { (void)sender; cuiGoAction((uintptr_t)data); }
static void task(void *data) { cuiGoAction((uintptr_t)data); }
void cui_go_connect(cui_widget *widget, uintptr_t handle) { cui_on_action(widget, action, (void *)handle); }
cui_timer *cui_go_every(cui_app *app, unsigned interval, uintptr_t handle) { return cui_every(app, interval, task, (void *)handle); }

extern void cuiGoDialog(uintptr_t handle, int result, char *path);
static void dialog_result(cui_dialog *dialog, cui_dialog_result result, const char *path, void *data)
{(void)dialog;cuiGoDialog((uintptr_t)data,(int)result,(char *)path);}
cui_command *cui_go_command(cui_app *app,const char *label,unsigned key,unsigned modifiers,uintptr_t handle)
{return cui_command_create(app,label,key,modifiers,task,(void *)handle);}
cui_dialog *cui_go_file_dialog(cui_window *window,cui_dialog_kind kind,const char *title,const char *initial,uintptr_t handle)
{return cui_file_dialog(window,kind,title,initial,dialog_result,(void *)handle);}
cui_dialog *cui_go_alert(cui_window *window,const char *title,const char *message,const char *accept,uintptr_t handle)
{return cui_alert(window,title,message,accept,dialog_result,(void *)handle);}
cui_dialog *cui_go_color_dialog(cui_window *window,const char *title,unsigned rgb,uintptr_t handle)
{return cui_color_dialog(window,title,rgb,dialog_result,(void *)handle);}
cui_dialog *cui_go_font_dialog(cui_window *window,const char *title,const cui_font_value *font,uintptr_t handle)
{return cui_font_dialog(window,title,font,dialog_result,(void *)handle);}
extern int cuiGoKey(uintptr_t handle, int key, unsigned modifiers);
static int key_action(cui_widget *sender,cui_key key,unsigned modifiers,void *data)
{(void)sender;return cuiGoKey((uintptr_t)data,(int)key,modifiers);}
int cui_go_key(cui_widget *widget,uintptr_t handle)
{return cui_on_key(widget,handle?key_action:NULL,(void *)handle);}
cui_dialog *cui_go_file_dialog_ex(cui_window *window,cui_dialog_kind kind,const char *title,const cui_file_options *options,uintptr_t handle)
{return cui_file_dialog_ex(window,kind,title,options,dialog_result,(void *)handle);}
extern void cuiGoCanvas(uintptr_t,cui_canvas_event *);
static void canvas_event(cui_widget *w,const cui_canvas_event *event,void *data)
{(void)w;cuiGoCanvas((uintptr_t)data,(cui_canvas_event *)event);}
void cui_go_canvas(cui_widget *w,uintptr_t handle)
{cui_canvas_on_event(w,handle?canvas_event:NULL,(void *)handle);}
