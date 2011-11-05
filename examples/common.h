#ifndef _EXAMPLES_COMMON_H
#define _EXAMPLES_COMMON_H

int parse_host_port(char *hostport, char *host, int hostLen, int *port);

#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

#define debug_print(fmt, ...) \
        do { if (DEBUG_TEST) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                               __LINE__, __func__, __VA_ARGS__); } while (0)

#endif
