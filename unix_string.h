/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

/* Some string functions not available natively on Windows */

#ifndef _UNIX_STRING_H
#define _UNIX_STRING_H

char *strcasestr(const char *s, const char *find);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);

#endif
