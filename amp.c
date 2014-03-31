/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/* Asynchrous Messaging Protocol
 *
 * http://amp-protocol.net
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "amp.h"
#include "amp_internal.h"
#include "dispatch.h"


amp_error_t _amp_send_unhandled_command_error(AMP_Proto_T *proto, AMP_Request_T *req)
{
    /* Fire off an _error box */

    AMP_Box_T *box;
    unsigned char *buf    = NULL;
    unsigned char *idx;
    unsigned char *packet = NULL;
    int packetSize;
    amp_error_t ret = 0;

    static char prefix[] = "Unhandled Command: '";
    static char suffix[] = "'";


    if ((box = amp_new_box()) == NULL)
        return ENOMEM;

    /* put _error key */
    if ((ret = amp_put_bytes(box, _ERROR, req->ask_key->value,
                             req->ask_key->size)) != 0)
        goto error;

    /* put _error_code key */
    if ((ret = amp_put_cstring(box, ERROR_CODE, AMP_ERROR_UNHANDLED)) != 0)
        goto error;

    /* put _error_description key */
    int bufSize = sizeof(prefix) + req->command->size + sizeof(suffix) - 2;
    if ((buf = MALLOC(bufSize)) == NULL)
    {
        ret = ENOMEM;
        goto error;
    }
    idx = buf;

    /* use sizeof(s)-1 instead of strlen() as cheap optimization */
    memcpy(idx, prefix, sizeof(prefix)-1);
    idx += sizeof(prefix)-1;

    memcpy(idx, req->command->value, req->command->size);
    idx += req->command->size;

    memcpy(idx, suffix, sizeof(suffix)-1);

    /* TODO - it might be handy to have an amp_put_bytes_no_copy() API
     * to avoid the copying overhead in cases such as this where the
     * box can happily take ownership of the malloc'd buffer */
    if ((ret = amp_put_bytes(box, ERROR_DESCR, buf, bufSize)) != 0)
        goto error;

    /* buf has been copied in to the box now */

    if ((ret = amp_serialize_box(box, &packet, &packetSize)) != 0)
        goto error;

    /* write handler has taken possession of buffer - don't free */
    ret = proto->write(proto, packet, packetSize, proto->write_arg);

error:
    amp_free_box(box);
    free(buf);
    return ret;
}

int _amp_new_request_from_box(AMP_Box_T *box, AMP_Request_T **request)
{
    int ret, buf_size;
    unsigned char *buf;

    AMP_Request_T *r = NULL;
    AMP_Chunk_T *cmd_name = NULL;
    AMP_Chunk_T *ask_key = NULL; /* default if no _ask key given */

    if ( (r = MALLOC(sizeof(*r))) == NULL)
        return ENOMEM;

    if ( (ret = amp_get_bytes(box, COMMAND, &buf, &buf_size)) != 0)
        goto error;

    if ( (cmd_name = amp_chunk_copy_buffer(buf, buf_size)) == NULL)
    {
        ret = ENOMEM;
        goto error;
    }

    if (amp_has_key(box, ASK))
    {
        if ( (ret = amp_get_bytes(box, ASK, &buf, &buf_size)) != 0)
            goto error;

        if ( (ask_key = amp_chunk_copy_buffer(buf, buf_size)) == NULL)
        {
            ret = ENOMEM;
            goto error;
        }
    }

    r->command = cmd_name;
    r->ask_key = ask_key;
    r->args    = box;

    *request = r;
    return 0;

error:
    *request = NULL;
    free(r);
    amp_free_chunk(cmd_name);
    amp_free_chunk(ask_key);
    return ret;
}

int _amp_new_response_from_box(AMP_Box_T *box, AMP_Response_T **response)
{
    int ret;
    unsigned int key;
    AMP_Response_T *r;

    if ( (r = MALLOC(sizeof(*r))) == NULL)
    {
        ret = ENOMEM;
        goto error;
    }

    if ( (ret = amp_get_uint(box, ANSWER, &key)) != 0)
        goto error;

    r->answer_key = key;
    r->args = box;

    *response = r;

    return 0;

error:
    *response = NULL;
    free(r);
    return ret;
}

