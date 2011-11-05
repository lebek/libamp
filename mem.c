/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "amp.h"
#include "amp_internal.h"
#include "mem.h"


#ifdef AMP_TEST_SUPPORT

/* concrete definitions of variables declared "extern" in mem.h */
malloc_ptr the_real_malloc;
bool failure_mode_enabled;
int allocations_until_failure;
bool allocation_failure_occurred;


void enable_malloc_failures(int times_until_failure)
{
    allocation_failure_occurred = false;
    failure_mode_enabled = true;
    allocations_until_failure = times_until_failure;
}

void disable_malloc_failures()
{
    failure_mode_enabled = false;
}


void *test_malloc(size_t size, char c)
{

    if (failure_mode_enabled)
    {
        if (allocations_until_failure-- <= 0)
        {
            allocation_failure_occurred = true;
            return NULL;
        }
    }

    void *ptr;
    if ( (ptr = the_real_malloc(size)) == NULL)
    {
        /* Real allocation failure during test suite run? Uh oh */
        allocation_failure_occurred = true;
        debug_print("%s\n", "REAL malloc() failure in test_malloc()! Uh oh.");
        return NULL;
    }

    memset(ptr, c, size);

    return ptr;
}
#endif
