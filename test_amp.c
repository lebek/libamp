/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/* TODO
 *
 *  -  Add a benchmark test-case for encoding, and for decoding AMP packets
 *     to/from memory buffers.
 *
 *     This will allow us to tweak and optimize the parser and encoder
 *     routines, while having concrete feedback about the impact those
 *     tweaks have.
 *
 *  - Optimize AMP-parser state machine. amp_parse_box() in amp.c
 *    State-machine states (KEY_LEN_READ, etc) should be bit-wise flags
 *    and testing for the flag should be a bitwise-AND operation, which
 *    may be faster(?)
 */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

/* Check - C unit testing framework */
#include <check.h>

#include "amp.h"
#include "amp_internal.h"
#include "list.h"
#include "test.h"
#include "dispatch.h"
#include "table.h"
#include "unix_string.h"

struct consume_case
{
    unsigned char *buf;
    int bufSize;
    int retVal;
    char *expectedResult;
};

unsigned char validBox[] = {
    0x00, 0x07, /* key of length 7 follows */
    0x5F, 0x61, 0x6E, 0x73, 0x77, 0x65, 0x72,
    0x00, 0x02, /* value of length 2 follows */
    0x32, 0x33,
    0x00, 0x05, /* key of length 5 follows */
    0x74, 0x6F, 0x74, 0x61, 0x6C,
    0x00, 0x02, /* value of length 2 follows */
    0x39, 0x34,
    0x00, 0x00, /* empty key terminates the box */
};

unsigned char badKeySizeBox[] = {
    0x04, 0x07, /* key length > 255 */
    0x5F, 0x61, 0x6E, 0x73, 0x77, 0x65,
    0x72, 0x00, 0x02, 0x32, 0x33, 0x00, 0x05, 0x74,
    0x6F, 0x74, 0x61, 0x6C, 0x00, 0x02, 0x39, 0x34,
    0x00, 0x00 };

unsigned char emptyBox[] = { 0x00, 0x00 };

struct consume_case consume_cases[] = {
    { validBox,      sizeof(validBox),      0,                "23"},
    { badKeySizeBox, sizeof(badKeySizeBox), AMP_BAD_KEY_SIZE, ""  },
};
int num_consume_tests = (sizeof(consume_cases) /
                         sizeof(struct consume_case));

List_T *saved_boxes = NULL;

static int save_box(AMP_Proto_T *proto, AMP_Box_T *box)
{
    int err;
    saved_boxes = List_push(saved_boxes, box, &err);
    return 0;
}

/* A dispatch function that just returns a code */
int dispatch_fail_code;
static int dispatch_fail(AMP_Proto_T *proto, AMP_Box_T *box)
{
    return dispatch_fail_code;
}

List_T *saved_results = NULL;
struct saved_result
{
    AMP_Proto_T *proto;
    AMP_Result_T *result;
    void *callback_arg;
};
void save_result_cb(AMP_Proto_T *proto, AMP_Result_T *result,
                    void *callback_arg)
{
    int err;
    struct saved_result *r;
    r = MALLOC(sizeof(*r));

    r->proto = proto;
    r->result = result;
    r->callback_arg = callback_arg;
    saved_results = List_push(saved_results, r, &err);
}

void ignore_result_cb(AMP_Proto_T *proto, AMP_Result_T *result,
                      void *callback_arg)
{
    amp_free_result(result);
}

int discarding_write_handler(AMP_Proto_T *proto, unsigned char *buf,
                             int bufSize, void *write_arg)
{
    free(buf);
    return 0;
}

List_T *saved_writes = NULL;
struct saved_write
{
    AMP_Proto_T *proto;
    void *write_arg;
    AMP_Chunk_T *chunk;
};
int save_writes(AMP_Proto_T *proto, unsigned char *buf, int bufSize,
                void *write_arg)
{
    int err;
    struct saved_write *w;
    w = MALLOC(sizeof(*w));

    w->proto = proto;
    w->write_arg = write_arg;
    w->chunk = amp_chunk_for_buffer(buf, bufSize);
    saved_writes = List_push(saved_writes, w, &err);
    return 0;
}

AMP_Proto_T *test_proto;
void core_setup ()
{
    test_proto = amp_new_proto();
    amp_set_write_handler(test_proto, discarding_write_handler, NULL);
}

static void free_result(void **x, void*cl)
{
    amp_free_result(*x);
}
static void free_box(void **x, void*cl)
{
    amp_free_box(*x);
}

void core_teardown ()
{
    amp_free_proto(test_proto);
    test_proto = NULL;

    /* Free any list items and associated objects */
    List_map(saved_boxes, free_box, NULL);
    List_free(&saved_boxes);

    List_map(saved_results, free_result, NULL);
    List_free(&saved_results);
}


/* Uses core_setup()  and core_teardown()
 */
START_TEST(test__amp_consume_bytes)
{
    /* XXX TODO this test case seems really crummy */

    /* the variable _i is made available by Check's "loop test" machinery */
    unsigned char *buf;
    int bufSize;
    AMP_Box_T *gotBox;
    struct consume_case c;

    c = consume_cases[_i];

    /* bypass _amp_process_full_packet machinery */
    test_proto->dispatch_box = save_box;

    int ret = amp_consume_bytes(test_proto, c.buf, c.bufSize);

    fail_unless(ret == c.retVal);

    if (ret == 0)
    {
        fail_unless(List_length(saved_boxes) == 1);

        saved_boxes = List_pop(saved_boxes, (void**)&gotBox);

        /* expected 0 return */
        fail_if( amp_get_bytes(gotBox, "_answer", &buf, &bufSize) );

        /* expected 0 return (memory equal) */
        fail_if( memcmp(buf, c.expectedResult, strlen(c.expectedResult)) );

        amp_free_box(gotBox);
    }
    else
    {
        /* Verify AMP_Proto is in a fatal error state */
        fail_unless(test_proto->error);

        /* Verify that amp_consume_bytes refuses to do any work when
         * AMP_Proto is in the fatal error state */
        fail_unless(amp_consume_bytes(test_proto, '\0', 1) == AMP_PROTO_ERROR);
    }
}
END_TEST

