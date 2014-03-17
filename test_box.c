/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Check - C unit testing framework */
#include <check.h>

#include "amp.h"
#include "amp_internal.h"
#include "test.h"

/* TODO:
 * - Rename/refactor tests to match the convention in test_types.c
 */


const char *validKeys[] = {
    "someKey",

    /* 255-byte string */
    "max len key max len key max len "
    "max len key max len key max len "
    "max len key max len key max len "
    "max len key max len key max len "

    "max len key max len key max len "
    "max len key max len key max len "
    "max len key max len key max len "
    "max len key max len key max len"
};
int num_validKeys = (sizeof(validKeys) / sizeof(char *));

const char *invalidKeys[] = {
    "",

    /* 256-byte string */
    "OVER LENGTH KEY 1 BYTE TOO LONG "
    "key over len key over len key   "
    "key over len key over len key   "
    "key over len key over len key   "

    "key over len key over len key   "
    "key over len key over len key   "
    "key over len key over len key   "
    "key over len key over len key   "
};
int num_invalidKeys = (sizeof(invalidKeys) / sizeof(char *));


/* Use for false positive checks */
const char *randomKeys[] = { "Michael Jackson", "Bladerunner",
                             "OpenBSD"};
int num_randomKeys = (sizeof(randomKeys) / sizeof(char *));

AMP_Box_T *test_box;



void box_setup ()
{
    test_box = amp_new_box();
}

void box_teardown ()
{
    amp_free_box(test_box);
}

/* Verify that amp_put_*() accepts as keys byte arrays
 * with length greater than 0 and less than 256 */
START_TEST(test_box_put_valid_keys)
{
    fail_unless(amp_put_cstring(test_box, validKeys[_i], "blah") == 0);
}
END_TEST

/* Verify that amp_has_key() finds inserted keys */
START_TEST(test_box_has_valid_keys)
{
    fail_unless(amp_put_cstring(test_box, validKeys[_i], "foo") == 0);
    fail_unless(amp_has_key(test_box, validKeys[_i]));
}
END_TEST

/* Verify that amp_put_*() does not insert keys where
 * length is less than 1 or greater than 255 bytes
 *
 * Since all amp_put_* functions are implemented in terms
 * of _amp_put_buf() - which does the sanity checking - this
 * test should be enough to verify that all amp_put_*
 * functions enforce sane key length. */
START_TEST(test_box_put_invalid_keys)
{
    int ret;

    ret = amp_put_cstring(test_box, invalidKeys[_i], "blob");
    fail_unless(ret == AMP_BAD_KEY_SIZE,
                "amp_put_cstring() != %d (got %d)\n", AMP_BAD_KEY_SIZE, ret);

    /* make sure it doesn't actually have the key */
    ret = amp_has_key(test_box, invalidKeys[_i]);
    fail_unless(ret == 0, "amp_has_key() != 0 (got %d)\n", ret);
}
END_TEST

/* Verify that amp_has_key() doesn't return false positives */
START_TEST(test_box_not_has_keys)
{
    fail_if(amp_has_key(test_box, randomKeys[_i]));
}
END_TEST

/* Verify that amp_put_bytes() accepts a zero-length value, or a
 * max-length value */
START_TEST(test_box_put_good_values)
{
    unsigned char *data = MALLOC(MAX_VALUE_LENGTH);
    unsigned char *buf;
    int bufSize;

    fail_unless(amp_put_bytes(test_box, "key", data, MAX_VALUE_LENGTH) == 0);

    fail_unless(amp_get_bytes(test_box, "key", &buf, &bufSize) == 0);
    fail_unless(bufSize == MAX_VALUE_LENGTH);

    fail_unless(amp_put_bytes(test_box, "key", data, 0) == 0);

    fail_unless(amp_get_bytes(test_box, "key", &buf, &bufSize) == 0);
    fail_unless(bufSize == 0);

    free(data);
}
END_TEST

/* Verify that amp_put_bytes() rejects values with
 * size > MAX_VALUE_LENGTH or < 0.
 */
START_TEST(test_box_put_bad_values)
{
    unsigned char *data = MALLOC(MAX_VALUE_LENGTH+1);

    fail_unless(amp_put_bytes(test_box, "key", data, MAX_VALUE_LENGTH+1) == AMP_BAD_VAL_SIZE);
    fail_unless(amp_put_bytes(test_box, "key", data, -1) == AMP_BAD_VAL_SIZE);

    free(data);
}
END_TEST


