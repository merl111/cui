#include "cui_navigation_internal.h"
#include "cui_search_internal.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
/* A bounded model with both valid and invalid IDs, ancestry and UTF-8. */
int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size)
{
    cui_tree_item tree[32];cui_choice choices[32];char names[32][33];
    size_t count=size/36;if(count>32)count=32;
    for(size_t i=0;i<count;++i){
        const uint8_t *p=data+i*36;memcpy(names[i],p+4,32);names[i][32]=0;
        tree[i]=(cui_tree_item){p[0],p[1],names[i],p[2]&1};
        choices[i]=(cui_choice){p[0],names[i],names[i],names[i],p[3]&1};
    }
    cui_tree_model *model=cui__tree_model_copy(tree,count);cui__tree_model_free(model);
    cui_choice_model *options=cui__choices_copy(choices,count);cui__choices_free(options);
    return 0;
}
