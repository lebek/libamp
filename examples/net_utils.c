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
  #include <Windows.h>
  #include <winsock2.h>
  #include <Ws2tcpip.h>
#endif


/* Resolve and connect to the given hostname, returning
 * a connected socket. On error, -1 is returned and
 * *errstr will point to a string describing the error.
 * *errstr may be set even if the call is successful. */
int connect_tcp(char *hostname, int port, const char **errstr)
{
    struct addrinfo hints, *res, *res0;
    int error;
    int s;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    error = getaddrinfo(hostname, NULL, &hints, &res0);
    if (error)
    {
        *errstr = gai_strerror(error);
        return -1;
    }

    s = -1;
    for (res = res0; res; res = res->ai_next) {
        s = socket(res->ai_family, res->ai_socktype,
                   res->ai_protocol);
        if (s == -1) {
            *errstr = strerror(errno);
            continue;
        }
        ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(port);

        if (connect(s, res->ai_addr, res->ai_addrlen) == -1) {
            *errstr = strerror(errno);
            close(s);
            s = -1;
            continue;
        }
        break;  /* okay we got one */
    }
    freeaddrinfo(res0);
    return s;
}
