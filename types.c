/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/*
 * Encoders/Decoders for the various AMP Types that we support.
 *
 */

/* For INFINITY macro, etc */
#define _ISOC99_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>


#include "amp.h"
#include "amp_internal.h"
#include "buftoll.h"


/* AMP Type: Bytes (known as String in Twisted) */

/* Store an array of bytes into an AMP_Box. */
int amp_put_bytes(AMP_Box_p box, const char *key,
                  const unsigned char *buf, int buf_size)
{
    return _amp_put_buf(box, key, buf, buf_size);
}

/* Retrieve an array of bytes from an AMP_Box. */
int amp_get_bytes(AMP_Box_p box, const char *key,
                  unsigned char **buf, int *size)
{
    return _amp_get_buf(box, key, buf, size);
}

/* Encode and store an array of bytes given as a NULL-terminated
 * C string. Does not store the extra NULL byte.
 * Returns 0 on success, or an AMP_* error code on failure. */
int amp_put_cstring(AMP_Box_p box, const char *key, const char *value)
{
    return _amp_put_buf(box, key, (unsigned char*)value, strlen(value));
}

/* AMP Type: Boolean */

/* Encode and store a boolean value into an AMP_Box.
 * Returns 0 on success, or an AMP_* error code on failure. */
int amp_put_bool(AMP_Box_p box, const char *key, int value)
{
    const char *buf;
    if (value)
        buf = "True";
    else
        buf = "False";

    return _amp_put_buf(box, key, (unsigned char *)buf, strlen(buf));
}

/* Retrieve and decode a boolean value from an AMP_Box.
 * If the boolean is true `value' is set to 1, otherwise it is set to 0.
 * Returns 0 on success, or an AMP_* error code on failure. */
int amp_get_bool(AMP_Box_p box, const char *key, int *value)
{
    int err;
    unsigned char *buf;
    int buf_size;

    if ( (err = _amp_get_buf(box, key, &buf, &buf_size)) != 0)
        return err;

    if (buf_size == 4 && memcmp(buf, "True", buf_size) == 0)
        *value = 1;
    else if (buf_size == 5 && memcmp(buf, "False", buf_size) == 0)
        *value = 0;
    else
        return AMP_DECODE_ERROR;

    return 0;
}

/* Encode and store a `long long' into an AMP_Box.
 * Returns 0 on success, or an AMP_* error code on failure. */
int amp_put_long_long(AMP_Box_p box, const char *key,
                      long long value)
{
    /* A ludicrous way to determine the maximum number of base-10 digits
     * required to represent any `long long' on this system.
     *
     * add 3 because:
     *         1) round up because casting to `int' truncates the `double'
     *         2) need extra byte to hold possible "-" sign char
     *         3) extra byte to hold NUL-terminator
     *
     * Any sane person would have just done char buf[100];
     */
    int bufLen = ((int)(log10(pow(2, sizeof(long long)*CHAR_BIT)))) + 3;
    char buf[bufLen];

    snprintf(buf, bufLen, "%lld", value);
    return _amp_put_buf(box, key, (unsigned char *)buf, strlen(buf));
}


/* Retrieve and decode a `long long' from an AMP_Box.
 * Stores the decoded integer in to the `long long' pointed to by `value'.
 * Returns 0 on success, or an AMP_* error code on failure. */
int amp_get_long_long(AMP_Box_p box, const char *key,
                      long long *value)
{
    int err;
    unsigned char *buf;
    int buf_size;

    if ( (err = _amp_get_buf(box, key, &buf, &buf_size)) != 0)
        return err;

    *value = buftoll(buf, buf_size, &err);

    /* `err' will have been set to 0 if conversion was successful.
     * Otherwise it will be an AMP_* error code. */
    return err;
}

/* AMP Type: Integer (C type `int') */

/* Encode and store a `int' into an AMP_Box.
 * Returns 0 on success, or an AMP_* error code on failure. */
int amp_put_int(AMP_Box_p box, const char *key, int value)
{
    /* A ludicrous way to determine the maximum number of base-10 digits
     * required to represent any `int' on this system. */
    /* add 3 because:
     *         1) round up because casting to `int' truncates the `double'
     *         2) need extra byte to hold possible "-" sign char
     *         3) extra byte to hold NUL-terminator
     *
     * Any sane person would have just done char buf[100];
     */
    int bufLen = ((int)(log10(pow(2, sizeof(int)*CHAR_BIT)))) + 3;
    char buf[bufLen];

    snprintf(buf, bufLen, "%d", value);
    return _amp_put_buf(box, key, (unsigned char *)buf, strlen(buf));
}


/* Retrieve and decode a `int' from an AMP_Box.
 * Stores the decoded integer in to the `int' pointed to by `value'.
 * Returns 0 on success, or an AMP_* error code on failure. */
