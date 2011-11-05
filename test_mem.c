/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

#include <stdlib.h>

#include <check.h>

#include "amp.h"
#include "amp_internal.h"
#include "mem.h"


void *failing_malloc(size_t size)
{
    return NULL;
}

void mem_setup(void)
{
    /* test_malloc() will always fail now */
    the_real_malloc = failing_malloc;
}

void mem_teardown(void)
{
    /* test_malloc() back to normal */
    the_real_malloc = malloc;
}


START_TEST(test_real_malloc_failure)
{
    /* that that our test_malloc() function can handle a simulation of
     * a real malloc() failure */
    allocation_failure_occurred = false;

    debug_print("%s\n", "Simulating real malloc() failure to test test_malloc():");

    void *result = test_malloc(10, '\0');

    fail_unless(result == NULL);
    fail_unless(allocation_failure_occurred == true);
}
END_TEST


Suite *make_mem_suite(void)
{
    Suite *s = suite_create ("Memory");

    TCase *tc_mem = tcase_create("memory");
    tcase_add_checked_fixture(tc_mem, mem_setup, mem_teardown);

    tcase_add_test(tc_mem, test_real_malloc_failure);

    suite_add_tcase(s, tc_mem);
    return s;
};
