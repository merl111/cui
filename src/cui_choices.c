#include "cui_search_internal.h"
#include "cui_desktop_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int compare(const void *a,const void *b)
{
    cui_item_id x=((const cui_choice_index *)a)->id,y=((const cui_choice_index *)b)->id;
    return (x>y)-(x<y);
}
void cui__choices_free(cui_choice_model *m)
{
    if(!m)return;
    for(size_t i=0;i<m->count;++i){free(m->items[i].label);free(m->items[i].detail);free(m->items[i].key);free(m->items[i].keywords);}
    free(m->items);free(m->lookup);free(m);
}
static int copy_item(cui_choice_entry *out,const cui_choice *in)
{
    if(!in->id || !in->label || !*in->label)return 0;
    const char *detail=in->detail?in->detail:"",*keywords=in->keywords?in->keywords:"";
    size_t a=strlen(in->label),b=strlen(detail),c=strlen(keywords);
    if(a>65536 || b>65536 || c>65536)return 0;
    char *search=malloc(a+b+c+3);if(!search)return 0;
    snprintf(search,a+b+c+3,"%s %s %s",in->label,detail,keywords);
    out->id=in->id;out->disabled=!!in->disabled;
    out->label=cui__desktop_copy(in->label);out->detail=cui__desktop_copy(detail);
    out->keywords=cui__desktop_copy(keywords);out->key=cui__search_key(search);free(search);
    return out->label&&out->detail&&out->keywords&&out->key;
}
cui_choice_model *cui__choices_copy(const cui_choice *items,size_t count)
{
    if(count>65536 || (count&&!items))return NULL;
    cui_choice_model *m=calloc(1,sizeof(*m));if(!m)return NULL;
    if(!count)return m;
    m->items=calloc(count,sizeof(*m->items));m->lookup=malloc(count*sizeof(*m->lookup));
    if(!m->items||!m->lookup){cui__choices_free(m);return NULL;}
    m->count=count;
    for(size_t i=0;i<count;++i){
        if(!copy_item(m->items+i,items+i)){cui__choices_free(m);return NULL;}
        m->lookup[i]=(cui_choice_index){items[i].id,i};
    }
    qsort(m->lookup,count,sizeof(*m->lookup),compare);
    for(size_t i=1;i<count;++i)if(m->lookup[i].id==m->lookup[i-1].id){cui__choices_free(m);return NULL;}
    return m;
}
cui_choice_entry *cui__choices_find(const cui_choice_model *m,cui_item_id id)
{
    if(!m||!m->count)return NULL;
    cui_choice_index key={id,0};const cui_choice_index *found=bsearch(&key,m->lookup,m->count,sizeof(key),compare);
    return found?m->items+found->index:NULL;
}