int _amp_new_result_with_response(AMP_Response_T *response, AMP_Result_T **result)
{
    AMP_Result_T *r;

    if ( (r = MALLOC(sizeof(*r))) == NULL)
        return ENOMEM;

    r->reason = AMP_SUCCESS;
    r->response = response;
    r->error = NULL;

    *result = r;

    return 0;
}


int _amp_new_error_from_box(AMP_Box_T *box, AMP_Error_T **error)
{
    int ret, key, buf_size;
    unsigned char *buf;

    AMP_Chunk_T *error_code  = NULL;
    AMP_Chunk_T *error_descr = NULL;
    AMP_Error_T *e;

    if ( (e = MALLOC(sizeof(*e))) == NULL)
        return ENOMEM;

    if ( (ret = amp_get_int(box, _ERROR, &key)) != 0)
        goto error; /* error decoding _answer key */

    if (amp_has_key(box, ERROR_CODE))
    {
        if ( (ret = amp_get_bytes(box, ERROR_CODE, &buf, &buf_size)) != 0)
            goto error;

        if ( (error_code = amp_chunk_copy_buffer(buf, buf_size)) == NULL)
        {
            ret = ENOMEM;
            goto error;
        }
    }

    if (amp_has_key(box, ERROR_DESCR))
    {
        if ( (ret = amp_get_bytes(box, ERROR_DESCR, &buf, &buf_size)) != 0)
            goto error;

        if ( (error_descr = amp_chunk_copy_buffer(buf, buf_size)) == NULL)
        {
            ret = ENOMEM;
            goto error;
        }
    }

    /* Free the box, we have what we need */
    amp_free_box(box);

    e->answer_key  = key;
    e->error_code  = error_code;
    e->error_descr = error_descr;

    *error = e;
    return 0;

error:
    free(e);
    amp_free_chunk(error_code);
    amp_free_chunk(error_descr);
    return ret;
}

int _amp_new_result_with_error(AMP_Error_T *error, AMP_Result_T **result)
{
    AMP_Result_T *r;

    if ( (r = MALLOC(sizeof(*r))) == NULL)
        return ENOMEM;

    r->reason = AMP_ERROR;
    r->response = NULL;
    r->error = error;

    *result = r;

    return 0;
}


int _amp_process_full_packet(AMP_Proto_T *proto, AMP_Box_T *box)
{
    /* Dispatch the box that has been accumulated by the given AMP_Proto .
     *
     * Returns 0 on success or an AMP_* code on failure.
     *
     * TODO - do we check for insane boxes that have e.g.
     *        an _answer and an _error key? */
    amp_error_t ret = 0;

    if (amp_num_keys(box) == 0)
        return AMP_BOX_EMPTY;

    if (amp_has_key(box, COMMAND))
    {
        debug_print("Received %s box.\n", COMMAND);

        AMP_Request_T *request;
        if ( (ret = _amp_new_request_from_box(box, &request)) != 0)
            return ret;

        proto->box = NULL; /* forget the box that is now held by the
                              request object */

        _AMP_Responder_p responder;
        if ( (responder = _amp_get_responder(proto->responders,
                                             request->command->value)) != NULL)
        {
            /* Fire off user-supplied responder */
            (responder->func)(proto, request, responder->arg);
        }
        else
        {
            amp_log("No handler for command: %s", request->command->value);

            /* send error to remote peer if _ask key present.
             * (ret == 0 before this point) */
            if (request->ask_key != NULL)
            {
                ret = _amp_send_unhandled_command_error(proto, request);
            }

            /* This free's the box too */
            amp_free_request(request);
            return ret;
        }
    }
    else if (amp_has_key(box, ANSWER))
    {
        debug_print("Received %s box.\n", ANSWER);

        AMP_Response_T *response;
        if ( (ret = _amp_new_response_from_box(box, &response)) != 0)
            return ret;

        proto->box = NULL; /* forget the box that is now held by the newly
                              created response object */

        _AMP_Callback_p cb;
        if ((cb = _amp_pop_callback(proto->outstanding_requests,
                                    response->answer_key)) != NULL) {
            AMP_Result_T *result;
            if ( (ret = _amp_new_result_with_response(response, &result)) != 0)
            {
                amp_free_response(response);
                _amp_free_callback(cb);
                return ret;
            }

            (cb->func)(proto, result, cb->arg);
            _amp_free_callback(cb);

        }
        else
        {
            amp_log("Got unknown _answer key: %u", response->answer_key);
            amp_free_response(response); /* free's the box too */
            return 0;
        }

    }
    else if (amp_has_key(box, _ERROR))
    {
        debug_print("Received %s box.\n", _ERROR);

        AMP_Error_T *error;
        if ( (ret = _amp_new_error_from_box(box, &error)) != 0)
            return ret;

        /* _amp_new_error_from_box was successful which means the box has
         * been free'd - so forget it to avoid a potential double-free() */
        proto->box = NULL;

        _AMP_Callback_p cb;
        if ((cb = _amp_pop_callback(proto->outstanding_requests,
                                    error->answer_key)) != NULL) {

            AMP_Result_T *result;
            if ( (ret = _amp_new_result_with_error(error, &result)) != 0)
            {
                amp_free_error(error);
                _amp_free_callback(cb);
                return ret;
            }

            (cb->func)(proto, result, cb->arg);
            _amp_free_callback(cb);

        }
        else
        {
            amp_log("Got unknown _error key: %u", error->answer_key);
            amp_free_error(error);
            return 0;
        }


    }
    else
    {
        amp_log("Invalid protocol data: %s", amp_strerror(AMP_REQ_KEY_MISSING));
        return AMP_REQ_KEY_MISSING;
    }

    return 0;
}

