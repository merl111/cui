#include "cui_desktop_internal.h"
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#define CHECK(x) do{if(!(x)){fprintf(stderr,"files:%d: %s (phase %d)\n",__LINE__,#x,phase);exit(1);}}while(0)
static cui_app *app;static cui_window *window;static cui_dialog *dialog,*accepted;
static int phase,ticks,calls;static char *directory,*alpha,*beta,*image_file,*folder_a,*folder_b,*saved;
static GtkWidget *chooser_window(void)
{
    GListModel *windows=gtk_window_get_toplevels();
    for(guint i=0;i<g_list_model_get_n_items(windows);++i){GtkWidget *widget=g_list_model_get_item(windows,i);int match=GTK_IS_FILE_CHOOSER(widget)&&GTK_IS_DIALOG(widget);g_object_unref(widget);if(match)return widget;}
    return NULL;
}
static GtkColumnView *file_view(GtkWidget *widget)
{
    if(GTK_IS_COLUMN_VIEW(widget)){
        GtkSelectionModel *model=gtk_column_view_get_model(GTK_COLUMN_VIEW(widget));
        if(g_list_model_get_n_items(G_LIST_MODEL(model))){GObject *item=g_list_model_get_item(G_LIST_MODEL(model),0);int match=G_IS_FILE_INFO(item);g_object_unref(item);if(match)return GTK_COLUMN_VIEW(widget);}
    }
    for(GtkWidget *child=gtk_widget_get_first_child(widget);child;child=gtk_widget_get_next_sibling(child)){GtkColumnView *found=file_view(child);if(found)return found;}
    return NULL;
}
static int matches_name(const char *name,const char *first,const char *second)
{return name&&(!strcmp(name,first)||(second&&!strcmp(name,second)));}
static int tree_row_matches(GtkTreeModel *model,GtkTreeIter *iter,const char *first,const char *second)
{
    int columns=gtk_tree_model_get_n_columns(model);
    for(int column=0;column<columns;++column){
        GValue value=G_VALUE_INIT;gtk_tree_model_get_value(model,iter,column,&value);int match=0;
        if(G_VALUE_HOLDS_STRING(&value))match=matches_name(g_value_get_string(&value),first,second);
        else if(G_VALUE_HOLDS_OBJECT(&value)){
            GObject *item=g_value_get_object(&value);
            if(G_IS_FILE_INFO(item))match=matches_name(g_file_info_get_name(G_FILE_INFO(item)),first,second);
            else if(G_IS_FILE(item)){char *name=g_file_get_basename(G_FILE(item));match=matches_name(name,first,second);g_free(name);}
        }
        g_value_unset(&value);if(match)return 1;
    }
    return 0;
}
static int choose_tree(GtkWidget *widget,const char *first,const char *second)
{
    if(GTK_IS_TREE_VIEW(widget)){
        GtkTreeModel *model=gtk_tree_view_get_model(GTK_TREE_VIEW(widget));GtkTreeIter iter;int found=0;
        GtkTreeSelection *selection=gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));gtk_tree_selection_unselect_all(selection);
        if(model&&gtk_tree_model_get_iter_first(model,&iter))do{
            if(tree_row_matches(model,&iter,first,second)){gtk_tree_selection_select_iter(selection,&iter);++found;}
        }while(gtk_tree_model_iter_next(model,&iter));
        if(found==(second?2:1))return 1;
    }
    for(GtkWidget *child=gtk_widget_get_first_child(widget);child;child=gtk_widget_get_next_sibling(child))if(choose_tree(child,first,second))return 1;
    return 0;
}
static int choose_names(GtkWidget *chooser,const char *first,const char *second)
{
    GtkColumnView *view=file_view(chooser);if(!view)return choose_tree(chooser,first,second);
    GtkSelectionModel *selection=gtk_column_view_get_model(view);guint n=g_list_model_get_n_items(G_LIST_MODEL(selection)),found=0;
    gtk_selection_model_unselect_all(selection);
    for(guint i=0;i<n;++i){GFileInfo *info=g_list_model_get_item(G_LIST_MODEL(selection),i);const char *name=g_file_info_get_name(info);
        if(name&&(!strcmp(name,first)||(second&&!strcmp(name,second)))){gtk_selection_model_select_item(selection,i,FALSE);++found;}
        g_object_unref(info);
    }
    return found==(second?2u:1u);
}
static void completed(cui_dialog *d,cui_dialog_result result,const char *path,void *data)
{
    (void)data;++calls;size_t filter=99;
    if(phase==1){
        CHECK(result==CUI_DIALOG_ACCEPTED);CHECK(cui_dialog_path_count(d)==2);CHECK(path==cui_dialog_path(d,0));
        const char *a=cui_dialog_path(d,0),*b=cui_dialog_path(d,1);
        CHECK((!strcmp(a,alpha)&&!strcmp(b,beta))||(!strcmp(a,beta)&&!strcmp(b,alpha)));
        CHECK(!cui_dialog_path(d,2));CHECK(cui_dialog_filter(d,&filter)&&filter==0);accepted=d;
    }else if(phase==3){CHECK(result==CUI_DIALOG_ACCEPTED&&cui_dialog_path_count(d)==1);CHECK(!strcmp(path,image_file));CHECK(cui_dialog_filter(d,&filter)&&filter==1);}
    else if(phase==5){
        CHECK(result==CUI_DIALOG_ACCEPTED&&cui_dialog_path_count(d)==2);CHECK(!cui_dialog_filter(d,&filter)&&filter==99);
        const char *a=cui_dialog_path(d,0),*b=cui_dialog_path(d,1);
        CHECK((!strcmp(a,folder_a)&&!strcmp(b,folder_b))||(!strcmp(a,folder_b)&&!strcmp(b,folder_a)));
    }else if(phase==7){CHECK(result==CUI_DIALOG_ACCEPTED&&cui_dialog_path_count(d)==1);CHECK(!strcmp(path,saved));CHECK(!g_file_test(saved,G_FILE_TEST_EXISTS));}
    else{CHECK(result==CUI_DIALOG_CANCELLED);CHECK(!*path&&!cui_dialog_path_count(d)&&!cui_dialog_path(d,0));CHECK(!cui_dialog_filter(d,&filter)&&filter==99);}
}
static cui_dialog *open_dialog(cui_dialog_kind kind,int multiple,size_t selected)
{
    char label[]="Documents";const char *documents[]={"txt","md"},*images[]={"png"};
    cui_file_filter filters[]={{label,documents,2},{"Images",images,1},{"All files",NULL,0}};
    cui_file_options options={directory,filters,3,selected,multiple};if(kind==CUI_DIALOG_FOLDER){options.filters=NULL;options.filter_count=0;}
    cui_dialog *d=cui_file_dialog_ex(window,kind,"File chooser integration",&options,completed,NULL);label[0]='X';return d;
}
static void tick(void *data)
{
    (void)data;CHECK(++ticks<180);
    if(phase==0){dialog=open_dialog(CUI_DIALOG_OPEN,1,0);CHECK(dialog);CHECK(!cui_dialog_path_count(dialog));phase=1;return;}
    if(phase==1||phase==3||phase==5){
        if(dialog->finished){++phase;return;}
        GtkWidget *chooser=chooser_window();if(!chooser||!dialog->native)return;
        if(phase==1){
            CHECK(gtk_file_chooser_get_select_multiple(GTK_FILE_CHOOSER(dialog->native)));
            GListModel *filters=gtk_file_chooser_get_filters(GTK_FILE_CHOOSER(dialog->native));CHECK(g_list_model_get_n_items(filters)==3);g_object_unref(filters);
            GtkFileFilter *selected=gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(dialog->native));CHECK(!strcmp(gtk_file_filter_get_name(selected),"Documents"));
            GFileInfo *info=g_file_info_new();g_file_info_set_display_name(info,"picture.png");CHECK(!gtk_filter_match(GTK_FILTER(selected),info));
            g_file_info_set_display_name(info,"βeta.MD");CHECK(gtk_filter_match(GTK_FILTER(selected),info));g_object_unref(info);
            if(!choose_names(chooser,"alpha.txt","βeta.MD"))return;
        }else if(phase==3){
            CHECK(!gtk_file_chooser_get_select_multiple(GTK_FILE_CHOOSER(dialog->native)));
            GListModel *filters=gtk_file_chooser_get_filters(GTK_FILE_CHOOSER(dialog->native));GtkFileFilter *image=g_list_model_get_item(filters,1);
            if(image!=gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(dialog->native)))gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog->native),image);
            g_object_unref(image);g_object_unref(filters);
            if(!choose_names(chooser,"picture.png",NULL))return;
        }else if(!choose_names(chooser,"Folder A","資料"))return;
        gtk_dialog_response(GTK_DIALOG(chooser),GTK_RESPONSE_ACCEPT);if(!dialog->finished)return;
        ++phase;return;
    }
    if(phase==2){dialog=open_dialog(CUI_DIALOG_OPEN,0,0);CHECK(dialog);phase=3;return;}
    if(phase==4){dialog=open_dialog(CUI_DIALOG_FOLDER,1,0);CHECK(dialog);phase=5;return;}
    if(phase==6){
        cui_file_options options={saved,NULL,0,0,0};
        dialog=cui_file_dialog_ex(window,CUI_DIALOG_SAVE,"Save destination",&options,completed,NULL);
        CHECK(dialog);phase=7;return;
    }
    if(phase==7){
        if(!dialog->finished){
            GtkWidget *chooser=chooser_window();if(!chooser||!dialog->native)return;
            char *name=gtk_file_chooser_get_current_name(GTK_FILE_CHOOSER(chooser));
            CHECK(name&&!strcmp(name,"draft-資料.md"));g_free(name);
            gtk_dialog_response(GTK_DIALOG(chooser),GTK_RESPONSE_ACCEPT);
            if(!dialog->finished)return;
        }
        CHECK(calls==4);
        dialog=open_dialog(CUI_DIALOG_OPEN,1,0);CHECK(dialog);cui_dialog_cancel(dialog);phase=8;return;
    }
    if(phase==8){
        if(!dialog->finished)return;CHECK(calls==5);CHECK(cui_dialog_path_count(accepted)==2);CHECK(cui_dialog_path(accepted,0));
        cui_dialog_cancel(accepted);CHECK(calls==5);
        dialog=open_dialog(CUI_DIALOG_OPEN,1,0);CHECK(dialog);phase=9;return;
    }
    if(dialog->native)cui_app_quit(app); /* Destruction must suppress an unfinished result. */
}
int main(void)
{
    directory=g_dir_make_tmp("cui-files-XXXXXX",NULL);CHECK(directory);
    alpha=g_build_filename(directory,"alpha.txt",NULL);beta=g_build_filename(directory,"βeta.MD",NULL);image_file=g_build_filename(directory,"picture.png",NULL);
    folder_a=g_build_filename(directory,"Folder A",NULL);folder_b=g_build_filename(directory,"資料",NULL);
    CHECK(g_file_set_contents(alpha,"a",-1,NULL)&&g_file_set_contents(beta,"b",-1,NULL)&&g_file_set_contents(image_file,"not image data",-1,NULL));CHECK(!g_mkdir(folder_a,0700)&&!g_mkdir(folder_b,0700));
    saved=g_build_filename(directory,"draft-資料.md",NULL);
    app=cui_app_create();CHECK(app);window=cui_window_create(app,"File integration",600,400);CHECK(window);cui_label(cui_window_root(window),"Native file filters and selections");
    const char *bad[]={"*.txt"};cui_file_filter filter={"Invalid",bad,1};cui_file_options options={directory,&filter,1,0,0};
    CHECK(!cui_file_dialog_ex(window,CUI_DIALOG_OPEN,"Invalid",&options,completed,NULL));
    options.filters=NULL;options.filter_count=0;options.multiple=1;CHECK(!cui_file_dialog_ex(window,CUI_DIALOG_SAVE,"Invalid",&options,completed,NULL));
    options.multiple=0;options.initial_filter=1;CHECK(!cui_file_dialog_ex(window,CUI_DIALOG_OPEN,"Invalid",&options,completed,NULL));
    CHECK(!cui_file_dialog_ex(window,CUI_DIALOG_COLOR,"Invalid",NULL,completed,NULL));
    CHECK(cui_every(app,100,tick,NULL));cui_window_show(window);cui_app_run(app);cui_app_destroy(app);CHECK(calls==5);
    g_remove(alpha);g_remove(beta);g_remove(image_file);g_rmdir(folder_a);g_rmdir(folder_b);g_rmdir(directory);
    g_free(alpha);g_free(beta);g_free(image_file);g_free(folder_a);g_free(folder_b);g_free(saved);g_free(directory);
    puts("files: native filters, multi-file/folder selection, Unicode paths, cancellation and retained results passed");return 0;
}
G_GNUC_END_IGNORE_DEPRECATIONS