START_TEST(test__amp_consume_bytes__multiple_boxes)
{
    /* This test is looped three times - the fist time we feed in all the boxes
     * in one big buffer. The second time, we feed it in one byte at a time,
     * and the 3rd time we feed it in 2 bytes at a time */

    /* the variable _i is made available by Check's "loop test" machinery */

    unsigned char block[512];
    unsigned char *buf;
    int bufSize;
    int blockSize = 0;
    AMP_Box_T *box, *box2, *box3;

    /* build buffer with 3 boxes in it */

    /* box 1 */
    box = amp_new_box();
    amp_put_cstring(box, COMMAND, "Cat");
    amp_put_cstring(box, "foo", "FOO");
    amp_put_cstring(box, "bar", "BAR");
    amp_put_int(box, ASK, 1);

    amp_serialize_box(box, &buf, &bufSize);

    memcpy(&block[blockSize], buf, bufSize);
    free(buf);
    blockSize += bufSize;

    /* box 2 */
    box2 = amp_new_box();

    amp_put_int(box2, ANSWER, 123);
    amp_put_int(box2, "answer", 42);

    amp_serialize_box(box2, &buf, &bufSize);

    memcpy(&block[blockSize], buf, bufSize);
    free(buf);
    blockSize += bufSize;

    /* box 3 */
    box3 = amp_new_box();

    amp_put_int(box3, _ERROR, 456);
    amp_put_cstring(box3, ERROR_CODE, "BAD");

    amp_serialize_box(box3, &buf, &bufSize);

    memcpy(&block[blockSize], buf, bufSize);
    free(buf);
    blockSize += bufSize;


    /* feed to amp_consume_bytes() */

    AMP_Proto_T *proto = amp_new_proto();

    /* captured dispatched boxes in a List_T* */
    proto->dispatch_box = save_box;

    int idx;
    switch(_i)
    {
        case 0:
            /* all at once */
            fail_if(amp_consume_bytes(proto, block, blockSize));
            break;
        case 1:
            /* one byte at a time */
            for (idx = 0; idx < blockSize; idx++)
            {
                fail_if(amp_consume_bytes(proto, &block[idx], 1));
            }
            break;
        case 2:
            /* two bytes at a time */

            idx = 0;
            /* if odd number of bytes - feed 1 byte first */
            if (blockSize % 2 == 1)
            {
                fail_if(amp_consume_bytes(proto, &block[0], 1));
                idx++;
            }

            for (; idx < blockSize; idx+=2)
            {
                fail_if(amp_consume_bytes(proto, &block[idx], 2));
            }
            break;
    }
    amp_free_proto(proto);

    /* 3 boxes should have been captured */

    fail_unless(List_length(saved_boxes) == 3);

    saved_boxes = List_reverse(saved_boxes);

    AMP_Box_T *gotBox;

    /* check box 1 - a _command box */
    saved_boxes = List_pop(saved_boxes, (void**)&gotBox);
    fail_unless( amp_boxes_equal(gotBox, box) );
    amp_free_box(gotBox);

    /* check box 2 - an _answer box */
    saved_boxes = List_pop(saved_boxes, (void**)&gotBox);
    fail_unless( amp_boxes_equal(gotBox, box2) );
    amp_free_box(gotBox);

    /* check box 3 - an _error box */
    saved_boxes = List_pop(saved_boxes, (void**)&gotBox);
    fail_unless( amp_boxes_equal(gotBox, box3) );
    amp_free_box(gotBox);

    amp_free_box(box);
    amp_free_box(box2);
    amp_free_box(box3);
}
END_TEST


START_TEST(test__amp_consume_bytes__dispatch_failure)
{
    int result;
    AMP_Proto_T *proto = amp_new_proto();

    dispatch_fail_code = 77;
    proto->dispatch_box = dispatch_fail;

    result = amp_consume_bytes(proto, validBox, sizeof(validBox));

    fail_unless(result == 77);
    fail_unless(proto->error == 77);

    amp_free_proto(proto);
}
END_TEST


START_TEST(test__amp_parse_box__invalid_state)
{
    /* If AMP_Proto_T *->state is not a known value, parsing should fail with AMP_INTERNAL_ERROR.
     * This should never happen, but it doesn't hurt to have test coverage for it */
    int result;

    test_proto->state = 666;

    result = amp_consume_bytes(test_proto, "parseme", 7);

    fail_unless(result == AMP_INTERNAL_ERROR);
    fail_unless(test_proto->error == AMP_INTERNAL_ERROR);
}
END_TEST


START_TEST(dispatch_handled_responses)
{
    /* XXX TODO test with multiple callbacks set, responses arriving
     * out of original order */

    AMP_Box_T *args, *answer, *test_box;
    unsigned int askKey, askKey2, askKey3;

    args = amp_new_box();

    /* Register some callbacks */
    amp_call(test_proto, "doSomething", args,
             save_result_cb, NULL, &askKey);
    amp_call(test_proto, "doSomethingBad", args,
             save_result_cb, NULL, &askKey2);
    amp_call(test_proto, "doSomethingBoring", args,
             save_result_cb, NULL, &askKey3);

    amp_free_box(args);


    /* Cancel a request */
    amp_cancel(test_proto, askKey3);

    /* Contrive a successful response - this should be ignored since
     * the request was cancelled */
    amp_put_int(test_proto->box, ANSWER, askKey3);

    fail_if( test_proto->dispatch_box(test_proto, test_proto->box));

    /* Verify our registered callback got 1 (and only 1) result... */
    fail_unless(List_length(saved_results) == 1);

    /* ...and that it contained the cancellation we expected... */
    struct saved_result *r;

    saved_results = List_pop(saved_results, (void**)&r);

    fail_unless(r->result->reason == AMP_CANCEL);
    fail_unless(r->result->error == NULL);
    fail_unless(r->result->response == NULL);
    amp_free_result(r->result);
    free(r);

    /* Contrive a successful response */
    test_box = amp_new_box();
    test_proto->box = test_box;
    amp_put_int(test_box, ANSWER, askKey);

    fail_if( test_proto->dispatch_box(test_proto, test_box));

    /* Verify our registered callback got a result... */
    fail_unless(List_length(saved_results) == 1);

    /* ...and that it contained the response we expected... */
    saved_results = List_pop(saved_results, (void**)&r);

    fail_unless(r->result->reason == AMP_SUCCESS);
    fail_unless( (r->result->response)->args == test_box);
    fail_unless(r->result->error == NULL);
    amp_free_result(r->result);
    free(r);

    /* Contrive an error response */
    test_box = amp_new_box();
    test_proto->box = test_box;
    char *testErr = "TEST_ERR";
    amp_put_int(test_box, _ERROR, askKey2);
    amp_put_cstring(test_box, ERROR_CODE, testErr);

    fail_if( test_proto->dispatch_box(test_proto, test_box));

    /* Verify our registered callback got a result... */
    fail_unless(List_length(saved_results) == 1);

    /* ...and that it contained the error we expected... */
    saved_results = List_pop(saved_results, (void**)&r);

    fail_unless(r->result->reason == AMP_ERROR);
    fail_if( memcmp(testErr, r->result->error->error_code->value,
                             r->result->error->error_code->size) );
    fail_unless(r->result->response == NULL);
    amp_free_result(r->result);
    free(r);
}
END_TEST

List_T *saved_requests = NULL;

