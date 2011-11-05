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
#ifndef _TABLE_CII_H
#define _TABLE_CII_H

typedef struct Table *Table_T;

extern Table_T Table_new (unsigned int hint,
	int cmp(const void *x, const void *y),
	unsigned hash(const void *key));

extern void Table_free(Table_T *table);

extern int Table_length(Table_T table);

extern int Table_num_buckets(Table_T table);

extern int Table_put(Table_T table, const void *key, void *value);

extern void *Table_get(Table_T table, const void *key);

extern void *Table_remove(Table_T table, const void *key);

extern void Table_map(Table_T table,
	void apply(const void *key, void **value, void *cl),
	void *cl);

extern void **Table_toArray(Table_T table, void *end);

#endif
