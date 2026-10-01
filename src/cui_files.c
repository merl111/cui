#include "cui_desktop_internal.h"
#include <stdlib.h>
#include <string.h>
static int extension_valid(const char *text)
{
    if(!text||!*text||strlen(text)>64||text[0]=='.')return 0;
    const unsigned char *p=(const unsigned char *)text;
    for(;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='_'||*p=='.'))return 0;
    return p[-1]!='.';
}
void cui__file_settings_free(cui_file_settings *s)
{
    if(!s)return;
    for(size_t i=0;i<s->count;++i){free(s->filters[i].name);cui__free_strings(s->filters[i].extensions,s->filters[i].count);}
    free(s->filters);free(s);
}
static int copy_filter(cui_file_type *out,const cui_file_filter *in)
{
    if(!in->name||!*in->name||strlen(in->name)>255||in->extension_count>32||(in->extension_count&&!in->extensions))return 0;
    out->name=cui__desktop_copy(in->name);if(!out->name)return 0;
    if(!in->extension_count)return 1;
    out->extensions=calloc(in->extension_count,sizeof(*out->extensions));if(!out->extensions)return 0;
    out->count=in->extension_count;
    for(size_t i=0;i<out->count;++i){
        if(!extension_valid(in->extensions[i]))return 0;
        out->extensions[i]=cui__desktop_copy(in->extensions[i]);if(!out->extensions[i])return 0;
    }
    return 1;
}
static int options_valid(cui_dialog_kind kind,const cui_file_options *options)
{
    if(options->filter_count>64||(options->filter_count&&!options->filters))return 0;
    if(options->filter_count?options->initial_filter>=options->filter_count:options->initial_filter!=0)return 0;
    if(kind==CUI_DIALOG_SAVE&&options->multiple)return 0;
    return kind!=CUI_DIALOG_FOLDER||!options->filter_count;
}
static cui_file_settings *copy_options(cui_dialog_kind kind,const cui_file_options *options)
{
    if(!options_valid(kind,options))return NULL;
    cui_file_settings *s=calloc(1,sizeof(*s));if(!s)return NULL;
    s->multiple=!!options->multiple;s->initial=options->initial_filter;
    if(options->filter_count){
        s->filters=calloc(options->filter_count,sizeof(*s->filters));if(!s->filters){free(s);return NULL;}
        s->count=options->filter_count;
        for(size_t i=0;i<s->count;++i)if(!copy_filter(s->filters+i,options->filters+i)){cui__file_settings_free(s);return NULL;}
    }
    return s;
}
cui_dialog *cui_file_dialog_ex(cui_window *parent,cui_dialog_kind kind,const char *title,const cui_file_options *options,cui_dialog_callback callback,void *data)
{
    if(kind<CUI_DIALOG_OPEN||kind>CUI_DIALOG_FOLDER)return NULL;
    const cui_file_options defaults={0};if(!options)options=&defaults;
    cui_file_settings *settings=copy_options(kind,options);if(!settings)return NULL;
    cui_dialog *d=cui__dialog_create(parent,kind,title,options->initial_path,NULL,NULL,callback,data);
    if(!d){cui__file_settings_free(settings);return NULL;}
    d->files=settings;d->selected_filter=settings->initial;return d;
}
static char **copy_paths(cui_dialog *d,const char *const *paths,size_t count)
{
    if(!count||count>65536||!paths)return NULL;
    if((!d->files||!d->files->multiple)&&count!=1)return NULL;
    char **copy=calloc(count,sizeof(*copy));if(!copy)return NULL;
    for(size_t i=0;i<count;++i){
        if(!paths[i]||!*paths[i]||!(copy[i]=cui__desktop_copy(paths[i]))){cui__free_strings(copy,count);return NULL;}
    }
    return copy;
}
void cui__file_finish(cui_dialog *d,cui_dialog_result result,const char *const *paths,size_t count,size_t filter)
{
    if(!d||d->finished)return;
    if(result==CUI_DIALOG_ACCEPTED){
        d->paths=copy_paths(d,paths,count);
        if(d->paths){d->path_count=count;d->selected_filter=filter;}
        else result=CUI_DIALOG_FAILED;
    }
    cui__desktop_finish(d,result,d->path_count?d->paths[0]:"");
}
size_t cui_dialog_path_count(const cui_dialog *d)
{return d&&d->finished&&d->result==CUI_DIALOG_ACCEPTED?d->path_count:0;}
const char *cui_dialog_path(const cui_dialog *d,size_t index)
{return index<cui_dialog_path_count(d)?d->paths[index]:NULL;}
int cui_dialog_filter(const cui_dialog *d,size_t *index)
{
    if(!index||!d||!d->finished||d->result!=CUI_DIALOG_ACCEPTED||!d->files||d->selected_filter>=d->files->count)return 0;
    *index=d->selected_filter;return 1;
}