static void sum_responder(AMP_Proto_T *proto, AMP_Request_T *request,
                               void *responder_arg)
{
    int err;
    saved_requests = List_push(saved_requests, request, &err);
}

static void multiply_responder(AMP_Proto_T *proto, AMP_Request_T *request,
                                    void *responder_arg)
{
    int err;
    saved_requests = List_push(saved_requests, request, &err);
}

START_TEST(dispatch_handled_requests)
{
    /* Verify that registered responders recieve their corresponding requests */
    AMP_Box_T *test_box;
    amp_add_responder(test_proto, "Sum", sum_responder, NULL);
    amp_add_responder(test_proto, "Multiply", multiply_responder, NULL);

    /* Generate Sum command */

    /* An AMP_Proto starts off with an empty box - so don't need to
     * manaully allocate one */
    test_box = test_proto->box;
    amp_put_int(test_box, ASK, 1);
    amp_put_cstring(test_box, COMMAND, "Sum");
    amp_put_int(test_box, "a", 3);
    amp_put_int(test_box, "b", 8);

    /* Dispatch Sum command */
    fail_if( test_proto->dispatch_box(test_proto, test_box));

    fail_unless(List_length(saved_requests) == 1);

    AMP_Request_T *sumRequest;
    saved_requests = List_pop(saved_requests, (void**)&sumRequest);
    fail_unless(sumRequest->args == test_box);
    amp_free_request(sumRequest);

    /* Verify a dropped responder doesn't get called */
    test_box = amp_new_box();
    test_proto->box = test_box;
    amp_put_int(test_box, ASK, 2);
    amp_put_cstring(test_box, COMMAND, "Multiply");
    amp_put_int(test_box, "a", 9);
    amp_put_int(test_box, "b", 3);

    /* Remove the Multipy responder */

    amp_remove_responder(test_proto, "Multiply");

    fail_if( test_proto->dispatch_box(test_proto, test_box));

    fail_unless(List_length(saved_requests) == 0);
}
END_TEST

START_TEST(unhandled_request_sends_error)
{
    /* Verify that an incoming request (_command) box, with no
     * registered command handler, causes an error box to be sent. */
    AMP_Box_T *test_box;
    AMP_Box_T *box;
    AMP_Proto_T *proto;
    struct saved_write *write;
    int bytesConsumed;
    int writtenErrorKey;
    unsigned char *buf;
    int bufSize;

    /* An AMP_Proto starts off with an empty box - so don't need to
     * manaully allocate one */
    test_box = test_proto->box;
    amp_put_int(test_box, ASK, 2);
    amp_put_cstring(test_box, COMMAND, "BadCommand");

    /* capture outgoing data */
    amp_set_write_handler(test_proto, save_writes, NULL);

    /* try to dispatch an unknown command - which should result
     * in an error box being written */
    test_proto->dispatch_box(test_proto, test_box);

    fail_unless(List_length(saved_writes) == 1);

    saved_writes = List_pop(saved_writes, (void**)&write);

    fail_unless(write->proto == test_proto); /* write handler called with correct AMP_Proto pointer */

    /* parse the written bytes as an AMP_Box and make sure it looks right */
    proto = amp_new_proto();
    fail_unless( amp_parse_box(proto, proto->box, &bytesConsumed,
                               write->chunk->value,
                               write->chunk->size) );

    /* parsing the box used all bytes that were written? */
    fail_unless(bytesConsumed == write->chunk->size);

    free(write->chunk->value);
    amp_free_chunk(write->chunk);
    free(write);

    /* right number of keys written?
     * 1 for "_error" key
     * 1 for "_error_code" key
     * 1 for "_error_description" key */
    fail_unless(amp_num_keys(proto->box) == 3);

    /* CHECK "_error" key */

    /* expected 0 return */
    fail_if(amp_get_int(proto->box, _ERROR, &writtenErrorKey));

    /* same _error key written as received _ask key */
    fail_unless(writtenErrorKey == 2);

    /* CHECK "_error_code" key */

    /* expected 0 return */
    fail_if(amp_get_bytes(proto->box, ERROR_CODE, &buf, &bufSize));

    /* correct code? */
    fail_unless(bufSize == strlen(AMP_ERROR_UNHANDLED));

    /* expected 0 return (memory is equal) */
    fail_if(memcmp(buf, AMP_ERROR_UNHANDLED, bufSize));

    /* correct description? */

    /* expected 0 return */
    fail_if(amp_get_bytes(proto->box, ERROR_DESCR, &buf, &bufSize));

    char expected[] = "Unhandled Command: 'BadCommand'";
    fail_unless(bufSize == strlen(expected));

    /* expected 0 return (memory is equal) */
    fail_if(memcmp(buf, expected, bufSize));

    /* NOW test that an error is NOT written if no
     * _ask key was given */
    amp_free_box(test_proto->box);
    test_box = amp_new_box();
    test_proto->box = test_box;

    amp_put_cstring(test_box, COMMAND, "BadCommand");
    test_proto->dispatch_box(test_proto, test_box);

    fail_unless(List_length(saved_writes) == 0);

    amp_free_proto(proto);
}
END_TEST

START_TEST(dispatch_unhandled_boxes)
{
    /* Test that receiving boxes if no corresponding handler has been
     * set does not crash your program */
    fail_unless(test_proto->dispatch_box(test_proto, test_proto->box) == AMP_BOX_EMPTY);

    /* A command box */
    amp_put_cstring(test_proto->box, COMMAND, "Foo");

    fail_if(test_proto->dispatch_box(test_proto, test_proto->box));

    /* An answer box */
    test_proto->box = amp_new_box();
    amp_put_int(test_proto->box, ANSWER, 1);

    fail_if(test_proto->dispatch_box(test_proto, test_proto->box));

    /* An error box */
    test_proto->box = amp_new_box();
    amp_put_int(test_proto->box, _ERROR, 1);

    fail_if(test_proto->dispatch_box(test_proto, test_proto->box));
}
END_TEST


START_TEST(test_process_bad_box)
{
    AMP_Box_T *box = amp_new_box();

    fail_unless(_amp_process_full_packet(NULL, box) == AMP_BOX_EMPTY);
    amp_free_box(box);

    box = amp_new_box();
    amp_put_cstring(box, "foo", "bar");

    fail_unless(_amp_process_full_packet(NULL, box) == AMP_REQ_KEY_MISSING);
    amp_free_box(box);
}
END_TEST

START_TEST(test_process_with_no_command_handler)
{
    amp_put_cstring(test_proto->box, COMMAND, "SomeUnknownCommand");

    fail_unless(test_proto->dispatch_box(test_proto, test_proto->box) == 0);

    /* dispatch a second unhandled command box to ensure the first
     * one was free'd properly. `make valgrind' will detect the error
     * if it wasn't. */
    test_proto->box = amp_new_box();
    amp_put_cstring(test_proto->box, COMMAND, "AnotherUnknownCommand");

    fail_unless(test_proto->dispatch_box(test_proto, test_proto->box) == 0);
}
END_TEST

