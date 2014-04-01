/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/* Example client for "Sum" command using blocking I/O */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifndef WIN32
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
#else
  /* Wow. Windows is really crap. This defines the version of Windows (XP, in this case), we're
   * compiling on - functions like getaddrinfo()/freeaddrinfo() are unavailable without this */
  #define _WIN32_WINNT 0x0501

  #define NI_MAXHOST 100
  #include <Windows.h>
  #include <winsock2.h>
  #include <Ws2tcpip.h>
#endif

/* libamp */
#include <amp.h>
// incomplete type on AMP_Proto_T requires this...
//#include <amp_internal.h>


#include "common.h"
#include "net_utils.h"
/* strtonum() from OpenBSD */
#include "strtonum.h"


#define MAX_OPERANDS 50 /* the most numbers we will add up */


typedef struct {
    int lastAskKey;
    int idx;
    int max_idx;
    long long operands[MAX_OPERANDS];
} *Sum_State_T;


void usage()
{
    fprintf(stderr, "sumclient <host:port> <a> <b> [ <c> ... ]\n\n"
                    "Ex: sumclient localhost:1234 5 7 9 2\n"
                    "Will use the Sum server on localhost port 1234 to add "
                    "the numbers 5, 7, 9 and 2.\n");
    exit(1);
}


/* forward declaration */
void do_sum_call(AMP_Proto_T *proto, Sum_State_T sum_state);


void resp_cb(AMP_Proto_T *proto, AMP_Result_T *result, void *callback_arg)
{
    Sum_State_T sum_state = callback_arg;
    int ret;
    long long total;

    switch (result->reason)
    {
    case AMP_SUCCESS:

        if ( (ret = amp_get_long_long((result->response)->args,
                                      "total", &total)) != 0) {
            fprintf(stderr, "Couldn't get total key: %s\n", amp_strerror(ret));
            exit(1);
        }
        amp_free_result(result);

        if (sum_state->idx == sum_state->max_idx)
        {
            printf("Finished sums. Total: %lld\n", total);
            exit(0);
        }
        else
        {
            sum_state->operands[sum_state->idx] = total;
            do_sum_call(proto, sum_state);
        }
        break;
    case AMP_ERROR:
        fprintf(stderr, "Got AMP error from remote peer.\n");
        if ((result->error)->error_code)
            fprintf(stderr, "Code: %s\n", (result->error)->error_code->value);
            fprintf(stderr, "Description: %s\n",
                    (result->error)->error_descr->value);

        amp_free_result(result);
        exit(1);
        break;
    }
}

void do_sum_call(AMP_Proto_T *proto, Sum_State_T sum_state)
{
    int ret;
    AMP_Box_T *box = amp_new_box();
    amp_put_long_long(box, "a", sum_state->operands[sum_state->idx]);
    amp_put_long_long(box, "b", sum_state->operands[sum_state->idx+1]);

    sum_state->idx++;

    ret = amp_call(proto, "Sum", box, resp_cb, sum_state,
                   &(sum_state->lastAskKey));
    if (ret)
    {
        fprintf(stderr, "amp_call() failed: %s\n", amp_strerror(ret));
        exit(1);
    }
}

int do_write(AMP_Proto_T *proto, unsigned char *buf, int bufSize, void *write_arg)
{
    send(*(int *)write_arg, buf, bufSize, 0);
    return 0;
}


int main(int argc, char *argv[])
{
    if (argc < 4)
        usage();

    char buf[256];
    int ret;

    char server_hostname[NI_MAXHOST];
    int server_port;
    int client_sock;

    Sum_State_T sum_state;
    if ( (sum_state = malloc(sizeof(*sum_state))) == NULL)
    {
        fprintf(stderr, "ERROR allocating Sum_State_T: %s\n", strerror(errno));
        exit(1);
    }

    sum_state->idx = 0;

    /* first operand begins at argv[2] */
    sum_state->max_idx = argc - 3;

    /* do we have too many operands? */
    if (sum_state->max_idx >= MAX_OPERANDS)
        usage();

    /* Convert operand arguments to numbers */
    const char *errstr;
    int i;
    for (i = 0; i <= sum_state->max_idx; i++)
    {
        sum_state->operands[i] = strtonum(argv[i+2], LLONG_MIN, LLONG_MAX, &errstr);
        if (errstr != NULL)
            usage();
    }

    /* split IP from port number */
    if (parse_host_port(argv[1], server_hostname, NI_MAXHOST, &server_port) != 0)
        usage();

/* Windows is dumb */
#ifdef WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup() failed.\n");
        exit(1);
    }
#endif

    fprintf(stderr, "Connecting to server...");
    /* Connect to server */
    if ((client_sock = connect_tcp(server_hostname, server_port, &errstr)) == -1)
    {
        fprintf(stderr, "Failed to connect: %s\n", errstr);
        return 1;
    }
    fprintf(stderr, "Done.\n");

    /* Done connecting... set up AMP Protocol */

    AMP_Proto_T *proto;
    if ( (proto = amp_new_proto()) == NULL)
    {
        fprintf(stderr, "Couldn't allocate AMP_Proto.\n");
        return 1;
    };

    amp_set_write_handler(proto, do_write, &client_sock);

    /* Set up libamp logging to stderr for any error messages that
     * it wishes to report */
    amp_set_log_handler(amp_stderr_logger);

    /* Make first call... */

    do_sum_call(proto, sum_state);

    /* Start reading loop */

    int bytesRead;
    while ( (bytesRead = recv(client_sock, buf, sizeof(buf), 0)) >= 0)
    {
        if ( (ret = amp_consume_bytes(proto, buf, bytesRead)) != 0)
        {
            fprintf(stderr, "ERROR detected by amp_consume_bytes(): %s\n", amp_strerror(ret));
            return 1;
        };
    }

    return 0;
}