AMP_Proto_T *amp_new_proto(void)
{
    AMP_Proto_T *proto;
    AMP_Box_T *box = NULL;
    _AMP_Callback_Map_p outstanding_requests = NULL;
    _AMP_Responder_Map_p responders = NULL;

    if ( (proto = MALLOC(sizeof(struct AMP_Proto))) == NULL)
        return NULL;

    proto->state = KEY_LEN_READ;
    proto->error = 0;
    if((box = amp_new_box()) == NULL)
        goto error;

    proto->last_ask_key = 0;

    proto->key_len = -1;
    proto->key_data_fetched = 0;

    proto->val_len = -1;
    proto->val_data_fetched = 0;

    proto->dispatch_box = _amp_process_full_packet;

    if ((outstanding_requests = _amp_new_callback_map()) == NULL)
        goto error;

    if((responders = _amp_new_responder_map()) == NULL)
        goto error;

    proto->box = box;
    proto->outstanding_requests = outstanding_requests;
    proto->responders = responders;

    debug_print("New AMP_Proto at 0x%p\n", proto);
    return proto;

error:
    free(proto);

    if (box)
        amp_free_box(box);

    if (outstanding_requests)
        _amp_free_callback_map(outstanding_requests);

    /* NOTE - uncomment this if new structures are allocated below
     * the line above where `responders' is allocated
    if (responders)
        _amp_free_responder_map(responders);
     */

    return NULL;
}


void amp_reset_proto(AMP_Proto_T *proto)
{
    proto->state = KEY_LEN_READ;

    proto->key_len = -1;
    proto->key_data_fetched = 0;

    proto->val_len = -1;
    proto->val_data_fetched = 0;

    /* TODO - more efficient to delete the key/values instead of freeing
     * and allocating a new box ? */
    amp_free_box(proto->box);
    proto->box = amp_new_box();

    proto->error = 0;
}


void amp_free_proto(AMP_Proto_T *proto)
{
    /* XXX TODO Hmmm... what about freeing proto->box ?
     * I guess lets do it for now... might need to support
     * a method for the user to acquire ownership of
     * an AMP_Box from the AMP_Proto
     * */
    amp_free_box(proto->box);

    _amp_free_callback_map(proto->outstanding_requests);
    _amp_free_responder_map(proto->responders);
    free(proto);
    debug_print("Free AMP_Proto at 0x%p\n", proto);
}