START_TEST(_amp_new_error_from_box__error_cases)
{
    int result;
    AMP_Box_T *box;
    AMP_Error_T *error = NULL;

    box = amp_new_box();
    amp_put_cstring(box, _ERROR, "1");
    amp_put_cstring(box, ERROR_CODE, "SOME_ERROR_CODE");
    amp_put_cstring(box, ERROR_DESCR, "Some error description.");

    /* all amp_get_* functions on this box, for the given key,
     * will fail with this code */
    box->get_fail_code = 77;
    box->get_fail_key = _ERROR;

    result = _amp_new_error_from_box(box, &error);

    fail_unless( result == 77 );
    fail_unless( error == NULL );

    /* exercise a different failure-path */

    box->get_fail_key = ERROR_CODE;
    result = _amp_new_error_from_box(box, &error);

    fail_unless( result == 77 );
    fail_unless( error == NULL );

    box->get_fail_key = ERROR_DESCR;
    result = _amp_new_error_from_box(box, &error);

    fail_unless( result == 77 );
    fail_unless( error == NULL );

    amp_free_box(box);
}
END_TEST


START_TEST(_amp_new_request_from_box__error_cases)
{
    int result;
    AMP_Box_T *box;
    AMP_Request_T *req = NULL;

    box = amp_new_box();
    amp_put_cstring(box, COMMAND, "SomeCommand");
    amp_put_cstring(box, ASK, "1");

    /* TEST SUPPORT: rig all amp_get_* functions on this box,
     * for the given key, to fail with this code. */
    box->get_fail_code = 77;
    box->get_fail_key = COMMAND;

    result = _amp_new_request_from_box(box, &req);

    fail_unless( result == 77 );
    fail_unless( req == NULL );

    /* exercise a different failure-path */

    box->get_fail_key = ASK;
    result = _amp_new_request_from_box(box, &req);

    fail_unless( result == 77 );
    fail_unless( req == NULL );

    amp_free_box(box);
}
END_TEST


START_TEST(_amp_remove_responder__not_found)
{
    _AMP_Responder_Map_p map = _amp_new_responder_map();

    /* just checking that it doesn't crash - and when run under valgrind
     * this test will guard against a lot of memory-related errors */
    _amp_remove_responder(map, "FooCommand");

    _amp_free_responder_map(map);
}
END_TEST


START_TEST(test_free_proto)
{
    /* test that amp_free_proto() also free's proto->box.
     * test_proto is created and free'd by core_setup/teardown */

    /* TODO - populate the callback map and any other dynamic data
     * structures contained by an AMP_Proto so we can be sure that
     * everything is really free'd up by amp_free_proto() */
    amp_put_cstring(test_proto->box, "someKey", "some junk");
}
END_TEST

void junk_callback(AMP_Proto_T *proto, AMP_Result_T *result,
                   void *callback_arg)
{
    amp_free_result(result);
}

START_TEST(test_process_with_malloc_failure)
{
    /* exercise _amp_process_full_packet() multiple times.
     * we set a counter which indicates the number of times malloc()
     * should succeed before returning NULL.
     * */

    int fail_after = 0;
    int result;

    /* COMMAND box */
    while (1)
    {
        amp_put_cstring(test_proto->box, COMMAND, "FooCommand");
        amp_put_int(test_proto->box, ASK, 1);

        enable_malloc_failures(fail_after++);

        /* run code under test */
        result = test_proto->dispatch_box(test_proto, test_proto->box);

        /* clean up and check result */
        disable_malloc_failures();
        amp_free_box(test_proto->box);
        test_proto->box = amp_new_box();

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            /* Yes */
            fail_unless(result == ENOMEM);
        }
        else
            /* The successful code path is already tested elsewhere. */
            break;
    }

    /* ANSWER box */
    int ask_key = 1;
    AMP_Proto_T *proto;
    _AMP_Callback_p cb;

    fail_after = 0;
    while (1)
    {
        proto = amp_new_proto();
        cb = _amp_new_callback(junk_callback, NULL);
        _amp_put_callback(proto->outstanding_requests, ask_key, cb);

        amp_put_int(proto->box, ANSWER, ask_key);

        enable_malloc_failures(fail_after++);

        /* run code under test */
        result = proto->dispatch_box(proto, proto->box);

        /* clean up and check result */
        disable_malloc_failures();
        amp_free_proto(proto);

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            /* Yes */
            fail_unless(result == ENOMEM);
        }
        else
            /* The successful code path is already tested elsewhere. */
            break;
    }

    /* ANSWER box - test another error-handling code path where the
     * _answer key does not decode to an integer - non-integer
     * _answer keys are not illegal per-se, but /we/ never send
     * an _ask key that is not an integer, so we can safely assume
     * that valid respones will always have intger _answer keys */

    proto = amp_new_proto();
    amp_put_cstring(proto->box, ANSWER, "garbage");

    /* run code under test */
    result = proto->dispatch_box(proto, proto->box);

    /* clean up and check result */
    amp_free_proto(proto);

    fail_unless(result == AMP_DECODE_ERROR);


    /* ERROR box */
    fail_after = 0;
    while (1)
    {
        proto = amp_new_proto();
        cb = _amp_new_callback(junk_callback, NULL);
        _amp_put_callback(proto->outstanding_requests, ask_key, cb);

        amp_put_int(proto->box, _ERROR, 1);

        enable_malloc_failures(fail_after++);

        /* run code under test */
        result = proto->dispatch_box(proto, proto->box);

        /* clean up and check result */
        disable_malloc_failures();
        amp_free_proto(proto);

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            /* Yes */
            fail_unless(result == ENOMEM);
        }
        else
            /* The successful code path is already tested elsewhere. */
            break;
    }
}
END_TEST

START_TEST(test_new_box_with_malloc_failures)
{
    /* test that amp_new_box() handles malloc() failures
     * gracefully */
    int fail_after = 0;
    AMP_Box_T *result;

    while (1)
    {
        enable_malloc_failures(fail_after++);

        /* run code under test */
        result = amp_new_box();

        /* clean up and check result */
        disable_malloc_failures();

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            /* Yes */
            fail_unless(result == NULL);
        }
        else
        {
            fail_unless(result != NULL);
            amp_free_box(result);
            break;
        }
    }

}
END_TEST

START_TEST(test_new_proto_with_malloc_failures)
{
    /* test that amp_new_proto() handles malloc() failures
     * gracefully */
    int fail_after = 0;
    AMP_Proto_T *result;

    while (1)
    {
        enable_malloc_failures(fail_after++);

        /* run code under test */
        result = amp_new_proto();

        /* clean up and check result */
        disable_malloc_failures();

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            /* Yes */
            fail_unless(result == NULL);
        }
        else
        {
            fail_unless(result != NULL);
            amp_free_proto(result);
            break;
        }
    }

}
END_TEST

