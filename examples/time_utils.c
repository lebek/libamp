#include <stdlib.h>
#include <sys/time.h>

#include <stdio.h>


double time_double(void)
{
    double result;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    result = tv.tv_sec;
    result += (double)tv.tv_usec / 1000000;
    return result;
}
