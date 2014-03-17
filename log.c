/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/* Logging-related code */


#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "amp.h"


#define MAX_LOG_LEN 256


amp_log_handler amp_log_handler_func = NULL;


void amp_set_log_handler(amp_log_handler handler)
{
    amp_log_handler_func = handler;
}


void amp_log(char *fmt, ...)
{
    if (amp_log_handler_func == NULL)
        return;

    char buf[MAX_LOG_LEN];
    va_list ap;

    va_start(ap, fmt);

    vsnprintf(buf, MAX_LOG_LEN, fmt, ap);
    amp_log_handler_func(buf);

    va_end(ap);
}


void amp_stderr_logger(char *message)
{
    /* Do NOT just pass message as the format string to fprintf
     * as it may contain untrusted data and would enable a
     * format-string exploit */
    fprintf(stderr, "%s\n", message);
}