START_TEST(test_parse_box_with_malloc_failures)
{
    AMP_Proto_T *proto;
    int fail_after = 0;
    int bytesConsumed;
    int result;

    while (1)
    {
        proto = amp_new_proto();

        enable_malloc_failures(fail_after++);

        /* run code under test */
        result = amp_parse_box(proto, proto->box, &bytesConsumed,
                               validBox, sizeof(validBox));

        /* clean up and check result */
        disable_malloc_failures();

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            /* Yes */
            fail_unless(result == 0);
            fail_unless(proto->error == ENOMEM);
            fail_unless(bytesConsumed >= 0);

            amp_free_proto(proto);
        }
        else
        {
            fail_unless(result == 1);
            fail_unless(proto->error == 0);
            fail_unless(bytesConsumed == sizeof(validBox));

            amp_free_proto(proto);
            break;
        }
    }
}
END_TEST

START_TEST(test_amp_call_with_malloc_failures)
{
    AMP_Box_T *args;
    unsigned int ask_key;
    int fail_after = 0;
    int result;

    while (1)
    {
        args = amp_new_box();
        enable_malloc_failures(fail_after++);

        /* Run code under test */
        result = amp_call(test_proto, "FooCommand", args,
                          NULL, NULL, &ask_key);

        /* clean up and check result */
        disable_malloc_failures();
        amp_free_box(args);

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            fail_unless(result == ENOMEM);

            /* And ensure that no state was left in the proto */
            fail_unless(Table_length(test_proto->outstanding_requests) == 0);
        }
        else
        {
            fail_unless(result == 0);
            break;
        }
    }
}
END_TEST

START_TEST(test_amp_cancel_with_malloc_failures)
{
    AMP_Box_T *args;
    AMP_Proto_T *proto;
    unsigned int askKey;
    int fail_after = 0;
    int result;

    while (1)
    {
        args = amp_new_box();
        proto = amp_new_proto();

        /* Register some callbacks */
        amp_call(proto, "someCommand", args,
                 ignore_result_cb, NULL, &askKey);
        amp_free_box(args);

        enable_malloc_failures(fail_after++);

        /* Run code under test */
        result = amp_cancel(proto, askKey);

        /* clean up and check result */
        disable_malloc_failures();
        amp_free_proto(proto);

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            fail_unless(result == ENOMEM);
        }
        else
        {
            fail_unless(result == 0);
            break;
        }
    }
}
END_TEST

START_TEST(test_amp_chunks_with_malloc_failures)
{
    int fail_after = 0;
    AMP_Chunk_T *result;
    unsigned char temp_buf[5];

    /* TEST amp_chunk_for_buffer() */
    while (1)
    {
        enable_malloc_failures(fail_after++);

        /* Run code under test */
        result = amp_chunk_for_buffer(temp_buf, sizeof(temp_buf));

        /* clean up and check result */
        disable_malloc_failures();
        amp_free_chunk(result);

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            fail_unless(result == NULL);
        }
        else
        {
            fail_unless(result != NULL);
            break;
        }
    }

    /* TEST amp_chunk_copy_buffer() */
    while (1)
    {
        enable_malloc_failures(fail_after++);

        /* Run code under test */
        result = amp_chunk_copy_buffer(temp_buf, sizeof(temp_buf));

        /* clean up and check result */
        disable_malloc_failures();
        amp_free_chunk(result);

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            fail_unless(result == NULL);
        }
        else
        {
            fail_unless(result != NULL);
            break;
        }
    }
}
END_TEST

START_TEST(test_amp_respond_with_malloc_failures)
{
    char *ask_key = "ask123";

    AMP_Request_T *req;
    AMP_Proto_T *proto = amp_new_proto();
    AMP_Box_T *resp_args = amp_new_box();

    int result;
    int fail_after = 0;

    amp_set_write_handler(proto, discarding_write_handler, NULL);

    NEW(req);
    memset(req, 0, sizeof(*req));

    /* The other fields of a `struct AMP_Request' are not accessed
     * when responding */
    req->ask_key = amp_chunk_copy_buffer(ask_key, strlen(ask_key));

    amp_put_cstring(resp_args, "field1", "VAL1");
    amp_put_cstring(resp_args, "field2", "VAL2");

    while (1)
    {
        enable_malloc_failures(fail_after++);

        /* Run code under test */
        result = amp_respond(proto, req, resp_args);

        /* clean up and check result */
        disable_malloc_failures();

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            fail_unless(result == ENOMEM);
        }
        else
        {
            fail_unless(result == 0);
            break;
        }
    }

    /* clean up */
    amp_free_proto(proto);
    amp_free_box(resp_args);
    amp_free_request(req);
}
END_TEST


START_TEST(test_amp_new_error_from_box_with_malloc_failures)
{
    int result;
    int fail_after = 0;
    AMP_Box_T *box;
    AMP_Error_T *error;

    box = amp_new_box();
    amp_put_cstring(box, _ERROR, "1");
    amp_put_cstring(box, ERROR_CODE, "SOME_ERROR_CODE");
    amp_put_cstring(box, ERROR_DESCR, "Some error description.");

    while (1)
    {
        enable_malloc_failures(fail_after++);

        /* Run code under test */
        result = _amp_new_error_from_box(box, &error);

        /* clean up and check result */
        disable_malloc_failures();

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            fail_unless(result == ENOMEM);
        }
        else
        {
            fail_unless(result == 0);
            /* The box has already been free'd if creating the AMP_Error
             * object was succesful. */
            amp_free_error(error);
            break;
        }
    }
}
END_TEST


START_TEST(test_amp_new_responder_with_malloc_failures)
{
    int fail_after = 0;
    _AMP_Responder_p result;

    while (1)
    {
        enable_malloc_failures(fail_after++);

        /* Run code under test */
        result = _amp_new_responder(NULL, NULL);

        /* clean up and check result */
        disable_malloc_failures();

        /* Were one or more malloc failures simulated? */
        if (allocation_failure_occurred)
        {
            fail_unless(result == NULL);
        }
        else
        {
            fail_unless(result != NULL);
            _amp_free_responder(result);
            break;
        }
    }
}
END_TEST


