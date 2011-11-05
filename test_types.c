/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

#define _ISOC99_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* Check - C unit testing framework */
#include <check.h>

#include "amp.h"
#include "amp_internal.h"
#include "test.h"
#include "buftoll.h"


void printBuf(unsigned char *buf, int bufLen)
{
    int i;
    char bufStr[4*bufLen+1];
    char* bufPtr = bufStr;
    for (i = 0; i < bufLen; i++)
    {
        bufPtr += snprintf(bufPtr, 5, "\\x%02X", buf[i]);
    }
    *bufPtr = '\0';
    debug_print("%s\n", bufStr);
}

/* these two arrays initalized in make_types_suite() */
char ll_max[100];
char ll_min[100];

struct put_ll_case {
    long long testValue;
    char *expectedResult;
} put_ll_cases[] = {
    {0,         "0"},
    {-1,        "-1"},
    {123,       "123"},
    {LLONG_MAX, ll_max},
    {LLONG_MIN, ll_min}
};
int num_put_ll_tests = (sizeof(put_ll_cases) /
                        sizeof(struct put_ll_case));

START_TEST(test__amp_put_long_long)
{
    int ret;
    unsigned char *buf;
    int bufSize;

    /* the variable _i is made available by Check's "loop test" machinery */
    struct put_ll_case c = put_ll_cases[_i];

    AMP_Box_p box = amp_new_box();

    debug_print("Testing %lld encodes to \"%s\"...\n",
                c.testValue, c.expectedResult);

    ret = amp_put_long_long(box, "key", c.testValue);

    fail_unless(ret == 0,
                "ERR: amp_put_long_long() != 0");

    amp_get_bytes(box, "key", &buf, &bufSize);

    fail_unless(bufSize == strlen(c.expectedResult),
                "ERR: bufSize != %d", strlen(c.expectedResult));

    if (memcmp(buf, c.expectedResult,
                    strlen(c.expectedResult)) != 0)
    {
        debug_print("%s", "buf contains wrong data\n");
        debug_print("%s", "bad buf: ");
        printBuf(buf, bufSize);
        fail("buf does not compare equal to c->expectedResult");
    }

    amp_free_box(box);
}
END_TEST

void incrementDigit(char *s)
{
    /* Ugly hack to increment an ASCII string representation of a number.
     * Do so by calling incrementDigit(pointer_to_last_char_in_string);
     * FAILS horribly if the string is empty or all 9's, but honey badger dont care */
    if (*s == '9')
    {
        *s = '0';
        incrementDigit(--s); /* recurse */
    }
    else
    {
        (*s)++;
    }
}

/* amp_get_long_long() test case data */

/* these two arrays initialized in main() */
char ll_maxPlusOne[100];
char ll_minMinusOne[100];

struct get_ll_case {
    int expectedError;
    long long expectedResult;
    char *testValue;
} get_ll_cases[] = {
    {0,  0,          "0"},
    {0, -1,         "-1"},
    {0,  1,         "+1"},
    {0,  123,        "123"},
    {0,  LLONG_MAX,   ll_max},
    {0,  LLONG_MIN,   ll_min},

    {AMP_DECODE_ERROR, 0, "--1"},
    {AMP_DECODE_ERROR, 0, "++1"},

    {AMP_DECODE_ERROR, 0, " 1"}, /* leading junk */
    {AMP_DECODE_ERROR, 0, "1 "}, /* trailing junk */

    {AMP_OUT_OF_RANGE, 0,  "999999999999999999999999999999999999999999999"},
    {AMP_OUT_OF_RANGE, 0, "-999999999999999999999999999999999999999999999"},

    {AMP_OUT_OF_RANGE, 0,  ll_maxPlusOne},
    {AMP_OUT_OF_RANGE, 0,  ll_minMinusOne}
};
int num_get_ll_tests = (sizeof(get_ll_cases) /
                        sizeof(struct get_ll_case));

START_TEST(test__amp_get_long_long)
{
    int ret;
    long long got;

    /* the variable _i is made available by Check's "loop test" machinery */
    struct get_ll_case c = get_ll_cases[_i];

    AMP_Box_p box = amp_new_box();

    if (c.expectedError) {
        debug_print("Testing \"%s\" fails decode with error %d.\n",
                    c.testValue, c.expectedError);
    }
    else
    {
        debug_print("Testing \"%s\" decodes to %lld.\n",
                    c.testValue, c.expectedResult);
    }

    amp_put_cstring(box, "key", c.testValue);

    ret = amp_get_long_long(box, "key", &got);

    if (c.expectedError) {
        fail_unless(ret == c.expectedError,
                    "ERR: amp_get_long_long() != %d", c.expectedError);
    }
    else
    {
        fail_unless(ret == 0,
                    "ERR: amp_get_long_long() != 0");

        fail_unless(got == c.expectedResult,
                    "ERR: %lld != %lld", got, c.expectedResult);
    }

    amp_free_box(box);
}
END_TEST


