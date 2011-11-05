#include <string.h>

#include "strtonum.h"

/* parse a string in the format "hostname:port" and save the hostname string to
 * the buffer `host' of size `hostLen`. Save the port number to the int pointed
 * to by `port'.
 * Returns: 0 on success
 *          1 if the string could not be parsed
 *          2 if the parsed hostname (including terminating NUL) could not fit in `hostLen` bytes
 */
int parse_host_port(char *hostport, char *host, int hostLen, int *port)
{
    char *portStr;
    portStr = strchr(hostport, ':');
    if (portStr == NULL)
        return 1;

    /* will parsed hostname overflow the buffer? */
    if ( (portStr - hostport + 1) > hostLen )
        return 2;

    memcpy(host, hostport, portStr - hostport);
    host[ portStr - hostport ] = '\x00';

    portStr++; /* point to start of port specification */

    *port = strtonum(portStr, 1, 65535, NULL);
    if (*port == 0)
        return 1; /* bad port */

    return 0;
}