START_TEST(test__amp_call_success)
{
    int a;
    unsigned int askKey;
    int writtenAskKey;
    unsigned char *buf;
    int bufSize;

    struct saved_write *write;
    AMP_Box_T *box;
    int bytesConsumed;

    char cmdToCall[] = "SomeCommand";

    /* set up an AMP_Proto and send a command using it */
    AMP_Proto_T *proto = amp_new_proto();
    amp_set_write_handler(proto, save_writes, NULL);

    AMP_Box_T *args = amp_new_box();
    amp_put_int(args, "a", 1776);

    fail_unless( amp_call(proto, cmdToCall, args, NULL, NULL, &askKey) == 0);

    amp_free_box(args);

    /* the write handler should have been called with a buffer
     * representing the fully-serialized command request */

    fail_unless(List_length(saved_writes) == 1);

    saved_writes = List_pop(saved_writes, (void**)&write);

    fail_unless(write->proto == proto); /* write handler called with correct AMP_Proto pointer */

    /* parse the written bytes as an AMP_Box and make sure it looks right */
    box = amp_new_box();
    fail_unless( amp_parse_box(proto, box, &bytesConsumed,
                               write->chunk->value,
                               write->chunk->size) );

    /* parsing the box used all bytes that were written? */
    fail_unless(bytesConsumed == write->chunk->size);

    free(write->chunk->value);
    amp_free_chunk(write->chunk);
    free(write);

    /* right number of keys written?
     * 1 for "_ask" key
     * 1 for "_command" key
     * 1 for "a" key */
    fail_unless(amp_num_keys(box) == 3);

    /* CHECK "_ask" key */

    /* expected 0 return */
    fail_if(amp_get_int(box, ASK, &writtenAskKey));

    /* same ask key written as returned by amp_call() ? */
    fail_unless(writtenAskKey == askKey);

    /* CHECK "_command" key */

    /* expected 0 return */
    fail_if(amp_get_bytes(box, COMMAND, &buf, &bufSize));

    /* written command name is correct length? */
    fail_unless(bufSize == strlen(cmdToCall));

    /* expected 0 return (memory is equal) */
    fail_if(memcmp(buf, cmdToCall, strlen(cmdToCall)));


    /* CHECK "a" key */

    /* expected 0 return */
    fail_if(amp_get_int(box, "a", &a));

    /* same value we wrote */
    fail_unless(a == 1776);

    amp_free_box(box);
    amp_free_proto(proto);
}
END_TEST


START_TEST(test__amp_cancel__success)
{
    int ask_key;
    int result;
    AMP_Box_T *args = amp_new_box();
    AMP_Proto_T *proto = amp_new_proto();
    struct saved_result *r;

    amp_set_write_handler(proto, discarding_write_handler, NULL);

    /* set up some call state */
    amp_call(proto, "SomeCommand", args, save_result_cb, (void*)0x123, &ask_key);
    amp_free_box(args);

    fail_unless( Table_length(proto->outstanding_requests) == 1 );

    /* cancel it */
    result = amp_cancel(proto, ask_key);

    fail_unless( result == 0 );

    /* ensure callback was called with correct result */
    fail_unless( List_length(saved_results) == 1 );

    saved_results = List_pop(saved_results, (void**)&r);

    fail_unless( r->proto == proto );

    fail_unless( r->result->reason == AMP_CANCEL );
    fail_unless( r->result->response == NULL );
    fail_unless( r->result->error == NULL );

    fail_unless( r->callback_arg == (void*)0x123 );

    /* and state was forgotten */
    fail_unless( Table_length(proto->outstanding_requests) == 0 );

    amp_free_result(r->result);
    free(r);
    amp_free_proto(proto);
}
END_TEST


START_TEST(test__amp_cancel__no_such_key)
{
    int result;
    AMP_Proto_T *proto = amp_new_proto();

    /* try to cancel a call that was never made */
    result = amp_cancel(proto, 42);

    fail_unless( result == AMP_NO_SUCH_ASK_KEY );

    amp_free_proto(proto);
}
END_TEST


/* Test amp_call_no_answer() */
START_TEST(test__amp_call_no_answer)
{
    int a;
    unsigned char *buf;
    int bufSize;

    struct saved_write *write;
    AMP_Box_T *box;
    int bytesConsumed;

    char cmdToCall[] = "SomeCommand";

    /* set up an AMP_Proto and send a command using it */
    AMP_Proto_T *proto = amp_new_proto();
    amp_set_write_handler(proto, save_writes, (void*)0x1234);

    AMP_Box_T *args = amp_new_box();
    amp_put_int(args, "a", 1776);

    fail_unless(amp_call_no_answer(proto, cmdToCall, args) == 0);

    amp_free_box(args);

    /* the write handler should have been called with a buffer
     * representing the fully-serialized command request */

    fail_unless(List_length(saved_writes) == 1);

    saved_writes = List_pop(saved_writes, (void**)&write);

    /* write handler called with correct AMP_Proto pointer? */
    fail_unless(write->proto == proto);

    /* write handler called with correct arg? */
    fail_unless(write->write_arg == (void*)0x1234);

    /* parse the written bytes as an AMP_Box and make sure it looks right */
    box = amp_new_box();
    fail_unless( amp_parse_box(proto, box, &bytesConsumed,
                               write->chunk->value,
                               write->chunk->size) );

    /* parsing the box used all bytes that were written? */
    fail_unless(bytesConsumed == write->chunk->size);

    free(write->chunk->value);
    amp_free_chunk(write->chunk);
    free(write);

    /* right number of keys written?
     * 1 for "_command" key
     * 1 for "a" key */
    fail_unless(amp_num_keys(box) == 2);

    /* CHECK "_command" key */

    /* expected 0 return */
    fail_if(amp_get_bytes(box, COMMAND, &buf, &bufSize));

    /* written command name is correct length? */
    fail_unless(bufSize == strlen(cmdToCall));

    /* expected 0 return (memory is equal) */
    fail_if(memcmp(buf, cmdToCall, strlen(cmdToCall)));


    /* CHECK "a" key */

    /* expected 0 return */
    fail_if(amp_get_int(box, "a", &a));

    /* same value we wrote */
    fail_unless(a == 1776);

    amp_free_box(box);
    amp_free_proto(proto);
}
END_TEST


START_TEST(test__amp_call_no_ask_key)
{
    /* We can pass a NULL pointer for the &ask_key when
     * calling amp_call() if we aren't interested in what ask
     * key is used */

    /* set up an AMP_Proto and send a command using it */
    AMP_Proto_T *proto = amp_new_proto();
    amp_set_write_handler(proto, discarding_write_handler, NULL);

    AMP_Box_T *args = amp_new_box();
    amp_put_int(args, "a", 1776);

    /* no crash */
    fail_if(amp_call(proto, "Command", args, NULL, NULL, NULL));

    amp_free_box(args);
    amp_free_proto(proto);
}
END_TEST