/* Stress test box creation and key addition */
START_TEST(test_stress_box_allocation)
{
    int i, j;
    char key[20];
    AMP_Box_T *boxes[100];
    unsigned char *buf;
    int bufSize;
    const char *testValue = "some test value";

    for (i = 0; i < 100; i++) {
        boxes[i] = amp_new_box();
        fail_if(boxes[i] == NULL, "Couldn't allocate box\n");

        for (j = 0; j < 100; j++) {
            snprintf(key, 20, "%06d", j);
            fail_unless(amp_put_bytes(boxes[i], key, testValue, strlen(testValue)+1) == 0);
        }
    }

    for (i = 0; i < 100; i++) {
        fail_unless(amp_num_keys(boxes[i]) == 100, "Wrong number of keys\n");

        for (j = 0; j < 100; j++) {
            snprintf(key, 20, "%06d", j);
            fail_unless(amp_get_bytes(boxes[i], key, &buf, &bufSize) == 0);
            fail_unless( strcmp(testValue, buf) == 0, "Values aren't equal\n");
            fail_unless( amp_del_key(boxes[i], key) == 0, "Couldn't delete key\n");
        }
        fail_unless(amp_num_keys(boxes[i]) == 0, "Box not empty\n");
        amp_free_box(boxes[i]);
    }
}
END_TEST


START_TEST(test__boxes_equal__yes)
{
    AMP_Box_T *a = amp_new_box();
    AMP_Box_T *b = amp_new_box();

    amp_put_cstring(a, "foo", "FOO");
    amp_put_cstring(a, "bar", "BAR");

    amp_put_cstring(b, "foo", "FOO");
    amp_put_cstring(b, "bar", "BAR");

    fail_unless( amp_boxes_equal(a, b) );

    amp_free_box(a);
    amp_free_box(b);
}
END_TEST


START_TEST(test__boxes_equal__diff_length)
{
    AMP_Box_T *a = amp_new_box();
    AMP_Box_T *b = amp_new_box();

    amp_put_cstring(a, "foo", "FOO");
    amp_put_cstring(a, "bar", "BAR");
    amp_put_cstring(a, "baz", "BAR");

    amp_put_cstring(b, "foo", "FOO");
    amp_put_cstring(b, "bar", "BAR");

    fail_unless( !amp_boxes_equal(a, b) );

    amp_free_box(a);
    amp_free_box(b);
}
END_TEST


START_TEST(test__boxes_equal__diff_keys)
{
    AMP_Box_T *a = amp_new_box();
    AMP_Box_T *b = amp_new_box();

    amp_put_cstring(a, "foo", "FOO");
    amp_put_cstring(a, "BAR", "BAR");

    amp_put_cstring(b, "foo", "FOO");
    amp_put_cstring(b, "bar", "BAR");

    fail_unless( !amp_boxes_equal(a, b) );

    amp_free_box(a);
    amp_free_box(b);
}
END_TEST


START_TEST(test__boxes_equal__diff_value_size)
{
    AMP_Box_T *a = amp_new_box();
    AMP_Box_T *b = amp_new_box();

    amp_put_cstring(a, "foo", "FOO");
    amp_put_cstring(a, "bar", "BAR");

    amp_put_cstring(b, "foo", "FOOOOO");
    amp_put_cstring(b, "bar", "BAR");

    fail_unless( !amp_boxes_equal(a, b) );

    amp_free_box(a);
    amp_free_box(b);
}
END_TEST


START_TEST(test__boxes_equal__diff_value_content)
{
    AMP_Box_T *a = amp_new_box();
    AMP_Box_T *b = amp_new_box();

    amp_put_cstring(a, "foo", "FOO");
    amp_put_cstring(a, "bar", "BAR");

    amp_put_cstring(b, "foo", "FoO");
    amp_put_cstring(b, "bar", "BAR");

    fail_unless( !amp_boxes_equal(a, b) );

    amp_free_box(a);
    amp_free_box(b);
}
END_TEST


unsigned int fake_hash(const void *key1)
{
    return 5;
}


START_TEST(test_box_has_key_corner_cases)
{
    AMP_Box_T *box = amp_new_box();
    box->hash = fake_hash; /* use same bucket */
    amp_put_cstring(box, "foo", "FOO");
    amp_put_cstring(box, "bar", "BAR");

    fail_unless( amp_has_key(box, "bar") );

    fail_unless( !amp_has_key(box, "fooooo") );
    fail_unless( !amp_has_key(box, "baz") );

    amp_free_box(box);
}
END_TEST


