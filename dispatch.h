/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

#include "table.h"
#include "amp.h"

struct _AMP_Callback
{
    int ask_key;
    amp_callback_func func;
    void *arg;
};

typedef struct _AMP_Callback *_AMP_Callback_p;


_AMP_Callback_p _amp_new_callback(amp_callback_func func, void *arg);

_AMP_Callback_Map_p _amp_new_callback_map(void);
int _amp_put_callback(_AMP_Callback_Map_p cb_map, int ask_key,
                           _AMP_Callback_p callback);
_AMP_Callback_p _amp_pop_callback(_AMP_Callback_Map_p cb_map, int ask_key);
void _amp_free_callback_map(_AMP_Callback_Map_p cb_map);
void _amp_free_callback(_AMP_Callback_p callback);


struct _AMP_Responder
{
    const char *command;
    amp_responder_func func;
    void *arg;
};

typedef struct _AMP_Responder *_AMP_Responder_p;

_AMP_Responder_p _amp_new_responder(amp_responder_func func, void *arg);

_AMP_Responder_Map_p _amp_new_responder_map(void);
void _amp_put_responder(_AMP_Responder_Map_p resp_map, const char *command,
                        _AMP_Responder_p responder);
_AMP_Responder_p _amp_get_responder(_AMP_Responder_Map_p resp_map,
                                    const unsigned char *command);
void _amp_remove_responder(_AMP_Responder_Map_p resp_map,
                           const char *command);
void _amp_free_responder_map(_AMP_Responder_Map_p resp_map);
void _amp_free_responder(_AMP_Responder_p responder);
