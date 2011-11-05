/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/* Benchmark client - Client for "Sum" command which performs as many synchronous calls
 * as possible
 * using blocking I/O */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef WIN32
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
#else
  #define NI_MAXHOST 100
  #include <Windows.h>
  #include <winsock2.h>
  #include <Ws2tcpip.h>
#endif

/* libamp */
#include <amp.h>

#include "common.h"
#include "net_utils.h"
#include "time_utils.h"
/* strtonum() from OpenBSD */
#include "strtonum.h"


void usage()
{
    fprintf(stderr, "benchclient <host:port> <number-of-calls>\n\n"
                    "Ex: benchclient localhost:1234 10000\n"
                    "Will use the Sum server on localhost port 1234 to perform "
                    "10,000 additions to test the AMP/s throughput of "
                    "the server.\n");
    exit(1);
}

typedef struct {
    int count;
    int max_count;
    double start_time;
} *Bench_State_T;

/* forward declaration */
void do_sum_call(AMP_Proto_p proto, Bench_State_T state);

void resp_cb(AMP_Proto_p proto, AMP_Result_p result, void *callback_arg)
{
    Bench_State_T state = callback_arg;
    int error;
    long long total;

    switch (result->reason)
    {
    case AMP_SUCCESS:

        if ( (error = amp_get_long_long((result->response)->args,
                                      "total", &total)) != 0) {
            fprintf(stderr, "Couldn't get total key: %s\n", amp_strerror(error));
            break;
        }

        /* Another addition done */
        state->count++;

        if (state->count == state->max_count)
        {
            double diff = time_double() - state->start_time;
            printf("Finished benchmark. Total: %d calls in %.03f seconds == %.1f AMP/s.\n",
                   state->count, diff, state->count / diff);
            exit(0);
        }
        else
        {
            do_sum_call(proto, state);
        }
        break;
    case AMP_ERROR:
        fprintf(stderr, "Got AMP error from remote peer.\n");

        if ((result->error)->error_code)
            fprintf(stderr, "Code: %s\n", (result->error)->error_code->value);

        if ((result->error)->error_descr)
            fprintf(stderr, "Description: %s\n",
                    (result->error)->error_descr->value);

        break;
    }
    amp_free_result(result);
}

void do_sum_call(AMP_Proto_p proto, Bench_State_T state)
{
    int ret;
    AMP_Box_p box = amp_new_box();
    amp_put_long_long(box, "a", 5);
    amp_put_long_long(box, "b", 7);

    ret = amp_call(proto, "Sum", box, resp_cb, state, NULL);
    if (ret)
    {
        fprintf(stderr, "amp_call() failed: %s\n", amp_strerror(ret));
        exit(1);
    }
}

int do_write(AMP_Proto_p proto, unsigned char *buf, int bufSize, void *write_arg)
{
    send(*(int *)write_arg, buf, bufSize, 0);
    return 0;
}


int main(int argc, char *argv[])
{
    if (argc != 3)
        usage();

    int client_sock;
    char buf[256];
    int ret;

    char server_hostname[NI_MAXHOST];
    int server_port;

    /* split IP from port number */
    if (parse_host_port(argv[1], server_hostname, NI_MAXHOST, &server_port) != 0)
        usage();

    Bench_State_T state;
    if ( (state = malloc(sizeof(*state))) == NULL)
    {
        fprintf(stderr, "Unable to allocate Bench_State_T.\n");
        exit(1);
    }

    state->count = 0;

    const char *errstr;
    state->max_count = strtonum(argv[2], 0, LLONG_MAX, &errstr);
    if (errstr != NULL)
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

    AMP_Proto_p proto;
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

    state->start_time = time_double();
    do_sum_call(proto, state);

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