int amp_get_int(AMP_Box_p box, const char *key, int *value)
{
    int err;
    unsigned char *buf;
    int buf_size;
    long long tmp = 0;

    if ( (err = _amp_get_buf(box, key, &buf, &buf_size)) != 0)
        return err;

    tmp = buftoll(buf, buf_size, &err);
    if (err == 0)
    {
        if (tmp >= INT_MIN && tmp <= INT_MAX)
        {
            *value = (int)tmp;
            return 0;
        }
        else
        {
            return AMP_OUT_OF_RANGE;
        }
    }
    else
    {
        /* `err' will have been set to 0 if conversion was successful.
         * Otherwise it will be an AMP_* error code. */
        return err;
    }
}

/* Encode and store an `unsigned int' into an AMP_Box.
 * Returns 0 on success, or an AMP_* error code on failure. */
int amp_put_uint(AMP_Box_p box, const char *key, unsigned int value)
{
    /* calculate maximum number of base-10 digits required to represent
     * any `unsigned int' on this system. same as for amp_put_int(), minus
     * one character since we won't need a '-' sign. */
    int bufLen = ((int)(log10(pow(2, sizeof(unsigned int)*8)))) + 2;
    char buf[bufLen];

    snprintf(buf, bufLen, "%u", value);
    return _amp_put_buf(box, key, (unsigned char *)buf, strlen(buf));
}


/* Retrieve and decode an `unsigned int' from an AMP_Box.
 * Stores the decoded integer in to the `unsigned int' pointed to by `value'.
 * Returns 0 on success, or an AMP_* error code on failure. */
int amp_get_uint(AMP_Box_p box, const char *key, unsigned int *value)
{
    int err;
    unsigned char *buf;
    int buf_size;
    long long tmp = 0;

    if ( (err = _amp_get_buf(box, key, &buf, &buf_size)) != 0)
        return err;

    tmp = buftoll(buf, buf_size, &err);
    if (err == 0)
    {
        if (tmp >= 0 && tmp <= UINT_MAX)
        {
            *value = (unsigned int)tmp;
            return 0;
        }
        else
        {
            return AMP_OUT_OF_RANGE;
        }
    }
    else
    {
        /* `err' will have been set to 0 if conversion was successful.
         * Otherwise it will be an AMP_* error code. */
        return err;
    }
}

/* AMP Type: Float (C `double') */

/* Encode and store a `double' in to an AMP_Box.
 * Returns 0 on success, or an AMP_* error code on failure. */
int amp_put_double(AMP_Box_p box, const char *key, double value)
{
    unsigned char buf[100];
    int buf_size;

    if (isnan(value))
    {
        buf_size = 3;
        strncpy((char *)buf, "nan", buf_size);
    }
    else if (isinf(value))
    {
        if (signbit(value) == 0)
        {
            /* positive */
            buf_size = 3;
            strncpy((char *)buf, "inf", buf_size);
        }
        else
        {
            /* negative */
            buf_size = 4;
            strncpy((char *)buf, "-inf", buf_size);
        }
    }
    else
    {
        /* not Infinity or NaN so assume a normal float */
        buf_size = snprintf((char *)buf, 100, "%.17f", value);
    }

    return _amp_put_buf(box, key, buf, buf_size);
}

/* Retrieve and decode a `double' from an AMP_Box.
 * Stores the decoded floating-point number in to the `double'
 * pointed to by `value'.
 * Returns 0 on success, or an AMP_* error code on failure. */
int amp_get_double(AMP_Box_p box, const char *key, double *value)
{
    unsigned char *buf;
    int buf_size;
    int ret;
    int base = 10;    /* we only parse base-10 numbers */
    int any = 0;      /* have we parsed any digits at all? */
    int neg = 0;      /* have parsed a negative sign? */
    int gotDot = 0;   /* have we parsed a dot ('.'), yet? */
    double acc = 0.0; /* accumulator */
    double fractionFactor = 0.1;

    int size;         /* copy of buf_size that we decrement in the loop below */
    int c;            /* the character being parsed */
    unsigned char *s; /* pointer in to input buffer */

    if ( (ret = _amp_get_buf(box, key, &buf, &buf_size)) != 0)
        return ret;

    s = buf;
    size = buf_size;

    if (size < 1)
        return AMP_DECODE_ERROR;

    /* Got at least one char */


    /* check for special values */
    if (size == 3)
    {
        if (strncmp((char *)buf, "inf", 3) == 0)
        {
            /* Return Positive Infinity */
            *value = INFINITY;
            return 0;
        }
        else if (strncmp((char *)buf, "nan", 3) == 0)
        {
            /* Return Not-a-number */
            *value = NAN;
            return 0;
        }
    }
    else if (size == 4 && strncmp((char *)buf, "-inf", 4) == 0)
    {
            /* Return Negative Infinity */
            *value = INFINITY * -1;
            return 0;
    }

    /* Not a recognized special value */

    /* Try to parse it as decimal digits with an optional leading sign
     * character (+ or -). */

    c = *s; /* grab first char - guaranteed to have at least one char in buf at this point */

    if (c == '+')
    {
        /* skip to next char - neg already 0 */
        s++;
        size--;
    }
    else if (c == '-')
    {
        /* skip to next char - set neg = 1 */
        s++;
        size--;
        neg = 1;
    }

    while (size-- > 0)
    {
        c = *s++;

        if (isdigit(c))
        {
            c -= '0';
        }
        else
        {
            if (c == '.' && !gotDot && any)
            {
                /* 1) found a decimal point
                 * 2) haven't found one before
                 * 3) have parsed at least one digit before now */

                gotDot = 1;
                continue;
            }
            else
            {
                return AMP_DECODE_ERROR;
            }
        }

        any = 1; /* have seen at least one digit now */

        if (!gotDot)
        {
            /* parsing integer portion */

            if (neg)
            {
                /* building a negative value... */
                acc *= base;
                acc -= c;
            }
            else
            {
                /* building a positive value... */
                acc *= base;
                acc += c;
            }
        }
        else
        {
            /* parsing fractional portion */

            if (neg)
            {
                /* building a negative value... */
                acc -= c * fractionFactor;
            }
            else
            {
                /* building a positive value... */
                acc += c * fractionFactor;
            }
            fractionFactor /= 10;
        }
    }

    if (!any) /* never parsed a digit */
        return AMP_DECODE_ERROR;

    *value = acc;
    return 0;
}


