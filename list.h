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

#ifndef _LIST_CII_H
#define _LIST_CII_H

typedef struct List List_T;
struct List
{
        List_T *rest;
        void *first;
};
extern List_T *     List_append (List_T *list, List_T *tail);
extern List_T *     List_copy   (List_T *list);
extern List_T *     List_list   (void *x, ...);
extern List_T *     List_pop    (List_T *list, void **x);
extern List_T *     List_push   (List_T *list, void *x, int *err);
extern List_T *     List_reverse(List_T *list);
extern int          List_length (List_T *list);
extern void         List_free   (List_T **list);
extern void         List_map    (List_T *list,
                                 void apply(void **x, void *cl), void *cl);
extern void **      List_toArray(List_T *list, void *end);

#endif