START_TEST(_amp_box_get_buf__key_not_found)
{
    unsigned char *buf;
    int buf_size, result;
    AMP_Box_T *box = amp_new_box();
    result = _amp_get_buf(box, "foo", &buf, &buf_size);
    fail_unless( result == AMP_KEY_NOT_FOUND );
    amp_free_box(box);
}
END_TEST


/* amp_del_key() tests */
static const char key1[] = "key1";
static const char key2[] = "key2";
static const char key3[] = "key3";

static const char val1[] = "val1";
static const char val2[] = "val2";
static const char val3[] = "val3";

void amp_del_key__setup()
{
    test_box = amp_new_box();

    /* override the hash function so that all keys fall in the same bucket */
    test_box->hash = fake_hash;

    amp_put_bytes(test_box, key1, val1, strlen(val1)+1);
    amp_put_bytes(test_box, key2, val2, strlen(val2)+1);
    amp_put_bytes(test_box, key3, val3, strlen(val3)+1);
}

void amp_del_key__teardown()
{
    amp_free_box(test_box);
}

START_TEST(test_amp_del_key__head)
{
    /* delete the key which is at the head of the bucket linked list */
    unsigned char *buf;
    int bufSize;

    int ret;
    fail_unless((ret = amp_del_key(test_box, key3)) == 0, "amp_del_key() failed. return code == %d", ret);

    fail_unless(amp_num_keys(test_box) == 2, "Wrong number of keys");

    /* still has other key values? */
    fail_unless((ret = amp_get_bytes(test_box, key1, &buf, &bufSize)) == 0,
                "amp_get_bytes() failed (ret code == %d", ret);
    fail_unless(strcmp(val1, buf) == 0);

    fail_unless((ret = amp_get_bytes(test_box, key2, &buf, &bufSize)) == 0,
                "amp_get_bytes() failed (ret code == %d", ret);
    fail_unless(strcmp(val2, buf) == 0);
}
END_TEST

START_TEST(test_amp_del_key__tail)
{
    /* delete the key which is at the tail of the bucket linked list */
    unsigned char *buf;
    int bufSize;

    int ret;
    fail_unless( (ret = amp_del_key(test_box, key1)) == 0, "amp_del_key() failed. return code == %d", ret);

    fail_unless( amp_num_keys(test_box) == 2, "Wrong number of keys");

    /* still has other key values? */
    fail_unless((ret = amp_get_bytes(test_box, key2, &buf, &bufSize)) == 0,
                "amp_get_bytes() failed (ret code == %d", ret);
    fail_unless(strcmp(val2, buf) == 0);

    fail_unless((ret = amp_get_bytes(test_box, key3, &buf, &bufSize)) == 0,
                "amp_get_bytes() failed (ret code == %d", ret);
    fail_unless(strcmp(val3, buf) == 0);

}
END_TEST

START_TEST(test_amp_del_key__mid)
{
    /* delete the key which is in the middle of the bucket linked list */
    unsigned char *buf;
    int bufSize;

    int ret;
    fail_unless( (ret = amp_del_key(test_box, key2)) == 0, "amp_del_key() failed. return code == %d", ret);

    fail_unless( amp_num_keys(test_box) == 2, "Wrong number of keys");

    /* still has other key values? */
    fail_unless((ret = amp_get_bytes(test_box, key1, &buf, &bufSize)) == 0,
                "amp_get_bytes() failed (ret code == %d", ret);
    fail_unless(strcmp(val1, buf) == 0);

    fail_unless((ret = amp_get_bytes(test_box, key3, &buf, &bufSize)) == 0,
                "amp_get_bytes() failed (ret code == %d", ret);
    fail_unless(strcmp(val3, buf) == 0);
}
END_TEST

START_TEST(test_amp_del_key__all)
{
    /* delete all the keys */
    fail_unless( amp_num_keys(test_box) == 3, "Doesn't have keys?");

    amp_del_key(test_box, key1);
    amp_del_key(test_box, key2);
    amp_del_key(test_box, key3);

    fail_unless( amp_num_keys(test_box) == 0, "Still has keys");
}
END_TEST