/* AMP Type: DateTime (C type `AMP_DateTime_p') */


/* An AMP DateTime representation is always 32 bytes. */
#define AMP_DT_SIZE 32


/* Encode and store an `AMP_DateTime' in to an AMP_Box.
 * Returns 0 on success, or an AMP_* error code on failure. */
int amp_put_datetime(AMP_Box_p box, const char *key, AMP_DateTime_p value)
{
    /* snprintf() needs space for an extra \0 even though we don't need it. */
    static uint8_t buf[AMP_DT_SIZE+1];
    char sign;
    int offset_hour, offset_min;

    /* assert values are within valid ranges */
    if (value->year < 1  || value->year > 9999   ||
        value->month < 1 || value->month > 12    ||
        value->day < 1   || value->day > 31      ||
        value->hour < 0  || value->hour > 23     ||
        value->min < 0   || value->min > 59      ||
        value->sec < 0   || value->sec > 59      ||
        value->msec < 0  || value->msec > 999999 ||
        value->utc_offset < -1439 ||
        value->utc_offset > 1439)
    {
        return AMP_ENCODE_ERROR;
    }

    offset_hour = value->utc_offset / 60;
    offset_min  = value->utc_offset % 60;

    if (value->utc_offset >= 0)
        sign = '+';
    else
    {
        sign = '-';
        /* If utc_offset is negative, then these two should be negative too -
         * but we need them to be positive */
        offset_hour = -offset_hour;
        offset_min = -offset_min;
    }

    snprintf((char *)buf, AMP_DT_SIZE+1,
             "%04d-%02d-%02dT%02d:%02d:%02d.%06ld%c%02d:%02d",
             value->year,
             value->month,
             value->day,
             value->hour,
             value->min,
             value->sec,
             value->msec,
             sign,
             offset_hour,
             offset_min);

    return amp_put_bytes(box, key, buf, AMP_DT_SIZE);
}


/* Retrieve and decode an `AMP_DateTime' from an AMP_Box.
 * Stores the decoded data in to the `AMP_DateTime' pointed to by `value'.
 * Returns 0 on success, or an AMP_* error code on failure. On error,
 * `value' may have been partially filled in before the error was detected. */
int amp_get_datetime(AMP_Box_p box, const char *key, AMP_DateTime_p value)
{
    int ret;
    uint8_t *buf;
    int buf_size;
    int err;

    int offset_hour, offset_min;

    if ( (ret = amp_get_bytes(box, key, &buf, &buf_size)) != 0)
        return ret;

    if (buf_size != 32)
        return AMP_DECODE_ERROR;

    /* TODO - as a minor optimization write an integer parsing function that
     * works on `int's instead of `long long's */

    value->year = buftoll_range(buf, 4, 1, 9999, &err);
    if (err)
        return err;

    value->month = buftoll_range(buf+5, 2, 1, 12, &err);
    if (err)
        return err;

    value->day = buftoll_range(buf+8, 2, 1, 31, &err);
    if (err)
        return err;

    value->hour = buftoll_range(buf+11, 2, 0, 23, &err);
    if (err)
        return err;

    value->min = buftoll_range(buf+14, 2, 0, 59, &err);
    if (err)
        return err;

    value->sec = buftoll_range(buf+17, 2, 0, 59, &err);
    if (err)
        return err;

    value->msec = buftoll_range(buf+20, 6, 0, 999999, &err);
    if (err)
        return err;

    offset_hour = buftoll_range(buf+27, 2, 0, 23, &err);
    if (err)
        return err;

    offset_min = buftoll_range(buf+30, 2, 0, 59, &err);
    if (err)
        return err;

    if (buf[26] == '+')
    {
        value->utc_offset = offset_hour * 60 + offset_min;
    }
    else if (buf[26] == '-')
    {
        value->utc_offset = offset_hour * -60 - offset_min;
    }
    else
        return AMP_DECODE_ERROR;

    return 0;
}

