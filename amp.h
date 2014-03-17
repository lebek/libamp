/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/* Public libamp API */

/* Terminology:
 *
 * A "box" refers to an AMP message, that is, a collection of key/value pairs.
 *
 */

#ifndef _AMP_H
#define _AMP_H

/* Prevent symbols from being named-mangled by evil C++ compilers */
#ifdef __cplusplus
extern "C" {
#endif

/* Error codes used in various places */

#define AMP_BAD_KEY_SIZE    100
#define AMP_BAD_VAL_SIZE    101
#define AMP_BOX_EMPTY       102

#define AMP_REQ_KEY_MISSING 103

#define AMP_UNKNOWN_CMD     104

/* The given AMP_Proto is in a fatal error state */
#define AMP_PROTO_ERROR     105

/* Couldn't find the key you were looking for */
#define AMP_KEY_NOT_FOUND   106

/* Couldn't decode the value to the requested type */
#define AMP_DECODE_ERROR    107

/* Couldn't encode the value */
#define AMP_ENCODE_ERROR    108

/* The decoded value falls outside the range of representable values
 * for the type of data you requested */
#define AMP_OUT_OF_RANGE    109

/* Libamp encountered an internal error. Please file a bug report. */
#define AMP_INTERNAL_ERROR  110

/* amp_cancel() could not find the ask_key you requested */
#define AMP_NO_SUCH_ASK_KEY 111


/* One of the codes above, or ENOMEM
 * TODO - go through and use this type instead of int where appropriate */
typedef int amp_error_t;


/* Standard error codes for the _error key in AMP boxes */
static const char AMP_ERROR_UNHANDLED[] = "UNHANDLED";
static const char AMP_ERROR_UNKNOWN[]   = "UNKNOWN";


/* Begin AMP_DLL macro definition - This is how we export symbols from the shared library */
#if WIN32
  /* This works on MingW, MSVC and probably other Windows compilers */
  #ifdef BUILDING_LIBAMP
    #define AMP_DLL __declspec(dllexport)
  #else
    #if BUNDLE_LIBAMP
      /* If bundling the libamp source files in your program, as in the case of the `test_amp'
       * test-suite runner */
      #define AMP_DLL
    #else
      #define AMP_DLL __declspec(dllimport)
    #endif
  #endif
#elif BUILDING_LIBAMP
  /* This works on GCC and probably other compilers */
  #define AMP_DLL __attribute__((__visibility__("default")))
#else
  #define AMP_DLL
#endif
/* End AMP_DLL macro definition */


/* Return a string describing the passed in error code */
AMP_DLL const char *amp_strerror(amp_error_t amp_error_num);


typedef struct AMP_Box AMP_Box_T;


/* A NULL-terminated chunk of bytes with a known size.
 *
 * `size' indicates how many bytes are in the chunk
 * NOT including the extra NULL byte at the end.
 *
 * This struct may be used to hold the buffer data
 * itself, in which case `value' will point to an
 * address within the area allocated for this struct.
 *
 * On the other hand, this struct may be allocated
 * with no additional room, and `value' will point
 * to a seperately allocated buffer. In this case
 * the seperate buffer will not be free'd when
 * amp_free_chunk() is called.
 *
 * *** SECURITY NOTE ***
 *
 * This struct is used to hold values taken from AMP
 * boxes, which are allowed to contain NULL bytes
 * themselves, but there are a lot of cases where you
 * can be reasonably sure that this won't happen, and
 * it may be convenient to treat the data as a C-string.
 *
 * The worst a malicious AMP peer could do would be to
 * make the data appear shorter than what was actually
 * parsed off the wire - and I don't /think/ that could
 * be used as an exploit vector, per se.
 *
 * Be sure you decide UP-FRONT how you are going to
 * treat the data... either as a buffer, or as a C-string,
 * and be consistent with that decision.
 *
 * Security holes can arise, for example, if you received
 * some sensative data that must pass a sanity check before
 * it can be safely used. If you conduct the sanity check
 * while treating the data as a C-string, but then USE the
 * data as a buffer, then you have a big problem.
 *
 * An attacker may be able to pass the check by inserting a
 * NULL to hide the malicious data from the checking routine. */
struct AMP_Chunk
{
    unsigned char *value;
    int size;

    /* When we allocate these structures, we allocate
     * additional room to store the buffer data + NULL byte,
     * which will begin at the address of this variable. */
    unsigned char _buffer_starts_here;
};
typedef struct AMP_Chunk AMP_Chunk_T;


/* Represents an AMP call from a remote peer.
 * Passed to responders registered via the
 * amp_add_responder() API */
struct AMP_Request
{
    AMP_Chunk_T *command;
    AMP_Chunk_T *ask_key;
    AMP_Box_T *args;
};
typedef struct AMP_Request AMP_Request_T;


/* Free an AMP_Request_T * */
void AMP_DLL amp_free_request(AMP_Request_T *request);


/* Represents an AMP answer emitted by the remote peer
 * as a result of a previous AMP call. Wrapped in an
 * AMP_Result and passed to callbacks registered via
 * the amp_call() API */
struct AMP_Response
{
    unsigned int answer_key;
    AMP_Box_T *args;
};
typedef struct AMP_Response AMP_Response_T;


/* Represents an AMP error emitted by the remote peer
 * as a result of a previous AMP call. Wrapped in an
 * AMP_Result and passed to callbacks registered via
 * the amp_call() API */
struct AMP_Error
{
    int answer_key;
    AMP_Chunk_T *error_code;
    AMP_Chunk_T *error_descr;
};
typedef struct AMP_Error AMP_Error_T;


enum amp_result_reason
{
    AMP_SUCCESS,
    AMP_ERROR,
    AMP_CANCEL
};


/* Used to represent AMP DateTime values. */
typedef struct {
    int     year;        /* Year (1-9999) */
    int     month;       /* Month [1-12] */
    int     day;         /* Day of the month [1-31] */

    int     hour;        /* Hours since midnight [0-23] */
    int     min;         /* Minutes after the hour [0-59] */
    int     sec;         /* Seconds after the minute [0-59] */
    long    msec;        /* Microseconds [0-999999] */

    int     utc_offset;  /* Offset from UTC in minutes. */

} AMP_DateTime_T;


/* Represents the result of an AMP call.
 *
 * `reason' indicates the type of event that generated this Result:
 *
 * AMP_SUCCESS - received an AMP response for this call, access it
 *               through r->response.
 *
 * AMP_ERROR - received an AMP error for this call, access it
 *             through r->error.
 *
 * AMP_CANCEL - call was cancelled via amp_cancel() API. No other
 *              data is available.
 *
 * Only one outcome is possible, and only the associated
 * pointer for that outcome is assured to be valid; the
 * rest should be assumed to be NULL, and not dereferenced. */
struct AMP_Result
{
    enum amp_result_reason reason;
    AMP_Response_T *response;
    AMP_Error_T *error;
};
typedef struct AMP_Result AMP_Result_T;


/* Free a an AMP_Result_T * */
void AMP_DLL amp_free_result(AMP_Result_T *result);


typedef struct AMP_Proto AMP_Proto_T;


/* Prototype for function to handle a new AMP box read off the wire */
typedef int (*amp_dispatch_box_handler)(AMP_Proto_T *proto, AMP_Box_T *box);


/* Prototype for responder function to handle an AMP command from the
 * remote peer.
 *
 * `request' - holds the arguments for this function call in
 *             request->args. These should be extracted using
 *             the amp_get_* functions.
 *
 * `responder_arg' - user defined argument, set when registering this function
 *                   via amp_add_responder().
 *
 * Once an answer/error box has been constructed for this request, it should be
 * dispatched using amp_respond()/amp_respond_error().
 *
 * `request' should be freed using amp_free_request() before this function
 * exits. */
typedef void (*amp_responder_func)(AMP_Proto_T *proto, AMP_Request_T *request,
                                   void *responder_arg);


/* Prototype for callback function to handle the response to a
 * previous AMP call.
 *
 * `result' - this structure is described in detail above.
 *
 * `callback_arg' - user defined argument, set when registering this callback
 *                  via amp_call().
 *
 * This function should call amp_free_result() on `result' once
 * the answer has been extracted. */
typedef void (*amp_callback_func)(AMP_Proto_T *proto, AMP_Result_T *result,
                                  void *callback_arg);


/* Cancel an AMP request, ie invoke the callback now, discard the
 * response.
 *
 * This doesn't stop an AMP request from being written, nor does it
 * attempt to close any sort of connection (since libamp doesn't know
 * anything about the transport).
 *
 * Calling cancel on an AMP request for which the callback has already
 * been invoked will do nothing. */
int AMP_DLL amp_cancel(AMP_Proto_T *proto, int ask_key);


/* Prototype for function to handle writing data
 * to the remote AMP peer. We don't want libamp to
 * be tied to sockets, unnecessarily. So we may
 * provide helpers that implement this function
 * for sockets, but users will still be able to
 * run libamp over other transport mechanisms.
 *
 * This function is required to take ownership of
 * the passed in buffer, and must free() it when
 * finished.
 *
 * Must return 0 on success, or non-zero on error. */
typedef int(*write_amp_data_func)(AMP_Proto_T *proto, unsigned char *buf,
                                  int buf_size, void *write_arg);


/* Initialize a new AMP Protocol.
 *
 * You create one of these for each connection to a remote AMP peer.
 *
 * Returns an AMP_Proto_T * on success, or NULL on allocation failure. */
AMP_DLL AMP_Proto_T *amp_new_proto(void);


/* Reset the parsing state of an AMP Protocol.
 *
 * Any current box being accumulated will be forgotten, and the AMP_Proto
 * will be placed in a state to begin parsing a new box. Any error state
 * associated with the AMP_Proto will be cleared.
 */
void AMP_DLL amp_reset_proto(AMP_Proto_T *proto);


/* Free an AMP_Proto - call this after you've lost the connection to the
 * remote AMP peer. */
void AMP_DLL amp_free_proto(AMP_Proto_T *proto);


/* Cause the AMP_Proto object to process these raw protocol bytes
 *
 * You must call this function for each chunk of data you
 * read from the other side of the connection.
 *
 * This function does NOT take ownership of the passed
 * in buffer, and so you should handle free()ing it yourself.
 *
 * RETURN VALUE
 *
 * Upon success the function returns 0.
 *
 * If an error occurs one of the following constants is returned:
 *
 *      ENOMEM - Error allocating memory during protocol parsing
 *      AMP_BAD_KEY_SIZE - Parsed a 2-byte key size integer > 255
 *      AMP_BOX_EMPTY - Found a terminating key (double NUL bytes) but
 *                      there were no other key/values parsed previously.
 *
 *      AMP_REQ_KEY_MISSING - Parsed an AMP packet but found no
 *                          _command, _answer or _error key.
 *
 *  SIDE EFFECTS
 *
 *  Each time a fully-formed AMP packet ("box") is encountered in the
 *  byte stream, which is fed in to this function, the corresponding
 *  callback function is invoked immediately to handle it.
 *
 *  This could be in the form of a responder function registered
 *  with amp_add_responder(). Or in the form of a result-handler
 *  function registered with amp_call().
 */
int AMP_DLL amp_consume_bytes(AMP_Proto_T *proto, unsigned char* buf, int nbytes);


/* Set handler function for writing data to the remote AMP peer */
void AMP_DLL amp_set_write_handler(AMP_Proto_T *proto, write_amp_data_func func,
                                   void *write_arg);


/* Call a remote AMP Command
 *
 * The passed in AMP_Box should contain key/values for the arguments that the
 * remote Command expects to receive. It should not contain any of the special
 * AMP protocol keys, such as _command, or _ask.
 *
 * The function supplied in `callback' will be invoked when a response is
 * received for this call. `callback_arg' is an argument to be passed to
 * the callback.
 *
 * Returns 0 on success, otherwise an an AMP_* error code. */
int AMP_DLL amp_call(AMP_Proto_T *proto, const char *command, AMP_Box_T *args,
             amp_callback_func callback, void *callback_arg, unsigned int *ask_key);


/* Same as amp_call() except does not request an answer from the remote peer.
 *
 * You will never receive a callback as a result of this call. */
int AMP_DLL amp_call_no_answer(AMP_Proto_T *proto, const char *command, AMP_Box_T *args);


/* Register a responder function to handle an AMP command from the
 * remote peer.
 *
 * `command' - the AMP command name
 * `responder' - an `amp_responder_func'
 * `responder_arg' - an application defined argument to be passed
 *                   to the responder */
void AMP_DLL amp_add_responder(AMP_Proto_T *proto, const char *command, void *responder,
                               void *responder_arg);


/* Remove an AMP responder
 *
 * `command' - the AMP command name */
void AMP_DLL amp_remove_responder(AMP_Proto_T *proto, const char *command);


/* Respond to an AMP request. This function is usually called from within
 * an `amp_responder_func', after the result has been formed for an AMP
 * request. It may however be called at a later stage if application code
 * wishes to defer the response.
 *
 * `request' - The AMP request that is being responded to.
 *
 * `args' - the key/values representing an AMP answer for the command
 *          contained within `request'. It should not contain any of
 *          the special AMP protocol keys, such as _answer.
 *
 * Returns 0 on success, otherwise an AMP_* error code. */
int AMP_DLL amp_respond(AMP_Proto_T *proto, AMP_Request_T *request, AMP_Box_T *args);


/* Respond with an error to an AMP request. This function is usually called
 * from within an `amp_responder_func', but it may be called at a later
 * stage if application code wishes to defer the response.
 *
 * `request' - The AMP request that is being responded to.
 *
 * `code' - the value encoded on the wire with the "_error_code" key. By
 *          convention this is uppercase English UTF-8 text in the form
 *          "SOME_ERROR_CODE".
 *
 * `description' - the value encoded on the wire with the "_error_description"
 *                 key. By convention this is English UTF-8 text that
 *                 describes the error that occured in some detail.
 *
 * Returns 0 on success, otherwise an an AMP_* error code. */
int AMP_DLL amp_respond_error(AMP_Proto_T *proto, AMP_Request_T *request,
                              char *error, char *description);


/* Allocate a new AMP_Box (using default values for the internal hash table.)
 *
 * Returns an AMP_Box_T * on success, or NULL on allocation failure. */
AMP_DLL AMP_Box_T * amp_new_box(void);


/* free() all memory associated with the AMP_Box */
void AMP_DLL amp_free_box(AMP_Box_T *box);


/* 1 if it has the key, or 0 if not */
int AMP_DLL amp_has_key(AMP_Box_T *box, const char *key);


/* 1 if boxes are equal, or 0 if not */
int AMP_DLL amp_boxes_equal(AMP_Box_T *box, AMP_Box_T *box2);


/* Number of key/value pairs in the AMP_Box */
int AMP_DLL amp_num_keys(AMP_Box_T *box);


/* Delete a key/value pair from the AMP_Box */
int AMP_DLL amp_del_key(AMP_Box_T *box, const char *key);


/* Logging functions */

/* A function-pointer which accepts a UTF-8-encoded
 * string that is to be logged. */
typedef void (*amp_log_handler)(char *message);


/* Set a handler which will recive messages logged by libamp */
void AMP_DLL amp_set_log_handler(amp_log_handler handler);


/* Log a message. If no log handler is set nothing will happen. */
void AMP_DLL amp_log(char *fmt, ...);


/* A log handler which simply writes messages to stderr */
void AMP_DLL amp_stderr_logger(char *message);


/* AMP Types - functions for encoding and decoding the
 * standard AMP types
 *
 *  -= GETTING VALUES OUT OF AN AMP_Box =-
 *
 * Every amp_get_* function returns 0 on success, with the requested value
 * being stored in the location provided by the passed in pointer.
 *
 * On error, non-zero is returned with the return code indicating
 * why the function failed:
 *
 * AMP_KEY_NOT_FOUND - The key you asked for was not found in the AMP_Box
 *
 * AMP_DECODE_ERROR - The value for the key you requested could not be
 *                    decoded to the type of data that you asked for.
 *
 *                    E.g. if you called amp_get_int(...) but the raw
 *                    bytes found in the AMP_Box was "hello", then
 *                    AMP_DECODE_ERROR would be returned, because
 *                    "hello" is not a proper encoding for an integer.
 *                    ( http://amp-protocol.net/Types#Integer )
 *
 * AMP_OUT_OF_RANGE - The decoded value does not fit within the representable
 *                    range of the data type you requested.
 *
 *                    E.g. if you used amp_get_int(...) but the value found in
 *                    the AMP_Box would overflow or underflow the range of `int'
 *                    on your machine, then this code is returned.
 *
 *                    Only applies to integral types.
 *
 *
 *  - PUTTING VALUES IN TO AN AMP_Box -
 *
 * Every amp_put_* function returns 0 on success.
 *
 * A copy of the passed-in key and value is stored in the AMP_Box,
 * and will be free()ed when the AMP_Box has amp_free_box() called on it.
 *
 * Existing keys are overwritten with the new value.
 *
 * On error, non-zero is returned with the return code indicating
 * why the function failed:
 *
 * ENOMEM - Error allocating memory while attemping to add the key/value
 *          to the AMP_Box.
 *
 * TODO - what else can go wrong putting values in to an AMP_Box ? */


/* AMP Type: Bytes ("String" in Twisted implementation) */

/* store a pointer to the buffer in to `buf' and store the length
 * of the buffer in to `size'.
 *
 * The returned buffer has NOT been copied, and ownership is retained
 * by the AMP_Box object. Do not modify the buffer.
 *
 * Use memcpy() to make your own copy if you need to change the
 * buffer's contents.
 *
 * Do not free() the buffer as it will be automatically free()'ed
 * when the AMP_Box is free()ed using amp_free_box() */
int AMP_DLL amp_get_bytes(AMP_Box_T *box, const char *key, unsigned char **buf, int *size);


/* Store a buffer of bytes in to the AMP_Box.
 *
 * This makes a copy of the passed in key, and buffer, and takes
 * ownership of these copies. Thus they will be free()ed automatically
 * when the the AMP_Box is free()ed with amp_free_box() */
int AMP_DLL amp_put_bytes(AMP_Box_T *box, const char *key, const unsigned char *buf, int size);


/* Encode and store a buffer of bytes given as a NULL-terminated
 * C string. Does not store the extra NULL byte. */
int AMP_DLL amp_put_cstring(AMP_Box_T *box, const char *key, const char *value);


/* AMP Type: Integer (C type `long long') */

/* get a `long long' from a value in an AMP box and store it in
 * the variable pointed to by `value' */
int AMP_DLL amp_get_long_long(AMP_Box_T *box, const char *key, long long *value);


/* put a `long long' value in to an AMP box. */
int AMP_DLL amp_put_long_long(AMP_Box_T *box, const char *key, long long value);


/* AMP Type: Integer (C type `int') */

/* Retrieve and decode a `int' from an AMP_Box. */
int AMP_DLL amp_get_int(AMP_Box_T *box, const char *key, int *value);


/* Encode and store a `int' into an AMP_Box. */
int AMP_DLL amp_put_int(AMP_Box_T *box, const char *key, int value);


/* AMP Type: Unsigned Integer (C type `unsigned int') */

/* Retrieve and decode an `unsigned int' from an AMP_Box. */
int AMP_DLL amp_get_uint(AMP_Box_T *box, const char *key, unsigned int *value);


/* Encode and store an `unsigned int' into an AMP_Box. */
int AMP_DLL amp_put_uint(AMP_Box_T *box, const char *key, unsigned int value);


/* AMP Type: Float (C type `double') */

/* store a `double' value in to the double pointed to by `value' */
int AMP_DLL amp_get_double(AMP_Box_T *box, const char *key, double *value);


/* put a `double' value in to an AMP box. */
int AMP_DLL amp_put_double(AMP_Box_T *box, const char *key, double value);


/* AMP Type: Boolean (C type `int') */

/* get a `bool' (int) from a value in an AMP box and store it in
 * the variable pointed to by `value' */
int AMP_DLL amp_get_bool(AMP_Box_T *box, const char *key, int *value);


/* put a `bool' (int) into an AMP box */
int AMP_DLL amp_put_bool(AMP_Box_T *box, const char *key, int value);


/* AMP Type: DateTime (C type `AMP_DateTime_T *') */

/* Encode and store an `AMP_DateTime' in to an AMP_Box.
 * Returns 0 on success, or an AMP_* error code on failure. */
int amp_put_datetime(AMP_Box_T *box, const char *key, AMP_DateTime_T *value);


/* Retrieve and decode an `AMP_DateTime' from an AMP_Box.
 * Stores the decoded data in to the `AMP_DateTime' pointed to by `value'.
 * Returns 0 on success, or an AMP_* error code on failure. On error,
 * `value' may have been partially filled in before the error was detected. */
int amp_get_datetime(AMP_Box_T *box, const char *key, AMP_DateTime_T *value);


/* TODO - More function prototypes for other standard AMP data types */

#ifdef __cplusplus
}
#endif

#endif /* _AMP_H */