void amp_set_write_handler(AMP_Proto_T *proto, write_amp_data_func func,
                           void *write_arg)
{
    proto->write = func;
    proto->write_arg = write_arg;
}


/* Try to parse a single AMP box out of the passed-in buffer
 * and fill the key/values in to `box'.
 *
 * Record the number of bytes consumed in the int pointed to
 * by `bytesConsumed'. `bytesConsumed' will be set regardless
 * of the success or failure of the parsing attempt.
 *
 * Returns 1 if a a full box was parsed, or 0 if not.
 *
 * Even if 0 is returned, the box may have been partially
 * filled with the key/values that have been accumulated
 * thus far.
 *
 * The parser will be left in such a state that subsequent calls
 * to amp_parse_box() will continue where it left off, and
 * continue to fill `box' with key/values until a full box
 * has been parsed.
 *
 * If an error occurs during parsing, proto->error will be set
 * and 0 will be returned.
 */
int amp_parse_box(AMP_Proto_T *proto, AMP_Box_T *box, int *bytesConsumed,
                  unsigned char* buf, int len)
{
    /* Guaranteed to have at least 1 byte in `buf' */

    int idx = 0;
    int bytes_remaining = 0;
    int bytes_needed = 0;

    while (idx < len) {
        bytes_remaining = len - idx;
        /* debug_print("state: %d\n", state); */
        /* debug_print("bytes_remaining: %d\n", bytes_remaining); */
        switch (proto->state) {
            case KEY_LEN_READ:
                if (proto->key_len == -1)
                {
                    /* consume first byte of key length */

                    /* idx points to the first byte of the key length */

                    if ( *(buf+idx) != '\0')
                    {
                        /* The 1st byte of the key length must always be \x00 */
                        proto->error = AMP_BAD_KEY_SIZE;
                        idx++;
                        *bytesConsumed = idx;
                        return 0; /* fail */
                    }

                    /* Just marks that we've consumed the first
                     * NUL byte of the key length */
                    proto->key_len = 0;
                    idx++;
                }
                else
                {
                    /* consume second byte of key length */
                    proto->key_len = *(buf+idx);
                    idx++;

                    if (proto->key_len == 0)
                    {
                        /* NUL-key signals the end of an AMP box */

                        /* Reset parser state so we can read more AMP packets.
                         *
                         * I /believe/ that the other variables, val_len,
                         * key_data_fetched and val_data_fetched should
                         * alreay have been reset as a result of the VAL_DATA_READ
                         * state that necessarily preceeded this state. */
                        proto->key_len = -1;

                        proto->state = KEY_LEN_READ; /* transition */

                        *bytesConsumed = idx;
                        return 1;

                    }
                    else
                    {
                        proto->state = KEY_DATA_READ; /* transition */
                    }
                }
                break;

            case KEY_DATA_READ:
                bytes_needed = proto->key_len - proto->key_data_fetched;
                if (bytes_remaining < bytes_needed)
                {
                    memcpy( &(proto->key_data[proto->key_data_fetched]),
                            buf+idx, bytes_remaining);
                    idx += bytes_remaining;
                    proto->key_data_fetched += bytes_remaining;
                    /* state stays the same */

                }
                else
                {
                    memcpy( &(proto->key_data[proto->key_data_fetched]),
                            buf+idx, bytes_needed);

                    /* NULL-terminate key so that it can be used as a
                     * dumb C string */
                    proto->key_data[proto->key_data_fetched+bytes_needed] = '\x00';
                    idx += bytes_needed;
                    proto->key_data_fetched += bytes_needed;
                    proto->state = VAL_LEN_READ; /* transition */
                }
                break;

            case VAL_LEN_READ:
                if (proto->val_len == -1)
                {
                    /* consume first byte of value length */

                    /* idx points to the first byte of the value length */

                    proto->val_len = *(buf+idx);
                    /* left shift it by one byte since this is the
                     * most-significant byte we just read */
                    proto->val_len = proto->val_len << 8;
                    idx++;
                }
                else
                {
                    /* consume second byte of value length */

                    /* combine it with previously left-shifted byte
                     * using bitwise OR */
                    proto->val_len |= *(buf+idx);
                    idx++;
                    proto->state = VAL_DATA_READ; /* transition */
                }
                break;

            case VAL_DATA_READ:
                bytes_needed = proto->val_len - proto->val_data_fetched;
                if (bytes_remaining < bytes_needed)
                {
                    memcpy( &(proto->val_data[proto->val_data_fetched]),
                            buf+idx, bytes_remaining);
                    idx += bytes_remaining;
                    proto->val_data_fetched += bytes_remaining;
                    /* state stays the same */
                }
                else
                {
                    memcpy( &(proto->val_data[proto->val_data_fetched]),
                            buf+idx, bytes_needed);
                    idx += bytes_needed;
                    proto->val_data_fetched += bytes_needed;

                    /* store the current key/value we've just finished
                     * reading in to the AMP_Box we're building */
                    proto->error = amp_put_bytes(box, proto->key_data,
                                                 proto->val_data,
                                                 proto->val_len);
                    if (proto->error) {
                        *bytesConsumed = idx;
                        return 0;
                    }

                    /* Reset parser state so we can read more key/values */
                    proto->key_len = -1;
                    proto->val_len = -1;

                    proto->key_data_fetched = 0;
                    proto->val_data_fetched = 0;

                    proto->state = KEY_LEN_READ; /* transition */
                }
                break;

            default:
                proto->error = AMP_INTERNAL_ERROR;
                return 0;
                break;
        }
    }
    *bytesConsumed = idx;
    return 0; /* proto->error not set, so this is a successful return -
                 it just means we never found the end of an AMP box */
}

