#ifndef CUI_TEST_ALLOCATIONS_H
#define CUI_TEST_ALLOCATIONS_H
#include <stddef.h>
/* Linux linker wrapping affects CUI/test object references, not GTK's allocator.
 * Fixed bookkeeping storage deliberately makes no allocations of its own. */
void cui_test_fail_allocation(size_t nth);
size_t cui_test_allocation_attempts(void);
int cui_test_allocation_failed(void);
size_t cui_test_live_allocations(void);
size_t cui_test_live_bytes(void);
#endif
