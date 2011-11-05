#ifndef _AMP_INTERNAL_H
#define _AMP_INTERNAL_H

#include <stdio.h>

#include "mem.h"
#include "table.h"


#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

#define debug_print(fmt, ...) \
        do { if (DEBUG_TEST) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                               __LINE__, __func__, __VA_ARGS__); } while (0)


static const char COMMAND[]     = "_command";
static const char ASK[]         = "_ask";
static const char ANSWER[]      = "_answer";
static const char _ERROR[]       = "_error";
static const char ERROR_CODE[]  = "_error_code";
static const char ERROR_DESCR[] = "_error_description";

#define MAX_KEY_LENGTH 0xff
#define MAX_VALUE_LENGTH 0xffff


enum amp_protocol_state
{
    KEY_LEN_READ,
    KEY_DATA_READ,
    VAL_LEN_READ,
    VAL_DATA_READ
};


struct amp_key_value
{
    char *key; /* NUL-terminated key string */
    int keySize; /* cached length of stored key. */
    void *value;
    int valueSize;

    /* When we allocate these structures, we allocate
     * additional room to store the key and value data,
     * which will begin at the address of this variable. */
    char _bufferSpaceStartsHere;
};


typedef int key_cmp_func(const void *x, const void *y);


typedef unsigned int key_hash_func(const void *key);


/* Collection of key/value pairs representing an AMP
 * packet. May represent either a request or a response.
 *
 * This is an opaque structure - you many only interact with it
 * by using the provided access functions. */
struct AMP_Box
{
    int size;
    key_cmp_func *cmp;
    key_hash_func *hash;
    int length;
    unsigned int timestamp;
#ifdef AMP_TEST_SUPPORT
    /* if get_fail_code is non-zero, then any amp_get_* API
     * function, matching `get_fail_key', will return
     * get_fail_code immediately without any other effect */
    int get_fail_code;
    const char *get_fail_key;
#endif
    struct binding
    {
        struct binding *link;
        struct amp_key_value *keyval;
    } **buckets;
};


typedef Table_T _AMP_Callback_Map_p;


typedef Table_T _AMP_Responder_Map_p;


/* Represents our side of an AMP connection.
 *
 * Encapsulates the protocol parsing state, and holds pointers to callback
 * functions which are invoked to process AMP requests and responses.
 *
 * This is an opaque structure - you many only interact with it
 * by using the provided access functions.
 */
struct AMP_Proto
{
    enum amp_protocol_state state;

    int error;

    int key_len; /* Initialize to -1 */

    /* An extra byte in this buffer so that we
     * can always NULL-terminate the key and
     * use it as a dumb C string */
    char key_data[256+1];
    int key_data_fetched; /* Num bytes fetched so far */

    int val_len; /* Initialize to -1 */

    /* XXX TODO - do we really want each AMP_Proto
     * to consume 65k? This should be optimised
     * at some point */
    unsigned char val_data[256*256];
    int val_data_fetched; /* Num bytes fetched so far */

    unsigned int last_ask_key;

    /* Pointer to function which will write data to the
     * other side of this AMP connection (typically
     * a TCP socket) */
    write_amp_data_func write;

    /* Pointer to application-defined argument passed
     * to write handler */
    void *write_arg;

    /* Pointer to function which will handle all
     * AMP boxes read off the wire */
    amp_dispatch_box_handler dispatch_box;

    /* hash table used to keep track of responses we are
     * waiting for */
    _AMP_Callback_Map_p outstanding_requests;

    /* hash table used to find command-handler functions for
     * incoming requests */
    _AMP_Responder_Map_p responders;

    /* The "current" AMP box being parsed. */
    AMP_Box_p box;
};


int _amp_get_buf(AMP_Box_p box, const char *key,
                 unsigned char **buf, int *size);


int _amp_put_buf(AMP_Box_p box, const char *key,
                 const unsigned char *buf, int buf_size);


/* Allocate a new AMP_Chunk with room to hold `size' bytes of data
 * and a terminating NULL byte. */
AMP_Chunk_p amp_new_chunk(int size);


/* Free the AMP_Chunk. if chunk->value points to a buffer outside
 * of the space allocated for the AMP_Chunk, it will not be
 * free'd */
void amp_free_chunk(AMP_Chunk_p chunk);


/* Allocate a new AMP_Chunk which refers to an existing buffer of `size'
 * bytes. The existing buffer is not modified, so it may not be
 * NULL-terminated. The existing buffer is not free'd when the AMP_Chunk
 * is free'd. */
AMP_Chunk_p amp_chunk_for_buffer(unsigned char *buf, int size);


/* Allocate a new AMP_Chunk which copies the buffer passed in, and also
 * places a NULL byte at the end of the copied data. */
AMP_Chunk_p amp_chunk_copy_buffer(unsigned char *buf, int size);
int amp_chunks_equal(AMP_Chunk_p c1, AMP_Chunk_p c2);


/* Free an AMP_Response_p */
void amp_free_response(AMP_Response_p response);


/* Free an AMP_Error_p */
void amp_free_error(AMP_Error_p error);


/* Serialize an AMP box into a newly-allocated buffer */
int amp_serialize_box(AMP_Box_p box, unsigned char **buf, int *size);


/* Log handler singleton used by all of libamp.
 * Defined in log.c */
extern amp_log_handler amp_log_handler_func;

#endif /* _AMP_INTERNAL_H */