START_TEST(test__amp_get_long_long__no_such_key)
{
    int ret;
    long long got;
    AMP_Box_p box = amp_new_box();

    ret = amp_get_long_long(box, "missing_key", &got);

    fail_unless(ret == AMP_KEY_NOT_FOUND);

    amp_free_box(box);
}
END_TEST


/* these two arrays initalized in main() */
char int_max[100];
char int_min[100];

struct put_int_case
{
    int testValue;
    char *expectedResult;
} put_int_cases[] = {
    {0,         "0"},
    {-1,        "-1"},
    {123,       "123"},
    {INT_MAX, int_max},
    {INT_MIN, int_min}
};
int num_put_int_tests = (sizeof(put_int_cases) /
                         sizeof(struct put_int_case));

START_TEST(test__amp_put_int)
{
    int ret;
    unsigned char *buf;
    int bufSize;

    /* the variable _i is made available by Check's "loop test" machinery */
    struct put_int_case c = put_int_cases[_i];

    AMP_Box_p box = amp_new_box();

    debug_print("Testing %d encodes to \"%s\"...\n",
                c.testValue, c.expectedResult);

    ret = amp_put_int(box, "key", c.testValue);

    fail_unless(ret == 0,
                "ERR: amp_put_int() != 0");

    amp_get_bytes(box, "key", &buf, &bufSize);

    fail_unless(bufSize == strlen(c.expectedResult),
                "ERR: bufSize != %d", strlen(c.expectedResult));

    if (memcmp(buf, c.expectedResult,
                    strlen(c.expectedResult)) != 0)
    {
        debug_print("%s", "buf contains wrong data\n");
        debug_print("%s", "bad buf: ");
        printBuf(buf, bufSize);
        fail("buf does not compare equal to c->expectedResult");
    }

    amp_free_box(box);
}
END_TEST

/* amp_get_int() test case data */

/* these two arrays initialized in main() */
char int_maxPlusOne[100];
char int_minMinusOne[100];

struct get_int_case
{
    int expectedError;
    int expectedResult;
    char *testValue;
} get_int_cases[] = {
    {0,  0,          "0"},
    {0, -1,         "-1"},
    {0,  1,         "+1"},
    {0,  123,        "123"},
    {0,  INT_MAX,   int_max},
    {0,  INT_MIN,   int_min},

    {AMP_DECODE_ERROR, 0, "--1"},
    {AMP_DECODE_ERROR, 0, "++1"},

    {AMP_DECODE_ERROR, 0, " 1"}, /* leading junk */
    {AMP_DECODE_ERROR, 0, "1 "}, /* trailing junk */

    {AMP_OUT_OF_RANGE, 0,  "999999999999999999999999999999999999999999999"},
    {AMP_OUT_OF_RANGE, 0, "-999999999999999999999999999999999999999999999"},

    {AMP_OUT_OF_RANGE, 0,  int_maxPlusOne},
    {AMP_OUT_OF_RANGE, 0,  int_minMinusOne}
};
int num_get_int_tests = (sizeof(get_int_cases) /
                         sizeof(struct get_int_case));

START_TEST(test__amp_get_int)
{
    int ret;
    int got;

    /* the variable _i is made available by Check's "loop test" machinery */
    struct get_int_case c = get_int_cases[_i];

    AMP_Box_p box = amp_new_box();

    if (c.expectedError)
    {
        debug_print("Testing \"%s\" fails decode with error %d.\n",
                    c.testValue, c.expectedError);
    }
    else
    {
        debug_print("Testing \"%s\" decodes to %d.\n",
                    c.testValue, c.expectedResult);
    }

    amp_put_cstring(box, "key", c.testValue);

    ret = amp_get_int(box, "key", &got);

    if (c.expectedError)
    {
        fail_unless(ret == c.expectedError,
                    "ERR: amp_get_int() != %d", c.expectedError);
    }
    else
    {
        fail_unless(ret == 0,
                    "ERR: amp_get_int() != 0");

        fail_unless(got == c.expectedResult,
                    "ERR: %d != %d", got, c.expectedResult);
    }

    amp_free_box(box);
}
END_TEST