START_TEST(test_amp_del_key__missing)
{
    /* try to delete a missing */
    fail_unless(amp_num_keys(test_box) == 3);

    fail_unless(amp_del_key(test_box, "missing") == -1);

    fail_unless(amp_num_keys(test_box) == 3);
}
END_TEST

void amp_serialize_box__setup()
{
    test_box = amp_new_box();
}

void amp_serialize_box__teardown()
{
    amp_free_box(test_box);
}

/*
 * Expected result generated via:
 *
 * from twisted.protocols.amp import AmpBox; import binascii
 * box = AmpBox()
 * box['key1'] = 'val1'
 * box['key2'] = 'val2'
 * hex = binascii.hexlify(box.serialize())
 * print ', '.join(['0x'+a[0]+a[1] for a in zip(hex[::2], hex[1::2])])
 *
 */

START_TEST(test_amp_serialize_box__valid)
{
    unsigned char *buf;
    int bufSize;
    const char expectedResult[] = { 0x00, 0x04, 0x6b, 0x65, 0x79,
                                    0x31, 0x00, 0x04, 0x76, 0x61,
                                    0x6c, 0x31, 0x00, 0x04, 0x6b,
                                    0x65, 0x79, 0x32, 0x00, 0x04,
                                    0x76, 0x61, 0x6c, 0x32, 0x00,
                                    0x00 };

    amp_put_cstring(test_box, "key1", "val1");
    amp_put_cstring(test_box, "key2", "val2");

    /* verify _all_ allocated bytes gets set */
    mem_check_char = '\1';

    fail_unless(amp_serialize_box(test_box, &buf, &bufSize) == 0);

    /* reset */
    mem_check_char = '\0';

    fail_if(memcmp(buf, expectedResult, bufSize));
    free(buf);
}
END_TEST

START_TEST(test_amp_serialize_box__empty)
{
    unsigned char *buf;
    int bufSize;

    /* Verify amp_serialize_box() returns AMP_BOX_EMPTY when box is empty */
    fail_unless(amp_serialize_box(test_box, &buf, &bufSize) == AMP_BOX_EMPTY);
}
END_TEST

Suite *make_box_suite() {
    Suite *s = suite_create ("Box");

    TCase *tc_box = tcase_create("box");
    tcase_add_checked_fixture(tc_box, box_setup, box_teardown);
    tcase_add_loop_test(tc_box, test_box_put_valid_keys, 0, num_validKeys);
    tcase_add_loop_test(tc_box, test_box_has_valid_keys, 0, num_validKeys);
    tcase_add_loop_test(tc_box, test_box_put_invalid_keys, 0, num_invalidKeys);
    tcase_add_loop_test(tc_box, test_box_not_has_keys, 0, num_randomKeys);

    tcase_add_test(tc_box, test_box_put_good_values);
    tcase_add_test(tc_box, test_box_put_bad_values);

    tcase_add_test(tc_box, test_stress_box_allocation);

    tcase_add_test(tc_box, test__boxes_equal__yes);
    tcase_add_test(tc_box, test__boxes_equal__diff_length);
    tcase_add_test(tc_box, test__boxes_equal__diff_keys);
    tcase_add_test(tc_box, test__boxes_equal__diff_value_size);
    tcase_add_test(tc_box, test__boxes_equal__diff_value_content);

    tcase_add_test(tc_box, test_box_has_key_corner_cases);

    tcase_add_test(tc_box, _amp_box_get_buf__key_not_found);
    suite_add_tcase(s, tc_box);

    TCase *tc_del_key = tcase_create("box delete keys");
    tcase_add_checked_fixture(tc_del_key, amp_del_key__setup,
                                          amp_del_key__teardown);
    tcase_add_test(tc_del_key, test_amp_del_key__head);
    tcase_add_test(tc_del_key, test_amp_del_key__tail);
    tcase_add_test(tc_del_key, test_amp_del_key__mid);
    tcase_add_test(tc_del_key, test_amp_del_key__all);
    tcase_add_test(tc_del_key, test_amp_del_key__missing);
    suite_add_tcase(s, tc_del_key);

    TCase *tc_serialize_box =  tcase_create("serialize box");
    tcase_add_checked_fixture(tc_serialize_box, amp_serialize_box__setup,
                                                amp_serialize_box__teardown);
    tcase_add_test(tc_serialize_box, test_amp_serialize_box__valid);
    tcase_add_test(tc_serialize_box, test_amp_serialize_box__empty);
    suite_add_tcase(s, tc_serialize_box);

    return s;
}
