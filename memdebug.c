#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

static struct {
	int alloc_count;
	int fail_count;
	int fail_start;

} mem;

void *
memdebug_fail (int fail_start)
{
	mem.alloc_count = 0;
	mem.fail_count = 0;
	mem.fail_start = fail_start;
}

void *
memdebug_malloc (size_t size)
{
	if (mem.fail_start <= mem.alloc_count) {
		mem.alloc_count++;
		mem.fail_count++;
		return NULL;
	}

	mem.alloc_count++;
	return malloc(size);
}

int
main (void)
{
	/* Simulate an OOM situation after 5 successful memory allocations */
	memdebug_fail(5);

	void *k;
	int i = 0;
	for (i; i < 10; i++) {
		if ( (k = memdebug_malloc(1)) == NULL) {
			printf("%d: ENOMEM\n", i);
		} else {
			printf("%d: SUCCESS\n", i);
		}

		free(k);
	}

	return 0;
}



