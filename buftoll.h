/* Copyright (c) 2011 - Eric P. Mangold
 *
 * See LICENSE.txt for details.
 */

/*
 * Convert a buffer containing base-10 ASCII digits to a `long long'.
 *
 * String may be prefixed with an optional sign character, either '+' or '-'.
 * All other characters must be decimal digits.
 * No other leading or trailing characters are allowed.
 *
 * Sets `err' to 0 on success, or an AMP_* error constant on failure.
 * This must be checked by the calling code.
 *
 * Returns the parsed value on success, or 0 on failure.
 *
 */

#ifndef _BUFTOLL_H
#define _BUFTOLL_H

long long buftoll(const unsigned char *nptr, int size, int *err);
long long buftoll_range(const unsigned char *nptr, int size,
                        long long minval, long long maxval, int *err);

#endif
