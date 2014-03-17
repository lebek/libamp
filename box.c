/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/*
 * Core implementation of the AMP_Box object.
 *
 */

/* Hash table implementation based on code from "C Interfaces and Implementations":
 *
 * Copyright (c) 1994,1995,1996,1997 by David R. Hanson.
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 * <http://www.opensource.org/licenses/mit-license.php>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#include "amp.h"
#include "amp_internal.h"


static int cmp_string(const void *key1, const void *key2)
{
    return strcmp((const char *)key1, (const char *)key2);
}


static unsigned int hash_string (const void *key)
{
    /* Empty strings will always hash to 0, but that's OK, isn't it? */
    const char *str = (const char *)key;
    unsigned int hash = 0;
    int i;
    for (i = 0; str[i] != 0; i++)
    {
        /* TODO - not sure how great of a distribution this will give us for the
         * typically short ASCII strings we'll be dealing with
         *
         * I think I would rather ROL and XOR, instead of SHL and ADD */
        hash = (hash << 4) + str[i];
    }
    return hash;
}


AMP_Box_T *amp_new_box(void)
{

    AMP_Box_T *box;
    int i, size;
    /* In the future a mechanism may be provided for customizing the number
     * of buckets, but I see no pressing need for it right now.
     *
     * static int primes[] = { 127, 127, 251, 509, 509, 1021, 2053, 4093,
     *                         8191, 16381, 32771, 65521, INT_MAX };
     * for (i = 1; primes[i] < hint; i++)
     *     ;
     * size = primes[i-1];
     */
    size = 127;
    box = MALLOC(sizeof (*box) +
                size*sizeof (box->buckets[0]));
    if (box == NULL)
        return NULL;
    box->size = size;

    box->cmp = cmp_string;
    box->hash = hash_string;
    box->buckets = (struct binding **)(box + 1);
    for (i = 0; i < box->size; i++)
                box->buckets[i] = NULL;
    box->length = 0;
    box->timestamp = 0;

#ifdef AMP_TEST_SUPPORT
    box->get_fail_code = 0;
    box->get_fail_key = NULL;
#endif

    debug_print("New AMP_Box with %d buckets at %p.\n", box->size, box);
    return box;
}


void amp_free_box(AMP_Box_T *box)
{
    /* Free the AMP_Box struct itself
     * along with all the `binding' and `amp_key_value'
     * structs that it owns. */
    if (box == NULL)
        return;

    struct binding *p, *next;
    int i;
    debug_print("Free AMP_Box at %p.\n", box);
    for (i = 0; i < box->size; i++)
    {
        p = box->buckets[i];

        while (p)
        {
            next = p->link;
            free(p->keyval);
            free(p);
            p = next;
        }
    }
    free(box);
}

int amp_num_keys(AMP_Box_T *box)
{
    return box->length;
}

/* Delete a key/value pair out of the AMP_Box.
 *
 * Frees the memory of the stored key and value data.
 *
 * Returns 0 if the key was found, or -1 if the key was not found */
int amp_del_key(AMP_Box_T *box, const char *key)
{
    int i;
    struct binding *p;
    struct binding *prev = NULL;

    i = box->hash(key) % box->size;
    for (p = box->buckets[i]; p; p = p->link)
    {
        if (box->cmp(key, p->keyval->key) == 0)
        {
            /* found it */

            /* was there a previous item in the linked list? */
            if (prev)
            {
                /* YES - link it to the /next/ binding, if there is one.
                 * p->link may be NULL, in which case `prev' is the new
                 * tail of the list */
                prev->link = p->link;
            }
            else
            {
                /* NO - so we're deleting the head of the list -
                 * so set the /next/ binding as the new head of
                 * the list.
                 *
                 * p->link may be NULL, in which case the linked-list
                 * will now be empty */
                box->buckets[i] = p->link;
            }
            free(p->keyval);
            free(p);
            box->length--;
            box->timestamp++;
            return 0;
        }
        prev = p;
    }
    return -1;
}


int amp_has_key(AMP_Box_T *box, const char *key)
{
    int i;
    struct binding *p;
    int keySize = strlen(key);

    i = box->hash(key) % box->size;
    for (p = box->buckets[i]; p; p = p->link)
        if (keySize == p->keyval->keySize &&
            box->cmp(key, p->keyval->key) == 0)
            break;
    return (p ? 1 : 0);
}

int amp_boxes_equal(AMP_Box_T *box, AMP_Box_T *box2)
{
    struct binding *p;
    unsigned char *buf;
    int i, bufSize;

    if (amp_num_keys(box) != amp_num_keys(box2))
        return 0;

    for (i = 0; i < box->size; i++)
    {
        for (p = box->buckets[i]; p; p = p->link)
        {
            if (!amp_has_key(box2, p->keyval->key))
                return 0;

            /* has key - but is value equal? */
            _amp_get_buf(box2, p->keyval->key, &buf, &bufSize);

            if (p->keyval->valueSize != bufSize)
                return 0;

            if (memcmp(p->keyval->value, buf, bufSize) != 0)
                return 0;
            /* values are equal - continue */
        }
    }

    return 1;
}


/* Store a key and an already-encoded value (buffer) in to the
 * box (hash table) */
