/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */


#include <stdlib.h>
#include <string.h>

/* Check - C unit testing framework */
#include <check.h>

#include "amp.h"
#include "amp_internal.h"
#include "list.h"

#include "test.h"


START_TEST(test__amp_log__no_logger)
{
    amp_set_log_handler(NULL);

    amp_log("foo"); /* doesn't crash, yay */
}
END_TEST


START_TEST(test__amp_log__stderr)
{
    amp_set_log_handler(amp_stderr_logger);

    amp_log("amp_stderr_logger test message"); /* doesn't crash, yay */
}
END_TEST


List_T logged_messages = NULL;

void test_logger(char *msg)
{
    int err;
    char *copy = MALLOC(strlen(msg)+1);
    memcpy(copy, msg, strlen(msg)+1);
    logged_messages = List_push(logged_messages, copy, &err);
}


START_TEST(test__amp_log__do_log)
{
    amp_set_log_handler(test_logger);
    amp_log("hello");
    amp_log("foo %d %s", 2, "bar"); /* doesn't crash, yay */

    fail_unless(List_length(logged_messages) == 2);

    char *msg;
    logged_messages = List_pop(logged_messages, (void**)&msg);

    fail_if(strcmp(msg, "foo 2 bar"));
    free(msg);

    logged_messages = List_pop(logged_messages, (void**)&msg);

    fail_if(strcmp(msg, "hello"));
    free(msg);
}
END_TEST


Suite *make_log_suite(void) {
    Suite *s = suite_create ("Log");

    TCase *tc_log = tcase_create("log");
    tcase_add_test(tc_log, test__amp_log__no_logger);
    tcase_add_test(tc_log, test__amp_log__stderr);
    tcase_add_test(tc_log, test__amp_log__do_log);
    suite_add_tcase(s, tc_log);

    return s;
}
