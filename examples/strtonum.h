/* Copyright (c) 2011 Eric P. Mangold
 *
 * See LICENSE.txt for details. */

#ifndef _STRTONUM_H
#define _STRTONUM_H

#include <limits.h>

/* some crappy GCC versions of limits.h don't define these */
#ifndef LLONG_MIN
#define LLONG_MIN      (-0x7fffffffffffffffLL-1)
#endif

#ifndef LLONG_MAX
#define LLONG_MAX      0x7fffffffffffffffLL
#endif

long long
strtonum(const char *numstr, long long minval, long long maxval,
    const char **errstrp);

#endif
