/* Copyright (c) 2011 - Eric P. Mangold
 *
 * See LICENSE.txt for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

/* Check - C unit testing framework */
#include <check.h>

#include "ampc.h"
#include "../test.h"


void compiler_test_setup (void)
{
}

void compiler_test_teardown (void)
{
}

char *_test_file(char *filename)
{
    return filename;
}

START_TEST(test_roundtrip)
{

    AMP_Compiler_T *c = ampc_new();

    if (c == NULL)
        fail("out of memory.");

    ampc_load_schema_file(_test_file("test_api.amp.json"));

    ampc_compile_header_file("test_api.libamp.h", NULL);

    /* Compile our test client/serve code which expect to utilize
     * the now-generated header.. */
    system("scons SConstruct.ampc_test");
}
END_TEST

Suite *make_ampc_suite()
{

    Suite *s = suite_create("Compiler");

    /* Compiler test cases */
    TCase *tc_compiler = tcase_create("compiler");
    tcase_add_checked_fixture(tc_compiler, compiler_test_setup, compiler_test_teardown);

    tcase_add_test(tc_compiler, test_roundtrip);

    suite_add_tcase(s, tc_compiler);
    return s;
}