/* these two arrays initialized in main() */
char uint_max[100];
char uint_maxPlusOne[100];

struct get_uint_case
{
    int expectedError;
    int expectedResult;
    char *testValue;
} get_uint_cases[] = {
    {0,  0,          "0"},
    {0,  1,          "1"},
    {0,  1,          "+1"},
    {0,  123,        "123"},
    {0,  UINT_MAX,   uint_max},
    {AMP_DECODE_ERROR, 0, " 1"}, /* leading junk */
    {AMP_DECODE_ERROR, 0, "1 "}, /* trailing junk */
    {AMP_OUT_OF_RANGE, 0, "-1"},
    {AMP_OUT_OF_RANGE, 0,  "9999999999999999999999999999999999999999999999999"},
    {AMP_OUT_OF_RANGE, 0,  uint_maxPlusOne},
};
int num_get_uint_tests = (sizeof(get_uint_cases) /
                         sizeof(struct get_uint_case));

START_TEST(test__amp_get_uint)
{
    unsigned int ret;
    unsigned int got;

    /* the variable _i is made available by Check's "loop test" machinery */
    struct get_uint_case c = get_uint_cases[_i];

    AMP_Box_p box = amp_new_box();

    if (c.expectedError)
    {
        debug_print("Testing \"%s\" fails decode with error %d.\n",
                    c.testValue, c.expectedError);
    }
    else
    {
        debug_print("Testing \"%s\" decodes to %d.\n",
                    c.testValue, c.expectedResult);
    }

    amp_put_cstring(box, "key", c.testValue);

    ret = amp_get_uint(box, "key", &got);

    if (c.expectedError)
    {
        fail_unless(ret == c.expectedError,
                    "ERR: amp_get_uint() != %d for input \"%s\"",
                    c.expectedError, c.testValue);
    }
    else
    {
        fail_unless(ret == 0,
                    "ERR: amp_get_uint() != 0 for input \"%s\"",
                    c.testValue);

        fail_unless(got == c.expectedResult,
                    "ERR: %u != %u", got, c.expectedResult);
    }

    amp_free_box(box);
}
END_TEST

struct put_uint_case
{
    int expectedError;
    int testValue;
    char *expectedResult;
} put_uint_cases[] = {
    {0,  0,          "0"},
    {0,  1,          "1"},
    {0,  123,        "123"},
    {0,  UINT_MAX,   uint_max}
};
int num_put_uint_tests = (sizeof(put_uint_cases) /
                          sizeof(struct put_uint_case));

START_TEST(test__amp_put_uint)
{
    int ret;
    unsigned char *buf;
    int bufSize;

    /* the variable _i is made available by Check's "loop test" machinery */
    struct put_uint_case c = put_uint_cases[_i];

    AMP_Box_p box = amp_new_box();

    debug_print("Testing %d encodes to \"%s\"...\n",
                c.testValue, c.expectedResult);

    ret = amp_put_uint(box, "key", c.testValue);

    if (c.expectedError)
    {
        fail_unless(ret == c.expectedError,
                    "ERR: amp_put_uint() != %d", c.expectedError);
    }
    else
    {

        amp_get_bytes(box, "key", &buf, &bufSize);

        fail_unless(bufSize == strlen(c.expectedResult),
                    "ERR: bufSize != %d", strlen(c.expectedResult));

        if (memcmp(buf, c.expectedResult,
                   strlen(c.expectedResult)) != 0)
        {
            debug_print("%s", "buf contains wrong data\n");
            debug_print("%s", "bad buf: ");
            printBuf(buf, bufSize);
            fail("buf does not compare equal to c->expectedResult");
        }
    }

    amp_free_box(box);
}
END_TEST


START_TEST(test__amp_get_uint__no_such_key)
{
    int ret;
    int got;
    AMP_Box_p box = amp_new_box();

    ret = amp_get_uint(box, "missing_key", &got);

    fail_unless(ret == AMP_KEY_NOT_FOUND);

    amp_free_box(box);
}
END_TEST


