/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */


/* Example server for "Sum" and "Divide" commands
 * using libevent for non-blocking I/O */


#include <sys/types.h>
#include <sys/stat.h>

#ifndef WIN32
  #include <netinet/in.h>
#else
  #include <winsock2.h>
  #include <Ws2tcpip.h>
#endif

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* libamp */
#include <amp.h>

/* libevent */
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>

#include "common.h"
/* strtonum() from OpenBSD */
#include "strtonum.h"

void usage()
{
    fprintf(stderr, "asyncserver <port>\n\n"
                    "E.g. asyncserver 1234\n"
                    "Will bind to all interface on port 1234 and process "
                    "client connections asynchronously.\n");
    exit(1);
}

void sum_responder(AMP_Proto_T proto, AMP_Request_T req,
                   void *responder_arg)
{
    debug_print("Got command request: %s\n", req->command->value);
    /* Process Sum */

    /* XXX Geeeez... I think we should start setting/clearing `errno'
     * instead of requiring the return value of amp_* functions to be
     * capture for error handling. */

    AMP_Box_T answer = NULL;
    int ret;
    long long a, b, total;

    if ( (ret = amp_get_long_long(req->args, "a", &a)) != 0)
    {
        fprintf(stderr, "Couldn't get `a' key: %s\n", amp_strerror(ret));
        goto error;
    }

    if ( (ret = amp_get_long_long(req->args, "b", &b)) != 0)
    {
        fprintf(stderr, "Couldn't get `b' key: %s\n", amp_strerror(ret));
        goto error;
    }

    if ( (answer = amp_new_box()) == NULL)
    {
        fprintf(stderr, "Couldn't allocate box.\n");
        goto error;
    }

    total = a + b;
    debug_print("Did Sum: %lld + %lld = %lld\n", a, b, total);

    if ( (ret = amp_put_long_long(answer, "total", total)) != 0)
    {
        fprintf(stderr, "Couldn't set `total' key: %s\n", amp_strerror(ret));
        goto error;
    }

    if ( (ret = amp_respond(proto, req, answer)) != 0)
    {
        fprintf(stderr, "Couldn't send answer: %d: %s\n", ret, amp_strerror(ret));
        goto error;
    }

error:
    amp_free_request(req);
    amp_free_box(answer);
}


/* Called by libamp to send data to the AMP peer */
int do_write(AMP_Proto_T p, unsigned char *buf, int bufSize, void *write_arg)
{
    debug_print("%s\n", "do_write()");
    struct bufferevent *bev = (struct bufferevent *)write_arg;

    bufferevent_write(bev, buf, bufSize);

    free(buf);
    return 0;
}


void conn_readcb(struct bufferevent *bev, void *state)
{
    debug_print("%s\n", "conn_readcb()");

    /* Feed libamp some data */

    unsigned char buf[256];
    int bytesRead;
    int ret;

    AMP_Proto_T proto = state;

    bytesRead = bufferevent_read(bev, buf, 256);

    if ( (ret = amp_consume_bytes(proto, buf, bytesRead)) != 0)
    {
        fprintf(stderr, "ERROR in amp_consume_bytes(): %s\n", amp_strerror(ret));
        return;
    }
}


void conn_writecb(struct bufferevent *bev, void *state)
{
    /* Called when the output buffer for this bufferevent has been drained */
    debug_print("%s\n", "conn_writecb()");
}


void conn_eventcb(struct bufferevent *bev, short events, void *state)
{
    debug_print("%s\n", "conn_eventcb()");

    if (events & BEV_EVENT_EOF)
    {
        fprintf(stderr, "Connection closed.\n");
    }
    else if (events & BEV_EVENT_ERROR)
    {
        fprintf(stderr, "Got an error on the connection: %s\n",
                strerror(errno)); /*XXX win32*/
    }
    /* None of the other events can happen here, since we haven't enabled
     * timeouts - so go ahead and free the bufferevent */
    bufferevent_free(bev);
}


void listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
                 struct sockaddr *sa, int socklen, void *state)
{
    /* Called when a new socket connection has been accept()ed */
    debug_print("%s\n", "listener_cb()");

    struct event_base *base = state;
    struct bufferevent *bev;

    bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev)
    {
        fprintf(stderr, "Error constructing bufferevent!");
        event_base_loopbreak(base);
        return;
    }

    /* Set up AMP protocol */
    AMP_Proto_T proto;
    if ( (proto = amp_new_proto()) == NULL)
    {
        fprintf(stderr, "Couldn't allocate AMP_Proto.\n");
        exit(1);
    }

    amp_add_responder(proto, "Sum", sum_responder, bev);
    amp_set_write_handler(proto, do_write, bev);

    /* Start read/write monitoring of the new connection */
    bufferevent_setcb(bev, conn_readcb, conn_writecb, conn_eventcb, proto);
    bufferevent_enable(bev, EV_READ);
    bufferevent_enable(bev, EV_WRITE);
}


int main(int argc, char *argv[])
{
    int server_port;

    if (argc != 2)
        usage();

    server_port = strtonum(argv[1], (long long)1, (long long)65535, NULL);
    if (server_port == 0)
        usage(); /* bad port */

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(server_port);
    /* struct zero'd - so will listen on all IPv4 interfaces (0.0.0.0) */


    /* init libevent */
#ifdef WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup() failed.\n");
        exit(1);
    }
#endif

    struct event_base *ev_base = event_base_new();
    if (ev_base == NULL)
    {
        fprintf(stderr, "Unable to allocate `struct ev_base'.\n");
        exit(1);
    }

    struct evconnlistener *listener;

    /* Listen... */
    listener = evconnlistener_new_bind(ev_base, listener_cb, (void *)ev_base,
                                       LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,
                                       -1,
                                       (struct sockaddr*)&sin,
                                       sizeof(sin));
    if (listener == NULL)
    {
        fprintf(stderr, "Could not create a listener!\n");
        return 1;
    }

    fprintf(stderr, "Listening on port %d...\n", server_port);

    /* Set up libamp logging to stderr for any error messages that
     * it wishes to report */
    amp_set_log_handler(amp_stderr_logger);


    /* Mainloop */
    event_base_dispatch(ev_base);

    evconnlistener_free(listener);
    event_base_free(ev_base);
    return 0;
}

