#include "cui.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size)
{
    cui_icon_asset *a=cui_icon_decode(data,size);
    if(a){cui_icon_asset *b=cui_icon_retain(a);cui_icon_release(a);cui_icon_release(b);}
    /* Also drive path validation with structured commands and arbitrary floats. */
    if(size>=8+sizeof(cui_icon_command)){
        float dimensions[2];cui_icon_command commands[32];
        memcpy(dimensions,data,8);
        size_t count=(size-8)/sizeof(*commands);if(count>32)count=32;
        memcpy(commands,data+8,count*sizeof(*commands));
        a=cui_icon_vector(dimensions[0],dimensions[1],commands,count);cui_icon_release(a);
    }
    return 0;
}