/* amp_put_double() test case data */
struct put_double_case
{
    double testValue;
    char *resultStartsWith;
} put_double_cases[] = {
    {0.0,                 "0.0"},
    {1.0,                 "1.0"},
    {-1.0,               "-1.0"},
    {3.14159265358979323, "3.141592653589793"},
    {99999.9999999,       "99999.9999"},
#ifdef NAN
    {NAN,                 "nan"},
#endif
#ifdef INFINITY
    {INFINITY,            "inf"},
    {-INFINITY,           "-inf"}
#endif
};
int num_put_double_tests = (sizeof(put_double_cases) /
                            sizeof(struct put_double_case));

START_TEST(test__amp_put_double)
{
    int ret;
    unsigned char *buf;
    int bufSize;

    /* the variable _i is made available by Check's "loop test" machinery */
    struct put_double_case c = put_double_cases[_i];

    AMP_Box_p box = amp_new_box();

    debug_print("Testing %.20f encoding startswith \"%s\"...\n",
                c.testValue, c.resultStartsWith);

    ret = amp_put_double(box, "key", c.testValue);

    fail_unless(ret == 0,
                "amp_put_double() != 0");

    amp_get_bytes(box, "key", &buf, &bufSize);

    fail_unless(bufSize >= strlen(c.resultStartsWith),
                "bufSize < %d", strlen(c.resultStartsWith));

    if (memcmp(buf, c.resultStartsWith,
                    strlen(c.resultStartsWith)) != 0)
    {
        debug_print("%s", "buf contains wrong data\n");
        debug_print("%s", "bad buf: ");
        printBuf(buf, bufSize);
        fail("buf does not start with \"%s\"", c.resultStartsWith);
    }

    amp_free_box(box);

}
END_TEST


/* amp_get_double() test case data */
struct get_double_case {
    int    expectedError;
    double expectedResult;
    char  *testValue;
} get_double_cases[] = {
    {0,  0.0,         "0.0"},
    {0,  1.0,         "1.0"},
    {0,  1.0,        "+1.0"},
    {0, -1.0,        "-1.0"},
    {0,  3.0,         "3."},
    {0, -3.0,        "-3."},
    {0,  3.0,         "3"},
    {0, -3.0,        "-3"},
    {0,  3.14159265358979323846,   "3.14159265358979323846"},
    {0, -3.14159265358979323846,  "-3.14159265358979323846"},
    {0,  99999.99999, "99999.99999"},

    {0,  1.012345678901234567890123456789,  "1.012345678901234567890123456789012345678901234567890123456789"
                                            "012345678901234567890123456789012345678901234567890123456789"},

    {0, -1.012345678901234567890123456789, "-1.012345678901234567890123456789012345678901234567890123456789"
                                            "012345678901234567890123456789012345678901234567890123456789"},

    {AMP_DECODE_ERROR, 0,  ""},
    {AMP_DECODE_ERROR, 0,  "+"},
    {AMP_DECODE_ERROR, 0,  "-"},
    {AMP_DECODE_ERROR, 0,  ".0"},
    {AMP_DECODE_ERROR, 0, "-.0"},
    {AMP_DECODE_ERROR, 0, "+.0"},
    {AMP_DECODE_ERROR, 0, "1..0"},

    {AMP_DECODE_ERROR, 0, "++0"},
    {AMP_DECODE_ERROR, 0, "--0"},
    {AMP_DECODE_ERROR, 0, " 0"},
    {AMP_DECODE_ERROR, 0, "0 "}
};
int num_get_double_tests = (sizeof(get_double_cases) /
                            sizeof(struct get_double_case));


START_TEST(test__amp_get_double__normal)
{
    int ret;
    unsigned char *buf;
    int bufSize;
    double got;

    /* the variable _i is made available by Check's "loop test" machinery */
    struct get_double_case c = get_double_cases[_i];

    AMP_Box_p box = amp_new_box();

    if (c.expectedError)
    {
        debug_print("Testing \"%s\" fails decode with error %d.\n",
                    c.testValue, c.expectedError);
    }
    else
    {
        debug_print("Testing \"%s\" decodes to %f\n",
                    c.testValue, c.expectedResult);
    }

    if (_amp_put_buf(box, "key", c.testValue, strlen(c.testValue)) != 0)
            fail("_amp_put_buf() != 0\n");


    ret = amp_get_double(box, "key", &got);

    if (c.expectedError)
    {
        fail_unless(ret == c.expectedError,
                    "ERR: amp_get_double() != %d", c.expectedError);
    }
    else
    {
        fail_unless(ret == 0,
                    "ERR: amp_get_double() == %d (expected 0)", ret);

        fail_unless(got == c.expectedResult,
                    "ERR: %.20f != %.20f", got, c.expectedResult);
    }

    amp_free_box(box);

}
END_TEST