START_TEST(test__amp_call__max_ask_key)
{
    /* Test that we can actually make a call using an _ask key == UINT_MAX */

    unsigned int askKey;
    long long writtenAskKey;

    struct saved_write *write;
    AMP_Box_T *box;
    int bytesConsumed;

    char cmdToCall[] = "SomeCommand";

    /* set up an AMP_Proto and send a command using it */
    AMP_Proto_T *proto = amp_new_proto();
    amp_set_write_handler(proto, save_writes, NULL);

    AMP_Box_T *args = amp_new_box();
    amp_put_int(args, "a", 1776);

    /* poke the private last_ask_key variable so that the next key used
     * is UINT_MAX */
    proto->last_ask_key = UINT_MAX - 1;

    fail_unless( amp_call(proto, cmdToCall, args, NULL, NULL, &askKey) == 0);

    amp_free_box(args);

    fail_unless(askKey == UINT_MAX);

    /* the write handler should have been called with a buffer
     * representing the fully-serialized command request */

    fail_unless(List_length(saved_writes) == 1);

    saved_writes = List_pop(saved_writes, (void**)&write);

    fail_unless(write->proto == proto); /* write handler called with correct AMP_Proto pointer */

    /* parse the written bytes as an AMP_Box and make sure it looks right */
    box = amp_new_box();
    fail_unless( amp_parse_box(proto, box, &bytesConsumed,
                               write->chunk->value,
                               write->chunk->size) );

    /* parsing the box used all bytes that were written? */
    fail_unless(bytesConsumed == write->chunk->size);

    free(write->chunk->value);
    amp_free_chunk(write->chunk);
    free(write);

    fail_if(amp_get_long_long(box, ASK, &writtenAskKey));

    /* same ask key written as returned by amp_call() ? */
    fail_unless(writtenAskKey == askKey);

    amp_free_box(box);
    amp_free_proto(proto);
}
END_TEST

START_TEST(test__answer_key_max)
{
    /* test that we can parse an _answer key == UINT_MAX */
    AMP_Box_T *box = amp_new_box();
    amp_put_long_long(box, ANSWER, UINT_MAX);

    AMP_Response_T *resp;
    fail_if(_amp_new_response_from_box(box, &resp));

    fail_unless(resp->answer_key == UINT_MAX);
    amp_free_response(resp);
}
END_TEST

START_TEST(test_amp_respond)
{
    struct saved_write *write;
    int bytesConsumed;

    char *ask_key = "ask123";
    unsigned char *buf;
    int buf_len;

    AMP_Request_T *req;
    AMP_Proto_T *proto = amp_new_proto();
    AMP_Box_T *resp_args = amp_new_box();

    amp_set_write_handler(proto, save_writes, NULL);

    NEW(req);
    memset(req, 0, sizeof(*req));

    /* The other fields of a `struct AMP_Request' are not accessed
     * when responding */
    req->ask_key = amp_chunk_copy_buffer(ask_key, strlen(ask_key));

    amp_put_cstring(resp_args, "field1", "VAL1");
    amp_put_cstring(resp_args, "field2", "VAL2");

    /* respond */
    amp_respond(proto, req, resp_args);

    /* a response box should have been written */
    fail_unless(List_length(saved_writes) == 1);

    saved_writes = List_pop(saved_writes, (void**)&write);

    /* write handler called with correct AMP_Proto pointer? */
    fail_unless(write->proto == proto);

    /* parse the written bytes as an AMP_Box and make sure it looks right */
    AMP_Box_T *box = amp_new_box();
    fail_unless( amp_parse_box(proto, box, &bytesConsumed,
                               write->chunk->value,
                               write->chunk->size) );

    /* parsing the box used all bytes that were written? */
    fail_unless(bytesConsumed == write->chunk->size);

    /* free saved write */
    free(write->chunk->value);
    amp_free_chunk(write->chunk);
    free(write);

    /* box has expected number of keys? */
    fail_unless(amp_num_keys(box) == 3);

    /* same answer key written as original ask_key? */
    fail_if( amp_get_bytes(box, ANSWER, &buf, &buf_len) );
    fail_unless(buf_len == strlen(ask_key));
    fail_if( memcmp(buf, ask_key, buf_len) );

    /* has field1 ? */
    fail_if( amp_get_bytes(box, "field1", &buf, &buf_len) );
    fail_unless(buf_len == 4);
    fail_if( memcmp(buf, "VAL1", 4) );

    /* has field2 ? */
    fail_if( amp_get_bytes(box, "field2", &buf, &buf_len) );
    fail_unless(buf_len == 4);
    fail_if( memcmp(buf, "VAL2", 4) );

    /* clean up */
    amp_free_request(req);
    amp_free_box(resp_args);
    amp_free_box(box);
    amp_free_proto(proto);
}
END_TEST

START_TEST(test__amp_strerror)
{
    const char *ret;

    ret = amp_strerror(1234567);
    fail_if( strcasestr(ret, "unknown") == NULL );

    ret = amp_strerror(ENOMEM);
    fail_if( strcasestr(ret, "out of memory") == NULL );

    ret = amp_strerror(AMP_BAD_KEY_SIZE);
    fail_if( strcasestr(ret, "invalid") == NULL );
    fail_if( strcasestr(ret, "key length") == NULL );
}
END_TEST

struct chunks_equal_case
{
    unsigned char *buf1;
    int bufSize1;
    unsigned char *buf2;
    int bufSize2;
    int expectedResult;
} chunks_equal_cases[] = {
    {"test",             4,  "test",          4,  1},
    {"foo",              3,  "bar",           3,  0},
    {" leadingSpace",    13, "leadingSpace",  12, 0},
    {"trailingSpace ",   14, "trailingSpace", 13, 0}
};
int num_chunks_equal_tests = (sizeof(chunks_equal_cases) /
                              sizeof(struct chunks_equal_case));

START_TEST(test__amp_chunks_equal)
{
    struct chunks_equal_case c = chunks_equal_cases[_i];
    AMP_Chunk_T *chunk1  = amp_chunk_for_buffer(c.buf1, c.bufSize1);
    AMP_Chunk_T *chunk2  = amp_chunk_for_buffer(c.buf2, c.bufSize2);
    fail_unless(amp_chunks_equal(chunk1, chunk2) == c.expectedResult);

    amp_free_chunk(chunk1);
    amp_free_chunk(chunk2);
}
END_TEST


