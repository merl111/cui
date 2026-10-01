#include "cui_desktop_internal.h"
#include <gtk/gtk.h>
#include <stdlib.h>
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static char **selected_paths(GtkFileChooser *chooser,size_t *count)
{
    GListModel *files=gtk_file_chooser_get_files(chooser);*count=g_list_model_get_n_items(files);
    char **paths=*count&&*count<=65536?calloc(*count,sizeof(*paths)):NULL;
    if(paths)for(size_t i=0;i<*count;++i){
        GFile *file=g_list_model_get_item(files,(guint)i);char *path=g_file_get_path(file);
        paths[i]=path?g_filename_to_utf8(path,-1,NULL,NULL,NULL):NULL;
        g_free(path);g_object_unref(file);
    }
    g_object_unref(files);return paths;
}
static size_t selected_filter(GtkFileChooser *chooser)
{
    GtkFileFilter *filter=gtk_file_chooser_get_filter(chooser);
    size_t encoded=filter?GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(filter),"cui-filter-index")):0;
    return encoded?encoded-1:(size_t)-1;
}
static void file_response(GtkNativeDialog *native,int response,gpointer data)
{
    cui_dialog *d=data;GtkFileChooser *chooser=GTK_FILE_CHOOSER(native);size_t count=0;
    cui_dialog_result result=response==GTK_RESPONSE_ACCEPT?CUI_DIALOG_ACCEPTED:CUI_DIALOG_CANCELLED;
    char **paths=result==CUI_DIALOG_ACCEPTED?selected_paths(chooser,&count):NULL;
    size_t index=selected_filter(chooser);
    g_signal_handlers_disconnect_by_data(native,d);gtk_native_dialog_destroy(native);g_object_unref(native);d->native=NULL;
    cui__file_finish(d,result,(const char *const *)paths,count,index);
    if(paths){for(size_t i=0;i<count;++i)g_free(paths[i]);free(paths);}
}
static void filters(GtkFileChooser *chooser,cui_file_settings *settings)
{
    if(!settings)return;
    gtk_file_chooser_set_select_multiple(chooser,settings->multiple);
    for(size_t i=0;i<settings->count;++i){
        cui_file_type *type=settings->filters+i;GtkFileFilter *filter=gtk_file_filter_new();
        gtk_file_filter_set_name(filter,type->name);
        if(!type->count)gtk_file_filter_add_pattern(filter,"*");
        for(size_t j=0;j<type->count;++j)gtk_file_filter_add_suffix(filter,type->extensions[j]);
        g_object_set_data(G_OBJECT(filter),"cui-filter-index",GSIZE_TO_POINTER(i+1));
        gtk_file_chooser_add_filter(chooser,filter);
        if(i==settings->initial)gtk_file_chooser_set_filter(chooser,filter);
        g_object_unref(filter);
    }
}
static void initial_path(GtkFileChooser *chooser,cui_dialog *d)
{
    if(!*d->path)return;
    char *path=g_filename_from_utf8(d->path,-1,NULL,NULL,NULL);if(!path)return;
    GFile *file=g_file_new_for_path(path);g_free(path);
    if(g_file_query_file_type(file,G_FILE_QUERY_INFO_NONE,NULL)==G_FILE_TYPE_DIRECTORY)
        gtk_file_chooser_set_current_folder(chooser,file,NULL);
    else if(d->kind==CUI_DIALOG_SAVE){
        GFile *folder=g_file_get_parent(file);char *base=g_file_get_basename(file);
        char *name=base?g_filename_to_utf8(base,-1,NULL,NULL,NULL):NULL;
        if(folder){gtk_file_chooser_set_current_folder(chooser,folder,NULL);g_object_unref(folder);}
        if(name)gtk_file_chooser_set_current_name(chooser,name);
        g_free(name);g_free(base);
    }else gtk_file_chooser_set_file(chooser,file,NULL);
    g_object_unref(file);
}
void cui__file_open_native(cui_dialog *d)
{
    GtkFileChooserAction action=d->kind==CUI_DIALOG_SAVE?GTK_FILE_CHOOSER_ACTION_SAVE:d->kind==CUI_DIALOG_FOLDER?GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:GTK_FILE_CHOOSER_ACTION_OPEN;
    GtkFileChooserNative *native=gtk_file_chooser_native_new(d->title,GTK_WINDOW(d->parent->native),action,NULL,NULL);
    filters(GTK_FILE_CHOOSER(native),d->files);initial_path(GTK_FILE_CHOOSER(native),d);
    d->native=native;g_signal_connect(native,"response",G_CALLBACK(file_response),d);gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));
}
G_GNUC_END_IGNORE_DEPRECATIONS