/* amp_get_double() special-value test case data */
struct get_double_special_case
{
    char *testValue;
    int   expectedError;
    int   isInfinite;
    int   isPositive;
    int   isNotANumber;
} get_double_special_cases[] = {
    {"nan",  0, 0, 1, 1},
    {"inf",  0, 1, 1, 0},
    {"-inf", 0, 1, 0, 0},

    {"Nan",  AMP_DECODE_ERROR, 0, 0, 0},
    {"snan", AMP_DECODE_ERROR, 0, 0, 0},
    {"Inf",  AMP_DECODE_ERROR, 0, 0, 0},
    {"-Inf", AMP_DECODE_ERROR, 0, 0, 0}
};
int num_get_double_special_tests = (sizeof(get_double_special_cases) /
                                    sizeof(struct get_double_special_case));

START_TEST(test__amp_get_double__special)
{
    int ret;
    unsigned char *buf;
    int bufSize;
    double got;


    /* the variable _i is made available by Check's "loop test" machinery */
    struct get_double_special_case c = get_double_special_cases[_i];

    AMP_Box_p box = amp_new_box();


    debug_print("Testing \"%s\" decoding...\n", c.testValue);

    if (_amp_put_buf(box, "key", c.testValue, strlen(c.testValue)) != 0)
            fail("_amp_put_buf() != 0\n");

    ret = amp_get_double(box, "key", &got);

    if (c.expectedError)
    {
        fail_unless(ret == c.expectedError,
                    "ERR: amp_get_double() != %d", c.expectedError);
    }
    else
    {
        fail_unless(ret == 0,
                    "ERR: amp_get_double() != 0");

        /* infinite? */
        fail_unless(c.isInfinite ? (isinf(got) != 0) : (isinf(got) == 0),
                    "ERR: %f failed test isInfinite == %d", got, c.isInfinite);

        /* positive? */
        fail_unless(c.isPositive ? (signbit(got) == 0) : (signbit(got) != 0),
                    "ERR: %f failed test isPositive == %d", got, c.isPositive);

        /* Not-A-Number? */
        fail_unless(c.isNotANumber ? (isnan(got) != 0) : (isnan(got) == 0),
                    "ERR: %f failed test isNotANumber == %d", got, c.isNotANumber);
    }

    amp_free_box(box);
}
END_TEST


START_TEST(test__amp_get_double__no_such_key)
{
    int ret;
    double got;
    AMP_Box_p box = amp_new_box();

    ret = amp_get_double(box, "missing_key", &got);

    fail_unless(ret == AMP_KEY_NOT_FOUND);

    amp_free_box(box);
}
END_TEST


/* amp_put_bool() test case data */
struct put_bool_case
{
    int testValue;
    char *expectedResult;
} put_bool_cases[] = {
    {0,         "False"},
    {1,         "True"},
    {-20,       "True"}
};
int num_put_bool_tests = (sizeof(put_bool_cases) /
                          sizeof(struct put_bool_case));

START_TEST(test__amp_put_bool)
{
    int ret;
    unsigned char *buf;
    int bufSize;

    struct put_bool_case c = put_bool_cases[_i];

    AMP_Box_p box = amp_new_box();

    debug_print("Testing %d encodes to \"%s\"...\n",
                c.testValue, c.expectedResult);

    ret = amp_put_bool(box, "key", c.testValue);

    fail_unless(ret == 0,
                "ERR: amp_put_bool() != 0");

    amp_get_bytes(box, "key", &buf, &bufSize);

    fail_unless(bufSize == strlen(c.expectedResult),
                "ERR: bufSize != %d", strlen(c.expectedResult));

    if (memcmp(buf, c.expectedResult,
                    strlen(c.expectedResult)) != 0)
    {
        debug_print("%s", "buf contains wrong data\n");
        debug_print("%s", "bad buf: ");
        printBuf(buf, bufSize);
        fail("ERR: bad boolean encoding.");
    }

    amp_free_box(box);
}
END_TEST

struct get_bool_case
{
    int expectedError;
    int expectedResult;
    char *testValue;
} get_bool_cases[] = {
    {0,                0, "False"},
    {0,                1, "True"},
    {AMP_DECODE_ERROR, 0, "1"},
    {AMP_DECODE_ERROR, 0, "true"},
    {AMP_DECODE_ERROR, 0, "True "},
    {AMP_DECODE_ERROR, 0, " True"},
    {AMP_DECODE_ERROR, 0, "Truee"},
    {AMP_DECODE_ERROR, 0, "Tru"}
};
int num_get_bool_tests = (sizeof(get_bool_cases) /
                          sizeof(struct get_bool_case));

