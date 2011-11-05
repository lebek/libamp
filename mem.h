/* Copyright (c) 2011 - Eric P. Mangold
 * Copyright (c) 2011 - Peter Le Bek
 *
 * See LICENSE.txt for details.
 */

#ifndef _AMP_MEM_H
#define _AMP_MEM_H

#include <stdlib.h>
#include <stdbool.h>

#ifdef AMP_TEST_SUPPORT

typedef void * (*malloc_ptr)(size_t size);

/* Set by unit tests to specify a char to fill malloc'd memory with. */
char mem_check_char;

extern malloc_ptr the_real_malloc; /* pointer to the real malloc()
                                      function */

/* If set to true then test_malloc() will begin to fail when
 * allocations_until_failure  <= 0. Otherwise test_malloc()
 * will always succeed without decrementing allocations_until_failure. */
extern bool failure_mode_enabled;

/* The number of times remaining that test_malloc() may be called before
 * it begins to return NULL. test_malloc() decrements this variable, and
 * when it is <= 0 test_malloc() returns NULL without allocating memory. */
extern int allocations_until_failure;

/* Whenever test_malloc() returns NULL this variable is set to true. */
extern bool allocation_failure_occurred;

/* Allocate a region of memory and memset it with char `c'. Can also
 * be rigged to fail (above) to test allocation failure error handling. */
extern void *test_malloc(size_t size, char c);
#define MALLOC(size) test_malloc(size, mem_check_char)

/* Set state of test_malloc() implementation */
void enable_malloc_failures(int times_until_failure);
void disable_malloc_failures();

#else
#define MALLOC(size) malloc(size)
#endif

#define  NEW(p) ((p) = MALLOC((long)sizeof *(p)))
#define FREE(ptr) ((void)(free((ptr)), (ptr) = 0))
#endif
