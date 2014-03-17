/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

#include <check.h>

#include "amp.h"
#include "buftoll.h"


START_TEST(test_buftoll_invalid_size)
{
    int result;
    int err = -1;

    result = buftoll("123", 0, &err);

    fail_unless(result == 0);
    fail_unless(err == AMP_DECODE_ERROR);
}
END_TEST

START_TEST(test_buftoll_no_digits)
{
    int result;
    int err = -1;

    result = buftoll("+", 1, &err);

    fail_unless(result == 0);
    fail_unless(err == AMP_DECODE_ERROR);
}
END_TEST

START_TEST(test__buftoll_range__error)
{
    int result;
    int err = -1;

    result = buftoll_range("1X3", 3, -10, 10, &err);

    fail_unless(result == 0);
    fail_unless(err == AMP_DECODE_ERROR);
}
END_TEST


Suite *make_buftoll_suite(void)
{
    Suite *s = suite_create ("buftoll");

    TCase *tc_buftoll = tcase_create("buftoll");

    tcase_add_test(tc_buftoll, test_buftoll_invalid_size);
    tcase_add_test(tc_buftoll, test_buftoll_no_digits);
    tcase_add_test(tc_buftoll, test__buftoll_range__error);

    suite_add_tcase(s, tc_buftoll);
    return s;
};
