/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/* From "C Interfaces and Implementations" - Original license follows:
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
#include <stdarg.h>
#include <stddef.h>
#include <errno.h>

#include "mem.h"
#include "list.h"

List_T *List_push(List_T *list, void *x, int *err)
{
    /* Reports success or failure in the integer pointed to by `err'.
     * On success `err' is set to 0, and the new head of the list
     * is returned.
     *
     * On failure `err' it is set to ENOMEM, and the List_T pointer
     * which was passed in is returned without any changes being made
     * to the list */
    List_T *p;
    NEW(p);
    if (p == NULL)
    {
        *err = ENOMEM;
        return list;
    }
    p->first = x;
    p->rest  = list;
    *err = 0;
    return p;
}

List_T *List_pop(List_T *list, void **x)
{
    if (list)
    {
        List_T *head = list->rest;
        if (x)
            *x = list->first;
        FREE(list);
        return head;
    }
    else
        return list;
}

List_T *List_reverse(List_T *list)
{
    List_T *head = NULL, *next;
    for ( ; list; list = next)
    {
        next = list->rest;
        list->rest = head;
        head = list;
    }
    return head;
}

int List_length(List_T *list)
{
    int n;
    for (n = 0; list; list = list->rest)
        n++;
    return n;
}

void List_free(List_T **list)
{
    List_T *next;
    for ( ; *list; *list = next)
    {
        next = (*list)->rest;
        FREE(*list);
    }
}

void List_map(List_T *list, void apply(void **x, void *cl), void *cl)
{
    for ( ; list; list = list->rest)
        apply(&list->first, cl);
}

/* CURRENTLY UNUSED FUNCTIONS - UNCOMMENT IF YOU NEED TO MAKE USE OF THEM

List_T *List_list(void *x, ...)
{
        va_list ap;
        List_T *list, *p = &list;
        va_start(ap, x);
        for ( ; x; x = va_arg(ap, void *))
        {
                NEW(*p);
                (*p)->first = x;
                p = &(*p)->rest;
        }
        *p = NULL;
        va_end(ap);
        return list;
}

List_T *List_append(List_T *list, List_T *tail)
{
        List_T **p = &list;
        while (*p)
                p = &(*p)->rest;
        *p = tail;
        return list;
}

List_T *List_copy(List_T *list)
{
        List_T *head, *p = &head;
        for ( ; list; list = list->rest)
        {
                NEW(*p);
                (*p)->first = list->first;
                p = &(*p)->rest;
        }
        *p = NULL;
        return head;
}

void **List_toArray(List_T *list, void *end)
{
        int i, n = List_length(list);
        void **array = MALLOC((n + 1)*sizeof (*array));
        for (i = 0; i < n; i++)
        {
                array[i] = list->first;
                list = list->rest;
        }
        array[i] = end;
        return array;
}

*/