int amp_consume_bytes(AMP_Proto_T *proto, unsigned char* buf, int len)
{
    /* Guaranteed to have at least 1 byte in `buf' */

    int idx = 0;
    int bytesConsumed = 0;
    int parseStatus;

    if (proto->error)
    {
        /* refuse to do any more work if the protocol
         * parser has previously encounted a fatal error. */
        return AMP_PROTO_ERROR;
    }

    while (idx < len) {

        parseStatus = amp_parse_box(proto, proto->box, &bytesConsumed,
                                    buf+idx, len-idx);
        idx += bytesConsumed;
        if (parseStatus)
        {
            /* got a full box */

            /* Dispatch it */
            proto->error = proto->dispatch_box(proto, proto->box);
            if (proto->error)
                return proto->error;

            /* Start parsing a new AMP box.
             * The handler function has taken ownership of the box we
             * just dispatched - so don't free it. */
            proto->box = amp_new_box();
        }
        else
        {
            /* might be error, or might have parsed partial box */
            if (proto->error)
                return proto->error;

            /* if no error it should mean that amp_parse_box()
             * consumed all of the bytes remaining in buf, and
             * we should fall out of the while-loop now */
        }
    }
    return 0;
}

AMP_Chunk_T *amp_new_chunk(int size)
{
    AMP_Chunk_T *c;
    if ( (c = MALLOC(sizeof(*c) + size)) == NULL)
        return NULL;

    /* since AMP_Chunk._buffer_starts_here is defined as char
     * we should have enough space for the chunk and an extra
     * NULL byte at the end */

    c->value = &c->_buffer_starts_here;
    c->size = size;
    *(c->value + size) = 0; /* NULL-terminate */
    return c;
}

AMP_Chunk_T *amp_chunk_for_buffer(unsigned char *buf, int size)
{
    AMP_Chunk_T *c;
    if ( (c = amp_new_chunk(0)) == NULL)
        return NULL;

    c->value = buf;
    c->size = size;
    return c;
}

AMP_Chunk_T *amp_chunk_copy_buffer(unsigned char *buf, int size)
{
    AMP_Chunk_T *c;
    if ( (c = amp_new_chunk(size)) == NULL)
        return NULL;

    memcpy(c->value, buf, size);
    return c;
}

void amp_free_chunk(AMP_Chunk_T *chunk)
{
    free(chunk);
}

int amp_chunks_equal(AMP_Chunk_T *c1, AMP_Chunk_T *c2)
{
    /* Return 1 if equal, otherwise 0 */
    if (c1->size == c2->size && memcmp(c1->value, c2->value, c1->size) == 0)
        return 1;
    return 0;
}