int _amp_put_buf(AMP_Box_T *box, const char *key,
                 const unsigned char *buf, int buf_size)
{
    int i;
    struct amp_key_value *keyval;
    struct binding *p;
    int keySize;
    int bytesNeeded;

    keySize = strlen(key);
    if (keySize > MAX_KEY_LENGTH || keySize == 0)
        return AMP_BAD_KEY_SIZE;

    if (buf_size > MAX_VALUE_LENGTH || buf_size < 0)
        return AMP_BAD_VAL_SIZE;

    bytesNeeded = 1; /* 1 for NUL-byte at end of key string */
    bytesNeeded += sizeof(struct amp_key_value);
    bytesNeeded += keySize;
    bytesNeeded += buf_size;
    /* TODO - I think this gives us an extra byte, since
     * amp_key_value already contains a variable of type
     * char this is just a placeholder and will be
     * overwritten */

    if ( (keyval = MALLOC(bytesNeeded)) == NULL)
        return ENOMEM;

    /* Initialize amp_key_value */
    keyval->key = &(keyval->_bufferSpaceStartsHere);
    strncpy(keyval->key, key, keySize + 1); /* copy key including NUL */
    keyval->keySize = keySize; /* cache key length */

    /* value falls directly after the key */
    keyval->value = keyval->key + keySize + 1;
    memcpy(keyval->value, buf, buf_size);
    keyval->valueSize = buf_size;

    /* END Initialize amp_key_value */


    /* Now do a hash-table STORE */
    i = box->hash(key) % box->size;

    for (p = box->buckets[i]; p; p = p->link)
        if (box->cmp(key, p->keyval->key) == 0)
            break;
    if (p == NULL)
    {
        if ( (p = MALLOC(sizeof(*p))) == NULL)
        {
            free(keyval);
            return ENOMEM;
        }
        /* link newly allocated `binding' to old head, which could be NULL */
        p->link = box->buckets[i];

        /* become the new head of the linked-list */
        box->buckets[i] = p;

        /* we're bigger now */
        box->length++;
    }
    else
    {
        free(p->keyval); /* free old keyval before
                            replacing it with newly-
                            allocated one */
    }
    /* just replace keyval pointer on the existing `binding' */
    p->keyval = keyval;

    /* not sure if we need this timestamp at all really... it *seems* to
     * only be used, in the original hash-table code, for sanity checking
     * when "mapping" a user-supplied function over the hash-table - it's
     * used to check that the hash-table hasn't been changed (I think) - not
     * sure we can about making that kind of check anywhere...
     *
     * Someone please verify that this is actually the only thing it's used
     * for. */
    box->timestamp++;

    return 0;
}


int _amp_get_buf(AMP_Box_T *box, const char *key,
                 unsigned char **buf, int *size)
{
    int i;
    struct binding *p;

#ifdef AMP_TEST_SUPPORT
    if (box->get_fail_code && !strcmp(key, box->get_fail_key))
    {
        /* simulated failure requsted for this key */
        return box->get_fail_code;
    }
#endif

    i = (*box->hash)(key)%box->size;
    for (p = box->buckets[i]; p; p = p->link)
    {
        if ((*box->cmp)(key, p->keyval->key) == 0)
        {
            *buf = p->keyval->value;
            *size = p->keyval->valueSize;
            return 0;
        }
    }
    return AMP_KEY_NOT_FOUND;
}

/* some places we use `size' for buffer size pointer, other places
 * `buf_size', we should choose one and a use it everywhere */
int amp_serialize_box(AMP_Box_T *box, unsigned char **buf_p, int *size_p)
{
    int i;
    int val_len, key_len;
    struct binding *p;
    unsigned char *buf;

    /* at least 2 bytes for terminating NULL-NULL */
    int size = 2;

    /* repeat double-for-loop.. is there a better way? does it matter?
     * we could at least keep a record of populated buckets to speed
     * up the second loop.. but the time saving would presumably be
     * miniscule given that box->size is rarely going to be > 127 */

    /* calculate memory required for buf */
    for (i = 0; i < box->size; i++)
    {
        for (p = box->buckets[i]; p; p = p->link)
        {
            /* extra 4 bytes for key/value length prefixes */
            size += (4 + p->keyval->keySize + p->keyval->valueSize);
        }
    }


    if (size == 2)
        return AMP_BOX_EMPTY;

    if ( (buf = MALLOC(size)) == NULL)
        return ENOMEM;

    *buf_p = buf;
    *size_p = size;

    /* iterate key-value pairs and populate buffer */
    for (i = 0; i < box->size; i++)
    {
        for (p = box->buckets[i]; p; p = p->link)
        {
            key_len = p->keyval->keySize;
            val_len = p->keyval->valueSize;

            *buf++ = 0;
            *buf++ = (char)key_len;

            memcpy(buf, p->keyval->key, key_len);
            buf += key_len;

            /* We know val_len fits in a 16-bit integer.
             * Thus, right-shifting by 8 will leave us with the
             * most-significant 8 bits, which are placed on the wire
             * first, because we are encoding big-endian values */
            *buf++ = (char)(val_len >> 8);

            /* mask out (zero) all bits except the first 8 bits. */
            *buf++ = (char)(val_len & 0xff);

            memcpy(buf, p->keyval->value, val_len);
            buf += val_len;
        }
    }

    /* NULL-NULL terminator */
    *buf++ = 0;
    *buf   = 0;

    return 0;
}