START_TEST(test__amp_reset_proto)
{
    AMP_Box_T *gotBox;
    AMP_Proto_T *proto = amp_new_proto();
    proto->dispatch_box = save_box;

    /* Cycle through all of the parsing states of an
     * AMP_Proto by taking a full valid box, and parsing
     * the first byte, then the first two bytes, then the first
     * three bytes, and so on, up to box size - 1.
     *
     * Calling amp_reset_proto() after each parsing run, and verify
     * we're in a clean state */

    AMP_Box_T *box = amp_new_box();
    amp_put_cstring(box, "foo", "FOO");
    amp_put_cstring(box, "bar", "BAR");

    unsigned char *buf;
    int bufSize;

    amp_serialize_box(box, &buf, &bufSize);

    int i;
    for (i = 1; i < bufSize; i++)
    {
        amp_consume_bytes(proto, buf, i);

        /* now reset protocol */
        amp_reset_proto(proto);

        /* verify protocol is in a clean parsing state */
        fail_unless( amp_num_keys(proto->box) == 0 );
        fail_unless( proto->state == KEY_LEN_READ );
        fail_unless( proto->error == 0 );
        fail_unless( proto->key_len == -1 );
        fail_unless( proto->val_len == -1 );
        fail_unless( proto->key_data_fetched == 0 );
        fail_unless( proto->val_data_fetched == 0 );
    }

    /* now cause a parsing error to verify resetting also clears the error state */
    amp_consume_bytes(proto, "\001\001", 2); /* bad key length */

    /* now reset protocol */
    amp_reset_proto(proto);

    /* verify protocol is in a clean parsing state */
    fail_unless( amp_num_keys(proto->box) == 0 );
    fail_unless( proto->state == KEY_LEN_READ );
    fail_unless( proto->error == 0 );
    fail_unless( proto->key_len == -1 );
    fail_unless( proto->val_len == -1 );
    fail_unless( proto->key_data_fetched == 0 );
    fail_unless( proto->val_data_fetched == 0 );

    /* now after all that, actually parse the full box and notice that it's
     * dispatched correctly */
    amp_consume_bytes(proto, buf, bufSize);

    fail_unless( List_length(saved_boxes) == 1);
    saved_boxes = List_pop(saved_boxes, (void**)&gotBox);
    fail_unless( amp_boxes_equal(gotBox, box) );

    amp_free_box(box);
    amp_free_box(gotBox);
    free(buf);
    amp_free_proto(proto);
}
END_TEST


Suite *make_core_suite()
{

    Suite *s = suite_create ("Core");

    /* Core test cases */
    TCase *tc_consume = tcase_create("consume");
    tcase_add_checked_fixture(tc_consume, core_setup, core_teardown);
    tcase_add_loop_test(tc_consume, test__amp_consume_bytes, 0,
                        num_consume_tests);
    tcase_add_loop_test(tc_consume, test__amp_consume_bytes__multiple_boxes,
                        0, 3);
    tcase_add_test(tc_consume, test__amp_consume_bytes__dispatch_failure);
    tcase_add_test(tc_consume, test__amp_parse_box__invalid_state);
    suite_add_tcase(s, tc_consume);

    TCase *tc_dispatch = tcase_create("dispatch");
    tcase_add_checked_fixture(tc_dispatch, core_setup, core_teardown);
    tcase_add_test(tc_dispatch, dispatch_handled_responses);
    tcase_add_test(tc_dispatch, dispatch_handled_requests);
    tcase_add_test(tc_dispatch, dispatch_unhandled_boxes);
    tcase_add_test(tc_dispatch, unhandled_request_sends_error);
    tcase_add_test(tc_dispatch, test_process_bad_box);
    tcase_add_test(tc_dispatch, test_process_with_no_command_handler);
    tcase_add_test(tc_dispatch, _amp_new_error_from_box__error_cases);
    tcase_add_test(tc_dispatch, _amp_new_request_from_box__error_cases);
    tcase_add_test(tc_dispatch, _amp_remove_responder__not_found);
    suite_add_tcase(s, tc_dispatch);

    /* test code paths which handle malloc() failure and other
     * memory-management tasks */
    TCase *tc_memory = tcase_create("memory");
    tcase_add_checked_fixture(tc_memory, core_setup, core_teardown);
    tcase_add_test(tc_memory, test_free_proto);
    tcase_add_test(tc_memory, test_process_with_malloc_failure);
    tcase_add_test(tc_memory, test_new_box_with_malloc_failures);
    tcase_add_test(tc_memory, test_new_proto_with_malloc_failures);
    tcase_add_test(tc_memory, test_parse_box_with_malloc_failures);
    tcase_add_test(tc_memory, test_amp_call_with_malloc_failures);
    tcase_add_test(tc_memory, test_amp_cancel_with_malloc_failures);
    tcase_add_test(tc_memory, test_amp_chunks_with_malloc_failures);
    tcase_add_test(tc_memory, test_amp_respond_with_malloc_failures);
    tcase_add_test(tc_memory, test_amp_new_error_from_box_with_malloc_failures);
    tcase_add_test(tc_memory, test_amp_new_responder_with_malloc_failures);
    suite_add_tcase(s, tc_memory);

    /* amp_call() and amp_call_no_answer() test cases */
    TCase *tc_call = tcase_create("call");
    tcase_add_test(tc_call, test__amp_call_success);
    tcase_add_test(tc_call, test__amp_call_no_answer);
    tcase_add_test(tc_call, test__amp_call_no_ask_key);
    tcase_add_test(tc_call, test__amp_call__max_ask_key);
    suite_add_tcase(s, tc_call);

    /* amp_cancel() */
    TCase *tc_cancel = tcase_create("cancel");
    tcase_add_test(tc_cancel, test__amp_cancel__success);
    tcase_add_test(tc_cancel, test__amp_cancel__no_such_key);
    suite_add_tcase(s, tc_cancel);

    /* response-related test */
    TCase *tc_response = tcase_create("response");
    tcase_add_test(tc_response, test__answer_key_max);
    tcase_add_test(tc_response, test_amp_respond);
    suite_add_tcase(s, tc_response);

    /* amp_strerror() test cases */
    TCase *tc_strerror = tcase_create("amp_strerror");
    tcase_add_test(tc_strerror, test__amp_strerror);
    suite_add_tcase(s, tc_strerror);

    /* amp_chunks_equal() test cases */
    TCase *tc_chunks_equal = tcase_create("amp_chunks_equal");
    tcase_add_loop_test(tc_chunks_equal, test__amp_chunks_equal,
                        0, num_chunks_equal_tests);
    suite_add_tcase(s, tc_chunks_equal);

    /* direct tests for AMP_Proto objects */
    TCase *tc_proto = tcase_create("AMP_Proto");
    tcase_add_test(tc_proto, test__amp_reset_proto);
    suite_add_tcase(s, tc_proto);

    return s;
}

int main()
{
    int number_failed;

    the_real_malloc = malloc;

    SRunner *sr = srunner_create(NULL);
    srunner_add_suite(sr, make_core_suite());
    srunner_add_suite(sr, make_types_suite());
    srunner_add_suite(sr, make_box_suite());
    srunner_add_suite(sr, make_log_suite());
    srunner_add_suite(sr, make_buftoll_suite());
    srunner_add_suite(sr, make_mem_suite());
    srunner_add_suite(sr, make_list_suite());
    srunner_add_suite(sr, make_table_suite());
    /* srunner_add_suite(sr, make_ampc_suite()); */
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
