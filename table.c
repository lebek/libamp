/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/* Modified from "C Interfaces and Implementations" - Original license follows:
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
#include <limits.h>
#include <stddef.h>
#include <errno.h>
#include <assert.h>

#include "mem.h"
#include "table.h"


struct Table
{
    int size;
    int (*cmp)(const void *x, const void *y);
    unsigned (*hash)(const void *key);
    int length;
    unsigned timestamp;
    struct binding
    {
        struct binding *link;
        const void *key;
        void *value;
    } **buckets;
};


Table_T *Table_new(unsigned int hint,
    int cmp(const void *x, const void *y),
    unsigned hash(const void *key))
{
    Table_T *table;
    int i;
    static unsigned int primes[] = { 127, 127, 251, 509, 509, 1021, 2053, 4093,
                                     8191, 16381, 32771, 65521, INT_MAX };
    for (i = 1; primes[i] < hint; i++)
        ;
    table = MALLOC(sizeof (*table) +
        primes[i-1]*sizeof (table->buckets[0]));
    if (table == NULL)
        return NULL;
    table->size = primes[i-1];
    table->cmp  = cmp;
    table->hash = hash;
    table->buckets = (struct binding **)(table + 1);
    for (i = 0; i < table->size; i++)
        table->buckets[i] = NULL;
    table->length = 0;
    table->timestamp = 0;
    return table;
}


void *Table_get(Table_T *table, const void *key)
{
    int i;
    struct binding *p;
    assert(table);
    assert(key);
    i = table->hash(key)%table->size;
    for (p = table->buckets[i]; p; p = p->link)
        if (table->cmp(key, p->key) == 0)
            break;
    return p ? p->value : NULL;
}


int Table_put(Table_T *table, const void *key, void *value)
{
    int i;
    struct binding *p;
    assert(table);
    assert(key);
    i = table->hash(key)%table->size;
    for (p = table->buckets[i]; p; p = p->link)
        if (table->cmp(key, p->key) == 0)
            break;
    if (p == NULL)
    {
        NEW(p);
        if (p == NULL)
            return ENOMEM;
        p->key = key;
        p->link = table->buckets[i];
        table->buckets[i] = p;
        table->length++;
    }
    p->value = value;
    table->timestamp++;
    /* We have no use-case fore returning `prev' here - so I'm changing
     * Table_put to return an int success/error code instead */
    /* return prev; */
    return 0;
}


void *Table_remove(Table_T *table, const void *key)
{
    int i;
    struct binding **pp;
    assert(table);
    assert(key);
    table->timestamp++;
    i = table->hash(key)%table->size;
    for (pp = &table->buckets[i]; *pp; pp = &(*pp)->link)
        if (table->cmp(key, (*pp)->key) == 0)
        {
            struct binding *p = *pp;
            void *value = p->value;
            *pp = p->link;
            FREE(p);
            table->length--;
            return value;
        }
    return NULL;
}


void Table_free(Table_T **table)
{
    assert(table && *table);
    if ((*table)->length > 0)
    {
        int i;
        struct binding *p, *q;
        for (i = 0; i < (*table)->size; i++)
            for (p = (*table)->buckets[i]; p; p = q)
            {
                q = p->link;
                /* I _think_ free()'ing p->value is desirable behavior
                 * for all of our tables */
                FREE(p->value);
                FREE(p);
            }
    }
    FREE(*table);
}

int Table_length(Table_T *table)
{
    /* Number of keys stored in table */
    assert(table);
    return table->length;
}

int Table_num_buckets(Table_T *table)
{
    assert(table);
    return table->size;
}

/* CURRENTLY UNUSED FUNCTIONS - UNCOMMENT IF YOU NEED TO MAKE USE OF THEM

void Table_map(Table_T *table,
    void apply(const void *key, void **value, void *cl),
    void *cl)
{
    int i;
    unsigned stamp;
    struct binding *p;
    assert(table);
    assert(apply);
    stamp = table->timestamp;
    for (i = 0; i < table->size; i++)
        for (p = table->buckets[i]; p; p = p->link)
        {
            apply(p->key, &p->value, cl);
            assert(table->timestamp == stamp);
        }
}

void **Table_toArray(Table_T *table, void *end)
{
    int i, j = 0;
    void **array;
    struct binding *p;
    assert(table);
    array = MALLOC((2*table->length + 1)*sizeof (*array));
    for (i = 0; i < table->size; i++)
        for (p = table->buckets[i]; p; p = p->link)
        {
            array[j++] = (void *)p->key;
            array[j++] = p->value;
        }
    array[j] = end;
    return array;
}


*/
