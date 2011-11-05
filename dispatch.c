/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/* Internal AMP request/response dispatch data structures */

#include <stdlib.h>
#include <string.h>

#include "table.h"

#include "amp.h"
#include "amp_internal.h"
#include "dispatch.h"

/* Callback map */

static int cmp_callback_key(const void *x, const void *y) {
    return *(unsigned int *)x != *(unsigned int *)y;
}

static unsigned hash_callback_key(const void *key) {
    /* Identity function should fill our buckets evenly since the key
     * is produced by a counter (i.e. keys are distributed evenly), and
     * counter maximum is significantly greater than no. of buckets */
    return *(unsigned int *)key;
}

_AMP_Callback_p _amp_new_callback(amp_callback_func func, void *arg)
{
    _AMP_Callback_p cb;
    if ( (cb = MALLOC(sizeof(struct _AMP_Callback))) == NULL)
        return NULL;

    cb->func = func;
    cb->arg = arg;

    return cb;
}

_AMP_Callback_Map_p _amp_new_callback_map(void)
{
    _AMP_Callback_Map_p cb_map = Table_new(0, cmp_callback_key,
                                           hash_callback_key);
    if (cb_map == NULL)
        return NULL;
    debug_print("New _AMP_Callback_Map at %p\n", cb_map);
    return cb_map;
}

int _amp_put_callback(_AMP_Callback_Map_p cb_map, int ask_key,
                      _AMP_Callback_p callback)
{
    callback->ask_key = ask_key;
    return Table_put(cb_map, &(callback->ask_key), callback);
}

_AMP_Callback_p _amp_pop_callback(_AMP_Callback_Map_p cb_map, int ask_key)
{
    return (_AMP_Callback_p)Table_remove(cb_map, &ask_key);
}

void _amp_free_callback_map(_AMP_Callback_Map_p cb_map)
{
    debug_print("Free _AMP_Callback_Map at %p\n", cb_map);
    Table_free(&cb_map);
}

void _amp_free_callback(_AMP_Callback_p callback)
{
    /* leave application code responsible for freeing cb->arg */
    free(callback);
}

/* Responder map */

static int cmp_responder_key(const void *key1, const void *key2)
{
    return strcmp((const char *)key1, (const char *)key2);
}

static unsigned hash_responder_key(const void *key) {
    /* According to 'The Practice of Programming', values
     * of 31 and 37 are proven to be good multipliers for
     * ASCII strings */
    const unsigned char *str = key;
    unsigned int hash = 0;
    int i;
    for (i = 0; str[i] != 0; i++) {
        hash = 37 * hash + str[i];
    }

    return hash;
}


_AMP_Responder_p _amp_new_responder(amp_responder_func func, void *arg)
{
    _AMP_Responder_p resp;
    if ( (resp = MALLOC(sizeof(struct _AMP_Responder))) == NULL)
        return NULL;

    resp->func = func;
    resp->arg = arg;

    return resp;
}

_AMP_Responder_Map_p _amp_new_responder_map(void)
{
    _AMP_Responder_Map_p resp_map = Table_new(0, cmp_responder_key,
                                              hash_responder_key);
    if (resp_map == NULL)
        return NULL;
    debug_print("New _AMP_Responder_Map at %p\n", resp_map);
    return resp_map;
}

void _amp_put_responder(_AMP_Responder_Map_p resp_map, const char *command,
                        _AMP_Responder_p responder)
{
    responder->command = command;
    Table_put(resp_map, responder->command, responder);
}

_AMP_Responder_p _amp_get_responder(_AMP_Responder_Map_p resp_map,
                                    const unsigned char *command)
{
    return (_AMP_Responder_p)Table_get(resp_map, (const void *)command);
}

void _amp_remove_responder(_AMP_Responder_Map_p resp_map,
                           const char *command)
{
    _AMP_Responder_p resp = (_AMP_Responder_p)Table_remove(resp_map, command);
    if (resp != NULL)
        _amp_free_responder(resp);
}

void _amp_free_responder_map(_AMP_Responder_Map_p resp_map)
{
    debug_print("Free _AMP_Responder_Map at %p\n", resp_map);
    Table_free(&resp_map);
}

void _amp_free_responder(_AMP_Responder_p responder)
{
    /* leave application code responsible for freeing responder->arg */
    free(responder);
}