/* XXX TODO TESTS */
void amp_free_request(AMP_Request_T *request)
{
    amp_free_chunk(request->command);

    /* May be NULL if no _ask key was given in the _command
     * box as allowed by the protocol */
    if (request->ask_key != NULL)
        amp_free_chunk(request->ask_key);

    /* May be manually free()ed and set to NULL
     * by user code */
    if (request->args != NULL)
        amp_free_box(request->args);

    free(request);
}

int _amp_new_result_with_cancel(AMP_Result_T **result)
{
    AMP_Result_T *r;

    if ( (r = MALLOC(sizeof(*r))) == NULL)
        return ENOMEM;

    r->reason = AMP_CANCEL;
    r->response = NULL;
    r->error = NULL;

    *result = r;

    return 0;
}

void amp_free_response(AMP_Response_T *response)
{
    amp_free_box(response->args);
    free(response);
}

/* XXX TODO TESTS */
void amp_free_error(AMP_Error_T *error)
{
    if (error->error_code != NULL)
        amp_free_chunk(error->error_code);

    if (error->error_descr != NULL)
        amp_free_chunk(error->error_descr);
    free(error);
}

void amp_free_result(AMP_Result_T *result)
{
    if (result->response != NULL)
        amp_free_response(result->response);

    if (result->error != NULL)
        amp_free_error(result->error);

    free(result);
}

int amp_next_ask_key(AMP_Proto_T *proto)
{
    /* In the case that proto->last_ask_key == UINT_MAX it will wrap around
     * on incrementation. Assume that UINT_MAX is large enough to prevent key
     * collisions with outstanding requests. */
    proto->last_ask_key++;
    return proto->last_ask_key;
}

int _amp_do_write(AMP_Proto_T *proto, unsigned char *buf, int buf_size)
{
    if (proto->write == NULL)
    {
        amp_log("AMP_Proto.write == NULL, missing call to amp_set_write_handler()?");
        free(buf);
        return 1;
    }        
    return proto->write(proto, buf, buf_size, proto->write_arg);
}

static int _amp_call(AMP_Proto_T *proto, const char *command, AMP_Box_T *args,
                     amp_callback_func callback, void *callback_arg, unsigned int *ask_key_ret,
                     int requiresAnswer)
{
    /* TODO mutates the passed in box by adding special _command and
     * _ask (optional) keys. But user code might re-use the AMP_Box
     * they passed us here to make future calls, possibly with different
     * key/values.... so will the presence of the special keys
     * in their box cause problems ever? */

    int ret, ask_key = -1;
    unsigned char *buf;
    int buf_size;
    _AMP_Callback_p cb = NULL;

    if ( (ret = amp_put_cstring(args, COMMAND, command)) != 0)
        goto error;

    if (requiresAnswer)
    {
        if ((cb = _amp_new_callback(callback, callback_arg)) == NULL)
        {
            ret = ENOMEM;
            goto error;
        }

        ask_key = amp_next_ask_key(proto);

        if (ask_key_ret != NULL)
            *ask_key_ret = ask_key;

        if ((ret = _amp_put_callback(proto->outstanding_requests,
                                     ask_key, cb)) != 0)
            goto error;

        if ( (ret = amp_put_uint(args, ASK, proto->last_ask_key)) != 0)
            goto error;
    }
    else
    {
        /* ensure it doesn't have an old _ask key */
        amp_del_key(args, ASK);
    }

    if ( (ret = amp_serialize_box(args, &buf, &buf_size)) != 0)
        goto error;

    /* The write handler should return 0 on success, or non-zero on error
     * so just pass on the value */
    return _amp_do_write(proto, buf, buf_size);

error:
    if (cb)
    {
        /* Try to remove this callback from outstanding_requests - does
         * nothing if it doesn't exist in the hash table */
        _amp_pop_callback(proto->outstanding_requests, ask_key);
        _amp_free_callback(cb);
    }
    return ret;
}