START_TEST(test__amp_get_bool)
{
    int ret;
    int got;

    /* the variable _i is made available by Check's "loop test" machinery */
    struct get_bool_case c = get_bool_cases[_i];

    AMP_Box_p box = amp_new_box();

    if (c.expectedError)
    {
        debug_print("Testing \"%s\" fails decode with error %d.\n",
                    c.testValue, c.expectedError);
    }
    else
    {
        debug_print("Testing \"%s\" decodes to %d.\n",
                    c.testValue, c.expectedResult);
    }

    amp_put_cstring(box, "key", c.testValue);
    ret = amp_get_bool(box, "key", &got);

    if (c.expectedError)
    {
        fail_unless(ret == c.expectedError,
                    "ERR: amp_get_bool() != %d", c.expectedError);
    }
    else
    {
        fail_unless(ret == 0,
                    "ERR: amp_get_bool() != 0");

        fail_unless(got == c.expectedResult,
                    "ERR: %d != %d", got, c.expectedResult);
    }

    amp_free_box(box);
}
END_TEST


START_TEST(test__amp_get_bool__no_such_key)
{
    int ret;
    int got;
    AMP_Box_p box = amp_new_box();

    ret = amp_get_bool(box, "missing_key", &got);

    fail_unless(ret == AMP_KEY_NOT_FOUND);

    amp_free_box(box);
}
END_TEST


/* AMP_DateTime test cases */
struct put_dt_case {
    int expectedError;
    AMP_DateTime testValue;
    char *expectedResult;
} put_dt_cases[] = {
      /* year  mon day hour min sec msec    utc_offset */
    {0, {2011, 6,  20, 5,   10, 15, 100000, 330},
        "2011-06-20T05:10:15.100000+05:30"},

    {0, {50, 11,  7, 17,   25, 15, 1234, -220},
        "0050-11-07T17:25:15.001234-03:40"},

    /* minimum date */
    {0, {1,   1, 1, 0, 0, 0, 0,     -1439},
        "0001-01-01T00:00:00.000000-23:59"},

    /* maximum date */
    {0, {9999, 12, 31, 23, 59, 59, 999999, 1439},
        "9999-12-31T23:59:59.999999+23:59"},

    /* year limits */
    {AMP_ENCODE_ERROR, {0,     1, 1, 0, 0, 0, 0, 0}, NULL},
    {AMP_ENCODE_ERROR, {10000, 1, 1, 0, 0, 0, 0, 0}, NULL},

    /* month limits */
    {AMP_ENCODE_ERROR, {1, 0,  1, 0, 0, 0, 0, 0}, NULL},
    {AMP_ENCODE_ERROR, {1, 13, 1, 0, 0, 0, 0, 0}, NULL},

    /* day limits */
    {AMP_ENCODE_ERROR, {1, 1, 0,  0, 0, 0, 0, 0}, NULL},
    {AMP_ENCODE_ERROR, {1, 1, 32, 0, 0, 0, 0, 0}, NULL},

    /* hour limits */
    {AMP_ENCODE_ERROR, {1, 1, 1, -1, 0, 0, 0, 0}, NULL},
    {AMP_ENCODE_ERROR, {1, 1, 1, 24, 0, 0, 0, 0}, NULL},

    /* minute limits */
    {AMP_ENCODE_ERROR, {1, 1, 1, 0, -1, 0, 0, 0}, NULL},
    {AMP_ENCODE_ERROR, {1, 1, 1, 0, 60, 0, 0, 0}, NULL},

    /* second limits */
    {AMP_ENCODE_ERROR, {1, 1, 1, 0, 0, -1, 0, 0}, NULL},
    {AMP_ENCODE_ERROR, {1, 1, 1, 0, 0, 60, 0, 0}, NULL},

    /* milisecond limits */
    {AMP_ENCODE_ERROR, {1, 1, 1, 0, 0, 0, -1,      0}, NULL},
    {AMP_ENCODE_ERROR, {1, 1, 1, 0, 0, 0, 1000000, 0}, NULL},

    /* utc_offset limits */
    {AMP_ENCODE_ERROR, {1, 1, 1, 0, 0, 0, 0, -1440}, NULL},
    {AMP_ENCODE_ERROR, {1, 1, 1, 0, 0, 0, 0,  1440}, NULL},

};
int num_put_dt_tests = (sizeof(put_dt_cases) /
                        sizeof(struct put_dt_case));


