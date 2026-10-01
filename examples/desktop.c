#include "cui_desktop.h"
#include "cui_layouts.h"
#include "cui_navigation.h"
#include "cui_inputs.h"
#include "cui_tables.h"
#include "cui_search.h"
#include "cui_feedback.h"
#include "cui_tokens.h"
#include <stdio.h>
#include <stdlib.h>
static cui_app *app;static cui_window *window;static cui_widget *status,*editor,*breadcrumbs,*navigation,*font_size_control;
static cui_widget *command_palette,*notification,*palette_button,*labels,*empty_labels,*label_error;
static const cui_tree_item nodes[]={{1,0,"CUI",1},{2,1,"Sources",1},{3,2,"Application.c",0},{4,2,"Layout.c",0},{5,1,"Examples",1},{6,5,"Python",0},{7,5,"Go",0},{8,5,"Zig",0}};
static cui_font_value selected_font={"Sans",12,400,0};
static unsigned selected_color=0x5266df;
static void picker_result(cui_dialog *dialog,cui_dialog_result result,const char *path,void *data)
{
    (void)path;(void)data;
    if(result!=CUI_DIALOG_ACCEPTED){cui_set_text(status,result==CUI_DIALOG_CANCELLED?"Selection cancelled":"Picker failed");return;}
    if(cui_dialog_font(dialog,&selected_font)){
        cui_font_apply(editor,&selected_font);cui_number_set(font_size_control,selected_font.points);cui_set_text(status,"Document font updated");
    }else if(cui_dialog_color(dialog,&selected_color)){
        char text[64];snprintf(text,sizeof(text),"Selected color: #%06X",selected_color);cui_set_text(status,text);
    }
}
static void choose_font(void *data){(void)data;cui_font_dialog(window,"Document font",&selected_font,picker_result,NULL);}
static void choose_color(void *data){(void)data;cui_color_dialog(window,"Accent color",selected_color,picker_result,NULL);}
static void chosen(cui_dialog *dialog,cui_dialog_result result,const char *path,void *data)
{
    (void)data;
    if(result!=CUI_DIALOG_ACCEPTED){cui_set_text(status,result==CUI_DIALOG_CANCELLED?"Cancelled":"Dialog failed");return;}
    if(cui_dialog_path_count(dialog)>1){char text[256];snprintf(text,sizeof(text),"Selected %zu paths. First: %s",cui_dialog_path_count(dialog),path);cui_set_text(status,text);}
    else cui_set_text(status,*path?path:"Confirmed");
}
static void show_file_dialog(cui_dialog_kind kind,const char *title,const char *initial,int multiple)
{
    const char *documents[]={"txt","md","c","h"},*images[]={"png","jpg","jpeg"};
    const cui_file_filter filters[]={{"Documents and source",documents,4},{"Images",images,3},{"All files",NULL,0}};
    cui_file_options options={initial,filters,3,0,multiple};if(kind==CUI_DIALOG_FOLDER){options.filters=NULL;options.filter_count=0;}
    cui_file_dialog_ex(window,kind,title,&options,chosen,NULL);
}
static void open_file(void *data){(void)data;show_file_dialog(CUI_DIALOG_OPEN,"Open files",NULL,1);}
static void save_file(void *data){(void)data;show_file_dialog(CUI_DIALOG_SAVE,"Choose a destination","Untitled.txt",0);}
static void folder(void *data){(void)data;show_file_dialog(CUI_DIALOG_FOLDER,"Choose folders",NULL,1);}
static void alert(void *data){(void)data;cui_alert(window,"Ready to continue?","This native confirmation does not modify files.","Continue",chosen,NULL);}
static void undo(void *data){(void)data;cui_undo(editor);}
static void redo(void *data){(void)data;cui_redo(editor);}
static void quit(void *data){(void)data;cui_app_quit(app);}
static void navigate(cui_widget *tree,void *data)
{
    (void)data;cui_item_id id=cui_tree_selected(tree);if(!id)return;
    cui_breadcrumb_item reversed[8],path[8];size_t count=0;
    while(id && count<8){const cui_tree_item *node=&nodes[id-1];reversed[count++]=(cui_breadcrumb_item){node->id,node->text};id=node->parent;}
    for(size_t i=0;i<count;++i)path[i]=reversed[count-i-1];
    cui_breadcrumbs_set_items(breadcrumbs,path,count);cui_set_text(status,path[count-1].text);
}
static void breadcrumb_clicked(cui_widget *sender,void *data)
{ (void)data;cui_tree_select(navigation,cui_breadcrumbs_activated(sender));navigate(navigation,NULL); }
static void validate_name(cui_widget *field,void *data)
{(void)data;cui_field_set_error(field,cui_get_text(cui_field_entry(field),NULL,0)?"":"A document name is required.");}
static void font_size(cui_widget *number,void *data)
{(void)data;selected_font.points=cui_number_get(number);cui_font_apply(editor,&selected_font);}
static void open_palette(cui_widget *sender,void *data)
{(void)data;cui_picker_set_query(command_palette,"");cui_picker_open(command_palette,sender);}
static void command_selected(cui_widget *sender,void *data)
{
    (void)data;if(cui_picker_last_event(sender)!=CUI_PICKER_SELECT)return;
    cui_item_id id=cui_picker_selected(sender);
    if(id==1)open_file(NULL);
    else if(id==2)choose_font(NULL);
    else if(id==3)cui_app_set_theme(app,CUI_THEME_LIGHT);
    else if(id==4)cui_app_set_theme(app,CUI_THEME_DARK);
    else cui_app_set_theme(app,CUI_THEME_SYSTEM);
    cui_feedback_show(notification,"Command selected","Use Search commands to choose another action.",CUI_ROLE_SUCCESS,"Commands",3500);
}
static void notification_action(cui_widget *sender,void *data)
{(void)data;if(cui_feedback_last_event(sender)==CUI_FEEDBACK_ACTION)open_palette(palette_button,NULL);}
static void suggestion_selected(cui_widget *sender,void *data)
{
    if(cui_picker_last_event(sender)!=CUI_PICKER_SELECT&&cui_picker_last_event(sender)!=CUI_PICKER_SUBMIT)return;
    char text[256];cui_picker_get_query(sender,text,sizeof(text));cui_set_text(data,text);
}
static void labels_changed(cui_widget *sender,void *data)
{
    (void)data;cui_tokens_event event=cui_tokens_last_event(sender);
    if(event==CUI_TOKENS_QUERY)return;
    if(event==CUI_TOKENS_SUBMIT){cui_feedback_show(label_error,"Choose an existing label","Type Design, Code or Review to select a label.",CUI_ROLE_WARNING,"Use Design",0);return;}
    cui_feedback_dismiss(empty_labels);
    if(cui_tokens_get_selected(sender,NULL,0))cui_feedback_dismiss(label_error);
    else cui_feedback_show(label_error,"A label is required","Add a label before continuing.",CUI_ROLE_DANGER,"Use Design",0);
}
static void add_default_label(cui_widget *sender,void *data)
{(void)data;if(cui_feedback_last_event(sender)==CUI_FEEDBACK_ACTION)cui_tokens_add(labels,11);}
static int run(void)
{
    app=cui_app_create();if(!app)return 1;window=cui_window_create(app,"CUI · Desktop controls",960,760);if(!window){cui_app_destroy(app);return 1;}
    cui_window_set_scrollable(window,1);
    cui_command *open=cui_command_create(app,"Open…",'O',CUI_MOD_PRIMARY,open_file,NULL);
    cui_command *save=cui_command_create(app,"Save as…",'S',CUI_MOD_PRIMARY|CUI_MOD_SHIFT,save_file,NULL);
    cui_command *choose=cui_command_create(app,"Choose folder…",0,0,folder,NULL);
    cui_command *confirm=cui_command_create(app,"Confirm…",0,0,alert,NULL);
    cui_command *exit=cui_command_create(app,"Quit",'Q',CUI_MOD_PRIMARY,quit,NULL);
    cui_command *undo_command=cui_command_create(app,"Undo",'Z',CUI_MOD_PRIMARY,undo,NULL);
    cui_command *redo_command=cui_command_create(app,"Redo",'Z',CUI_MOD_PRIMARY|CUI_MOD_SHIFT,redo,NULL);
    cui_menu *bar=cui_menu_create(app),*file=cui_menu_create(app),*edit=cui_menu_create(app);
    cui_menu_add(file,open);cui_menu_add(file,save);cui_menu_add(file,choose);cui_menu_add_separator(file);cui_menu_add(file,exit);
    cui_menu_add(edit,undo_command);cui_menu_add(edit,redo_command);cui_menu_add_submenu(bar,"File",file);cui_menu_add_submenu(bar,"Edit",edit);cui_window_set_menu(window,bar);
    cui_widget *root=cui_window_root(window);cui_set_role(cui_label(root,"Desktop essentials"),CUI_ROLE_TITLE);
    cui_label(root,"Native dialogs, shared commands, shortcuts and text editing.");
    cui_command *actions[]={open,save,choose,confirm};cui_toolbar(root,actions,4);
    cui_widget *split=cui_split(root,CUI_HORIZONTAL,0.3);
    cui_box_set_padding(cui_split_pane(split,0),8);cui_box_set_padding(cui_split_pane(split,1),12);
    cui_widget *grid=cui_grid(cui_split_pane(split,0),2,10);
    cui_set_role(cui_label(cui_grid_cell(grid,0,0,1,2),"Workspace"),CUI_ROLE_HEADING);
    cui_label(cui_grid_cell(grid,1,0,1,1),"Project");cui_entry(cui_grid_cell(grid,1,1,1,1),"CUI");
    cui_widget *wrap=cui_wrap(cui_split_pane(split,0),6);cui_badge(wrap,"Native",CUI_ROLE_SUCCESS);cui_badge(wrap,"C ABI",CUI_ROLE_SUBTLE);cui_badge(wrap,"High DPI",CUI_ROLE_SUBTLE);
    cui_widget *tree=cui_tree(cui_split_pane(split,0),nodes,8);cui_expand(tree,1);cui_on_action(tree,navigate,NULL);navigation=tree;
    const cui_breadcrumb_item path[]={{1,"CUI"},{2,"Sources"},{3,"Application.c"}};
    breadcrumbs=cui_breadcrumbs(cui_split_pane(split,1),path,3);cui_on_action(breadcrumbs,breadcrumb_clicked,NULL);cui_tree_select(tree,3);
    cui_widget *pages=cui_tabs(cui_split_pane(split,1));cui_expand(pages,1);
    cui_widget *document=cui_tab_add(pages,"Document");
    cui_widget *field=cui_field(document,"Document name","Untitled","Choose a name for the document.");
    cui_widget *schedule=cui_box(document,CUI_HORIZONTAL,10);
    cui_widget *date_box=cui_box(schedule,CUI_VERTICAL,4),*time_box=cui_box(schedule,CUI_VERTICAL,4);
    cui_label(date_box,"Date");cui_date(date_box,(cui_date_value){2026,9,30});
    cui_label(time_box,"Time");cui_time_input(time_box,(cui_time_value){14,30,0});
    cui_command *pickers[]={cui_command_create(app,"Choose font…",0,0,choose_font,NULL),cui_command_create(app,"Choose color…",0,0,choose_color,NULL)};
    cui_toolbar(document,pickers,2);
    cui_label(document,"Font size");cui_widget *size=cui_number(document,12,6,200,0.5,1);font_size_control=size;
    editor=cui_textarea(document,"Edit this text, then try Undo and Redo from the menu or keyboard.");cui_expand(editor,1);
    cui_on_action(field,validate_name,NULL);cui_on_action(size,font_size,NULL);
    cui_accessibility(editor,"Document","Editable document with native undo and redo");
    cui_widget *data=cui_tab_add(pages,"Data");
    cui_label(data,"Edit cells, select multiple rows, or click a heading to sort.");
    const char *headers[]={"Name","Count","Status"};const char *cells[]={"Design system","12","Ready","Native controls","28","In progress","Bindings","3","Ready"};
    cui_widget *table=cui_table(data,headers,3);cui_table_set_rows(table,cells,3);cui_table_set_multiple(table,1);cui_table_set_editable(table,0,1);cui_table_set_editable(table,1,1);cui_table_set_editable(table,2,1);cui_expand(table,1);
    cui_widget *interactions=cui_tab_add(pages,"Search & feedback");
    cui_widget *banner=cui_feedback(interactions,CUI_BANNER);
    cui_feedback_show(banner,"Your workspace","Search commands and try a document-name suggestion.",CUI_ROLE_SUBTLE,"",0);
    cui_widget *suggestions=cui_picker(interactions,CUI_AUTOCOMPLETE,"Document name…");
    const cui_choice names[]={{1,"Design notes","Project planning","",0},{2,"Release checklist","Ship with confidence","",0},{3,"Meeting notes","Decisions and next steps","",0}};
    cui_picker_set_items(suggestions,names,3);cui_on_action(suggestions,suggestion_selected,cui_field_entry(field));
    palette_button=cui_button(interactions,"Search commands…");cui_on_action(palette_button,open_palette,NULL);
    command_palette=cui_picker(interactions,CUI_COMMAND_PALETTE,"Search by name or keyword…");
    const cui_choice commands[]={{1,"Open file…","Choose a document","file",0},{2,"Choose font…","Native font picker","text typography",0},{3,"Light appearance","Apply a light theme","theme",0},{4,"Dark appearance","Apply a dark theme","theme",0},{5,"System appearance","Follow the operating system","theme",0}};
    cui_picker_set_items(command_palette,commands,5);cui_on_action(command_palette,command_selected,NULL);
    notification=cui_feedback(interactions,CUI_TOAST);cui_on_action(notification,notification_action,NULL);
    cui_widget *label_page=cui_tab_add(pages,"Labels");
    cui_set_role(cui_label(label_page,"Project labels"),CUI_ROLE_HEADING);
    labels=cui_tokens(label_page,"Choose a label…",3);
    const cui_choice tags[]={{11,"Design","Interface and experience","ux",0},{12,"Code","Implementation","development",0},{13,"Review","Quality and feedback","testing",0}};
    cui_tokens_set_items(labels,tags,3);cui_on_action(labels,labels_changed,NULL);
    empty_labels=cui_feedback(label_page,CUI_EMPTY_STATE);label_error=cui_feedback(label_page,CUI_ERROR_STATE);
    cui_feedback_show(empty_labels,"No labels yet","Choose labels to organize this project.",CUI_ROLE_SUBTLE,"Add Design",0);
    cui_on_action(empty_labels,add_default_label,NULL);cui_on_action(label_error,add_default_label,NULL);
    status=cui_label(root,"File dialogs select paths; this example does not read or write your files.");
    const char *page=getenv("CUI_DESKTOP_PAGE");if(page)cui_set_selected(pages,atoi(page));
    const char *text_scale=getenv("CUI_TEXT_SCALE");if(text_scale)cui_app_set_text_scale(app,atof(text_scale));
    cui_window_show(window);if(!page)cui_focus(editor);cui_app_run(app);cui_app_destroy(app);return 0;
}
#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE a,HINSTANCE b,LPSTR c,int d){(void)a;(void)b;(void)c;(void)d;return run();}
#else
int main(void){return run();}
#endif
