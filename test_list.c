/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/* Just a few extra tests for lines/branches that have no coverage yet.
 * The linked-list implementation was copied from CII and is generally
 * believed to be bug-free. Even though we don't have direct tests
 * for a lot of the code in list.c is seems very likely that if it was
 * ever broken it would cause regressions elsewhere in the test
 * suite. */

#include <stdlib.h>
#include <errno.h>

/* Check - C unit testing framework */
#include <check.h>

#include "list.h"
#include "mem.h"

START_TEST(test_push_with_malloc_failures)
{
    int fail_after = 0;
    int err = 5; /* force a value so we don't get test ambiguity from
                    random memory */
    void *arg = (void*)0x1234;

    List_T lst = NULL;

    while (1)
    {
        enable_malloc_failures(fail_after++);

        /* Run code under test */
        lst = List_push(lst, arg, &err);

        /* clean up and check result */
        disable_malloc_failures();

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            fail_unless(lst == NULL);
            fail_unless(err == ENOMEM);
        }
        else
        {
            fail_unless(lst != NULL);
            fail_unless(err == 0);
            /* clean up */
            lst = List_pop(lst, &arg);
            break;
        }
    }
}
END_TEST


START_TEST(test_free)
{
    int err;
    List_T lst = NULL;
    void *a = NULL;
    void *b = NULL;

    lst = List_push(lst, a, &err);
    lst = List_push(lst, b, &err);

    fail_unless(lst != NULL);

    List_free(&lst);

    fail_unless(lst == NULL);
}
END_TEST


/* data passed to applicant() recorded here */
struct {
    void **x;
    void *arg;
} apply_calls[2];
int apply_idx = 0;

/* record arguments to be verifed by test-case */
void applicant(void **x, void *arg)
{
    apply_calls[apply_idx].x   = x;
    apply_calls[apply_idx].arg = arg;
    apply_idx++;
}

START_TEST(test_map)
{
    int err;
    List_T lst = NULL;
    void *a = (void*)0x1234;
    void *b = (void*)0x4567;

    lst = List_push(lst, b, &err);
    lst = List_push(lst, a, &err);

    fail_unless(lst != NULL);

    apply_idx = 0;

    List_map(lst, applicant, (void *)0x333);

    fail_unless(apply_idx == 2);

    fail_unless( *(apply_calls[0].x) == a);
    fail_unless(apply_calls[0].arg == (void*)0x333);

    fail_unless( *(apply_calls[1].x) == b);
    fail_unless(apply_calls[1].arg == (void*)0x333);

    List_free(&lst);
}
END_TEST

START_TEST(test_pop)
{
    int err;
    List_T lst = NULL;
    void *x;
    void *a = (void*)0x1234;
    void *b = (void*)0x4567;

    lst = List_push(lst, b, &err);
    lst = List_push(lst, a, &err);

    fail_unless(lst != NULL);

    lst = List_pop(lst, NULL);
    fail_unless(List_length(lst) == 1);

    lst = List_pop(lst, &x);
    fail_unless(lst == NULL);
    fail_unless(List_length(lst) == 0);
    fail_unless(x == (void*)0x4567);

    lst = List_pop(lst, NULL);
    fail_unless(lst == NULL);
}
END_TEST

Suite *make_list_suite(void)
{
    Suite *s = suite_create ("Linked-List");

    TCase *tc_list = tcase_create("linked-list");
    tcase_add_test(tc_list, test_push_with_malloc_failures);
    tcase_add_test(tc_list, test_free);
    tcase_add_test(tc_list, test_map);
    tcase_add_test(tc_list, test_pop);
    suite_add_tcase(s, tc_list);

    return s;
};