START_TEST(test__amp_put_datetime)
{
    int ret;
    uint8_t *buf;
    int buf_size;

    /* the variable _i is made available by Check's "loop test" machinery */
    struct put_dt_case c = put_dt_cases[_i];

    AMP_Box_p box = amp_new_box();

    ret = amp_put_datetime(box, "key", &(c.testValue));

    fail_unless(ret == c.expectedError);

    if (!c.expectedError)
    {
        amp_get_bytes(box, "key", &buf, &buf_size);

        fail_unless(buf_size == strlen(c.expectedResult),
                    "ERR: buf_size != %d", strlen(c.expectedResult));

        if (memcmp(buf, c.expectedResult,
                        strlen(c.expectedResult)) != 0)
        {
            debug_print("%s", "buf contains wrong data\n");
            debug_print("%s", "bad buf: ");
            printBuf(buf, buf_size);
            fail("buf does not compare equal to c->expectedResult");
        }
    }

    amp_free_box(box);
}
END_TEST


/* AMP_DateTime test cases */
struct get_dt_case {
    int expectedError;
    char *testValue;
    AMP_DateTime expectedResult;
} get_dt_cases[] = {

    /* one byte too long */
    {AMP_DECODE_ERROR, "2011-06-20T05:10:15.100000+05:300", 0},

    /* one byte too short */
    {AMP_DECODE_ERROR, "2011-06-20T05:10:15.100000+05:3", 0},

    /* minimum date */
    {0, "0001-01-01T00:00:00.000000-23:59",
        {1,   1, 1, 0, 0, 0, 0,     -1439}},

    /* maximum date */
    {0, "9999-12-31T23:59:59.999999+23:59",
        {9999, 12, 31, 23, 59, 59, 999999, 1439}},

    /* some arbitrary date */
    {0, "2011-06-20T05:10:15.100000+05:30",
        {2011, 6,  20, 5,   10, 15, 100000, 330}},

    /* another arbitrary date */
    {0, "0050-11-07T17:25:15.001234-03:40",
        {50, 11,  7, 17,   25, 15, 1234, -220}},

    /* bad year */
    {AMP_OUT_OF_RANGE, "0000-01-01T00:00:00.000000+00:00", 0},

    /* bad month */
    {AMP_OUT_OF_RANGE, "0001-00-01T00:00:00.000000+00:00", 0},
    {AMP_OUT_OF_RANGE, "0001-13-01T00:00:00.000000+00:00", 0},

    /* bad day */
    {AMP_OUT_OF_RANGE, "0001-01-00T00:00:00.000000+00:00", 0},
    {AMP_OUT_OF_RANGE, "0001-01-32T00:00:00.000000+00:00", 0},

    /* bad hour */
    {AMP_OUT_OF_RANGE, "0001-01-01T24:00:00.000000+00:00", 0},

    /* bad minute */
    {AMP_OUT_OF_RANGE, "0001-01-01T00:60:00.000000+00:00", 0},

    /* bad second */
    {AMP_OUT_OF_RANGE, "0001-01-01T00:00:60.000000+00:00", 0},

    /* bad offset */
    {AMP_OUT_OF_RANGE, "0001-01-01T00:00:00.000000+24:00", 0},
    {AMP_OUT_OF_RANGE, "0001-01-01T00:00:00.000000+00:60", 0},

    {AMP_OUT_OF_RANGE, "0001-01-01T00:00:00.000000-24:00", 0},
    {AMP_OUT_OF_RANGE, "0001-01-01T00:00:00.000000-00:60", 0},
};
int num_get_dt_tests = (sizeof get_dt_cases /
                        sizeof get_dt_cases[0]);


START_TEST(test__amp_get_datetime)
{
    int ret;
    AMP_DateTime dt;

    memset(&dt, 0, sizeof(dt));

    /* the variable _i is made available by Check's "loop test" machinery */
    struct get_dt_case c = get_dt_cases[_i];

    AMP_Box_p box = amp_new_box();

    amp_put_cstring(box, "key", c.testValue);

    ret = amp_get_datetime(box, "key", &dt);

    fail_unless(ret == c.expectedError);

    if (!c.expectedError)
    {
        fail_if( memcmp(&dt, &(c.expectedResult), sizeof(dt)) );
    }

    amp_free_box(box);
}
END_TEST


