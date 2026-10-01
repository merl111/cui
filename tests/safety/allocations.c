#include "allocations.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void *__real_realloc(void *,size_t);
void __real_free(void *);
typedef struct allocation { void *pointer; size_t size; } allocation;
static allocation slots[32768];
static size_t live, bytes, attempts, fail_at;
static int failed;
void cui_test_fail_allocation(size_t nth) { attempts=0; fail_at=nth; failed=0; }
size_t cui_test_allocation_attempts(void) { return attempts; }
int cui_test_allocation_failed(void) { return failed; }
size_t cui_test_live_allocations(void) { return live; }
size_t cui_test_live_bytes(void) { return bytes; }
static int should_fail(void)
{ ++attempts; if(fail_at && attempts==fail_at){ failed=1; return 1; } return 0; }
static size_t find(void *pointer)
{ for(size_t i=0;i<sizeof(slots)/sizeof(*slots);++i)if(slots[i].pointer==pointer)return i; return SIZE_MAX; }
static void remember(void *pointer,size_t size)
{
    if(!pointer)return;
    size_t i=find(NULL);
    if(i==SIZE_MAX){fputs("Allocation ledger capacity exceeded\n",stderr);abort();}
    slots[i]=(allocation){pointer,size};++live;bytes+=size;
}
static void forget(size_t i)
{ if(i!=SIZE_MAX){bytes-=slots[i].size;--live;slots[i]=(allocation){0};} }
void *__wrap_malloc(size_t size)
{ if(should_fail())return NULL;void *p=__real_malloc(size);remember(p,size);return p; }
void *__wrap_calloc(size_t count,size_t size)
{
    if(should_fail() || (size && count>SIZE_MAX/size))return NULL;
    void *p=__real_calloc(count,size);remember(p,count*size);return p;
}
void *__wrap_realloc(void *pointer,size_t size)
{
    if(should_fail())return NULL;
    size_t i=pointer?find(pointer):SIZE_MAX;
    void *p=__real_realloc(pointer,size);
    if(p || !size){forget(i);remember(p,size);}
    return p;
}
void __wrap_free(void *pointer)
{ if(pointer)forget(find(pointer));__real_free(pointer); }
