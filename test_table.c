/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/* Just a few extra tests for lines/branches that have no coverage yet.
 * The hash-table implementation was copied from CII and is generally
 * believed to be bug-free. Even though we don't have direct tests
 * for a lot of the code in table.c is seems very likely that if it was
 * ever broken it would cause regressions elsewhere in the test
 * suite. */

/* Check - C unit testing framework */
#include <check.h>

#include "table.h"

static int cmpatom(const void *x, const void *y)
{
    return x != y;
}

static unsigned hashatom(const void *key)
{
    /* all items go in same bucket */
    return 0;
}

START_TEST(test_new_with_size)
{
    /* A size hint should be respected */

    Table_T *table = Table_new(130, cmpatom, hashatom);

    fail_unless( Table_num_buckets(table) == 127 );

    Table_free(&table);
}
END_TEST


START_TEST(test_put_replace)
{
    /* New values replace old values with the same key. */

    Table_T *table = Table_new(0, cmpatom, hashatom);

    Table_put(table, (void*)0x12, (void*)0x34);
    Table_put(table, (void*)0x12, (void*)0x56);

    fail_unless( Table_length(table) == 1 );
    fail_unless( Table_get(table, (void*)0x12) == (void*)0x56);

    Table_remove(table, (void*)0x12);
    Table_free(&table);
}
END_TEST


START_TEST(test_put_in_same_bucket)
{
    /* Our hash function always places key/vals in the same bucket -
     * assert that multiple items can be placed and retreived from
     * the same bucket */

    Table_T *table = Table_new(0, cmpatom, hashatom);

    Table_put(table, (void*)0x12, (void*)0x34);
    Table_put(table, (void*)0x56, (void*)0x78);

    fail_unless( Table_length(table) == 2 );
    fail_unless( Table_get(table, (void*)0x12) == (void*)0x34);
    fail_unless( Table_get(table, (void*)0x56) == (void*)0x78);

    Table_remove(table, (void*)0x12);
    Table_remove(table, (void*)0x56);
    Table_free(&table);
}
END_TEST


START_TEST(test_get)
{
    /* Table_get() with non-existant key returns NULL */

    Table_T *table = Table_new(0, cmpatom, hashatom);

    Table_put(table, (void*)0x12, (void*)0x34);

    fail_unless( Table_length(table) == 1 );
    fail_unless( Table_get(table, (void*)0x12) == (void*)0x34);

    /* non-existant key */
    fail_unless( Table_get(table, (void*)0x34) == NULL);

    Table_remove(table, (void*)0x12);
    Table_free(&table);
}
END_TEST

Suite *make_table_suite(void)
{
    Suite *s = suite_create ("Hash-Table");

    TCase *tc_table = tcase_create("hash-table");
    tcase_add_test(tc_table, test_new_with_size);
    tcase_add_test(tc_table, test_put_replace);
    tcase_add_test(tc_table, test_put_in_same_bucket);
    tcase_add_test(tc_table, test_get);
    suite_add_tcase(s, tc_table);

    return s;
};