Suite *make_types_suite()
{

    /* init some test data */

    /* for long long tests */
    snprintf(ll_max, 100, "%lld", LLONG_MAX);
    snprintf(ll_min, 100, "%lld", LLONG_MIN);

    int llMaxLen;
    int llMinLen;
    llMaxLen = strlen(ll_max);
    llMinLen = strlen(ll_min);

    /* Increment string rep of LLONG_MAX so it is just above acceptable range */
    strncpy(ll_maxPlusOne, ll_max, 100);
    incrementDigit(&ll_maxPlusOne[llMaxLen-1]);


    /* Decrement string rep of LLONG_MAX so it is just below acceptable range */
    strncpy(ll_minMinusOne, ll_min, 100);
    /* Since it is a negative number.. incrementing the integer portion makes it smaller */
    incrementDigit(&ll_minMinusOne[llMinLen-1]);


    /* for int tests */
    snprintf(int_max, 100, "%d", INT_MAX);
    snprintf(int_min, 100, "%d", INT_MIN);

    int intMaxLen;
    int intMinLen;
    intMaxLen = strlen(int_max);
    intMinLen = strlen(int_min);

    /* Increment string rep of INT_MAX so it is just above acceptable range */
    strncpy(int_maxPlusOne, int_max, 100);
    incrementDigit(&int_maxPlusOne[intMaxLen-1]);


    /* Decrement string rep of INT_MAX so it is just below acceptable range */
    strncpy(int_minMinusOne, int_min, 100);
    /* Since it is a negative number.. incrementing the integer portion makes it smaller */
    incrementDigit(&int_minMinusOne[intMinLen-1]);


    /* for uint tests */
    snprintf(uint_max, 100, "%u", UINT_MAX);

    int uintMaxLen = strlen(uint_max);

    /* Increment string rep of UINT_MAX so it is just above acceptable range */
    strncpy(uint_maxPlusOne, uint_max, 100);
    incrementDigit(&uint_maxPlusOne[uintMaxLen-1]);

    /* done init */

    Suite *s = suite_create ("Types");

    TCase *tc_long_long = tcase_create("long long");
    tcase_add_loop_test(tc_long_long, test__amp_put_long_long, 0,
                        num_put_ll_tests);
    tcase_add_loop_test(tc_long_long, test__amp_get_long_long, 0,
                        num_get_ll_tests);
    tcase_add_test(tc_long_long, test__amp_get_long_long__no_such_key);
    suite_add_tcase(s, tc_long_long);

    TCase *tc_int = tcase_create("int");
    tcase_add_loop_test(tc_int, test__amp_put_int, 0,
                        num_put_int_tests);
    tcase_add_loop_test(tc_int, test__amp_get_int, 0,
                        num_get_int_tests);
    suite_add_tcase(s, tc_int);

    TCase *tc_uint = tcase_create("uint");
    tcase_add_loop_test(tc_uint, test__amp_get_uint, 0,
                        num_get_uint_tests);
    tcase_add_loop_test(tc_uint, test__amp_put_uint, 0,
                        num_put_uint_tests);
    tcase_add_test(tc_uint, test__amp_get_uint__no_such_key);
    suite_add_tcase(s, tc_uint);

    TCase *tc_double = tcase_create("double");
    tcase_add_loop_test(tc_double, test__amp_put_double, 0,
                        num_put_double_tests);

    tcase_add_loop_test(tc_double, test__amp_get_double__normal, 0,
                        num_get_double_tests);

    tcase_add_loop_test(tc_double, test__amp_get_double__special, 0,
                        num_get_double_special_tests);
    tcase_add_test(tc_double, test__amp_get_double__no_such_key);
    suite_add_tcase(s, tc_double);

    TCase *tc_bool = tcase_create("bool");
    tcase_add_loop_test(tc_bool, test__amp_put_bool, 0, num_put_bool_tests);
    tcase_add_loop_test(tc_bool, test__amp_get_bool, 0, num_get_bool_tests);
    tcase_add_test(tc_bool, test__amp_get_bool__no_such_key);
    suite_add_tcase(s, tc_bool);

    TCase *tc_dt = tcase_create("datetime");
    tcase_add_loop_test(tc_dt, test__amp_put_datetime, 0, num_put_dt_tests);
    tcase_add_loop_test(tc_dt, test__amp_get_datetime, 0, num_get_dt_tests);
    suite_add_tcase(s, tc_dt);

    return s;
}
