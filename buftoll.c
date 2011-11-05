/* $OpenBSD: strtoll.c,v 1.6 2005/11/10 10:00:17 espie Exp $ */

/* Copyright (c) 2011 - Eric P. Mangold
 *
 * See LICENSE.txt for details.
 *
 * Heavily modifed from strtoll.c in OpenBSD. Original copyright follows.
 *
 *
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define _ISOC99_SOURCE

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "amp.h"

/* Some crappy GCC versions of limits.h don't define these.
 * Added by teratorn 2011/06/18 */
#ifndef LLONG_MIN
#define LLONG_MIN      (-0x7fffffffffffffffLL-1)
#endif

#ifndef LLONG_MAX
#define LLONG_MAX      0x7fffffffffffffffLL
#endif

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

long long
buftoll(const unsigned char *nptr, int size, int *err)
{
    const unsigned char *s;
    long long acc, cutoff;
    int c;
    int neg, any, cutlim;
    int base = 10;

    acc = 0; /* accumulator */
    any = 0; /* set to 1 if we've parsed at least one digit */
    neg = 0; /* parsing negative number? */
    s = nptr;

    if (size <= 0)
    {
        *err = AMP_DECODE_ERROR;
        return 0;
    }

    /*
     * Compute the cutoff value between legal numbers and illegal
     * numbers.  That is the largest legal value, divided by the
     * base.  An input number that is greater than this value, if
     * followed by a legal input character, is too big.  One that
     * is equal to this value may be valid or not; the limit
     * between valid and invalid numbers is then based on the last
     * digit.  For instance, if the range for long longs is
     * [-9223372036854775808..9223372036854775807] and the input base
     * is 10, cutoff will be set to 922337203685477580 and cutlim to
     * either 7 (neg==0) or 8 (neg==1), meaning that if we have
     * accumulated a value > 922337203685477580, or equal but the
     * next digit is > 7 (or 8), the number is too big, and we will
     * return a range error.
     *
     * Set `any' if any digits are consumed.
     */

    /* check for sign character */
    c = *s;
    if (c == '+')
    {
        neg = 0;
        s++;
        size--;
    }
    else if (c == '-')
    {
        neg = 1;
        s++;
        size--;
    }
    cutoff = neg ? LLONG_MIN : LLONG_MAX;
    cutlim = cutoff % base;
    cutoff /= base;
    if (neg)
    {
        /* The original implementation included this check - however, on C99, I
         * believe that this conditional can never be true: C99 defines the
         * result of integer modulo as keeping the sign of the dividend. Thus,
         * if neg == 1, then cutoff will be negative, and the result of
         * cutlim = cutoff % base will always be <= 0.  See section 6.5.5 of the
         * C99 standard. */
        /*
        if (cutlim > 0) {
            cutlim -= base;
            cutoff += 1;
        }
        */
        cutlim = -cutlim;
    }

    /* parse digits */
    while (size-- > 0)
    {
        c = *s++;

        if (isdigit(c))
        {
                c -= '0';
        }
        else
        {
            *err = AMP_DECODE_ERROR;
            return 0;
        }

        if (neg)
        {
            if (acc < cutoff || (acc == cutoff && c > cutlim))
            {
                *err = AMP_OUT_OF_RANGE;
                return 0;
            }
            else
            {
                any = 1;
                acc *= base;
                acc -= c;
            }
        }
        else
        {
            if (acc > cutoff || (acc == cutoff && c > cutlim))
            {
                *err = AMP_OUT_OF_RANGE;
                return 0;
            }
            else
            {
                any = 1;
                acc *= base;
                acc += c;
            }
        }
    }

    if (any)
    {
        /* Got a result */
        *err = 0;
        return acc;
    }
    else
    {
        /* Empty buffy or just a sign (+ or -) char processed.
         * No digits processed */
        *err = AMP_DECODE_ERROR;
        return 0;
    }
}


long long
buftoll_range(const unsigned char *nptr, int size, long long minval, long long maxval, int *err)
{
    long long value;

    value = buftoll(nptr, size, err);
    if (*err)
        return 0;

    if (value < minval || value > maxval)
    {
        *err = AMP_OUT_OF_RANGE;
        return 0;
    }
    return value;
}