int amp_call(AMP_Proto_T *proto, const char *command, AMP_Box_T *args,
             amp_callback_func callback, void *callback_arg, unsigned int *ask_key)
{
    return _amp_call(proto, command, args, callback, callback_arg, ask_key, 1);
}

int amp_call_no_answer(AMP_Proto_T *proto, const char *command, AMP_Box_T *args)
{
    /* TODO mutates the passed in box by adding special _answer key.
     * But user code might re-use the AMP_Box they passed us here,
     * possibly with different key/values.... so will the presence of the
     * special keys in their box cause problems ever? */
    return _amp_call(proto, command, args, NULL, NULL, NULL, 0);
}

int amp_cancel(AMP_Proto_T *proto, int ask_key)
{
    int ret;
    _AMP_Callback_p cb;
    if ((cb = _amp_pop_callback(proto->outstanding_requests,
                                ask_key)) != NULL) {
        AMP_Result_T *result;
        if ( (ret = _amp_new_result_with_cancel(&result)) != 0)
        {
            /* This would be quit awkward if _amp_new_result_with_cancel
             * fails here due to a malloc() failure.... we would have
             * forgotten about the callback, so any incoming _answer or
             * _error boxes for this ask_key will be ignored. BUT the
             * user-defined callback function will never have been called
             * with a result indicating cancellation as was requested */
            _amp_free_callback(cb);
            return ret;
        }

        (cb->func)(proto, result, cb->arg);
        _amp_free_callback(cb);

        return 0;
    }
    return AMP_NO_SUCH_ASK_KEY;
}

void amp_add_responder(AMP_Proto_T *proto, const char *command, void *responder,
                       void *responder_arg)
{
    _AMP_Responder_p resp = _amp_new_responder(responder, responder_arg);
    _amp_put_responder(proto->responders, command, resp);
}

void amp_remove_responder(AMP_Proto_T *proto, const char *command)
{
    _amp_remove_responder(proto->responders, command);
}

int amp_respond(AMP_Proto_T *proto, AMP_Request_T*request, AMP_Box_T *args)
{
    int ret;
    unsigned char *buf;
    int buf_size;

    if ( (ret = amp_put_bytes(args, ANSWER, request->ask_key->value,
                              request->ask_key->size)) != 0)
        return ret;

    if ( (ret = amp_serialize_box(args, &buf, &buf_size)) != 0)
        return ret;

    /* proto->write() should return 0 on success, or non-zero on error
     * so just pass on the value */
    return _amp_do_write(proto, buf, buf_size);
}

/* Error codes as defined in amp.h */
struct {
    int error_code;
    const char *error_string;
} amp_code_to_string[] = {
    {AMP_BAD_KEY_SIZE,    "Invalid AMP key length"},
    {AMP_BAD_VAL_SIZE,    "Invalid AMP value length"},
    {AMP_BOX_EMPTY,       "AMP box contains no key/value pairs"},
    {AMP_REQ_KEY_MISSING, "AMP box did not contain a special key required by the wire-protocol"},
    {AMP_PROTO_ERROR,     "AMP_Proto object is in fatal error state due to previously encountered error"},
    {AMP_KEY_NOT_FOUND,   "The requested key was not found in the AMP box"},
    {AMP_DECODE_ERROR,    "The value failed to decode to the requested type"},
    {AMP_ENCODE_ERROR,    "The value failed to encode as the specified type"},
    {AMP_OUT_OF_RANGE,    "The decoded value falls outside the representable range of the requested type"},
    {AMP_INTERNAL_ERROR,  "Libamp encountered an internal error. Please file a bug report."},
    {AMP_NO_SUCH_ASK_KEY, "amp_cancel() could not find the ask_key you requested."},
    {ENOMEM,              "malloc() failed. Out Of Memory."}
};

const char *amp_strerror(int amp_error_num)
{
    int i;
    int num_items = sizeof(amp_code_to_string) / sizeof(amp_code_to_string[0]);

    for (i = 0; i < num_items; i++)
        if (amp_code_to_string[i].error_code == amp_error_num)
            return amp_code_to_string[i].error_string;

    return "Unknown libamp error code";
}

