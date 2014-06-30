#ifndef LIBRAIN_test_utils_h
# define LIBRAIN_test_utils_h 1

# include <stdio.h>
# include <time.h>
# include <assert.h>

# include "utils.h"

# define PRINTF(FMT,...) fprintf(stdout, FMT, ##__VA_ARGS__)
# define kiB 1024
# define MiB (kiB*kiB)
# define GiB (kiB*kiB*kiB)
# define BUFFSIZE 1024

static inline void
randomize(uint8_t *b, size_t s)
{
	uint8_t tmp[BUFFSIZE];

	memset(b, 0, s);

	// Fill a buffer of random bytes
	FILE *f = fopen("/dev/urandom", "r");
	for (size_t total=0; total < BUFFSIZE ;) {
		ssize_t r = fread(tmp+total, 1, BUFFSIZE-total, f);
		assert (r >= 0);
		total += r;
	}
	fclose(f);
	f = NULL;
	
	// Repeat the buffer into the target
	size_t low = _lower_multiple(s, BUFFSIZE);
	if (low) {
		for (size_t total=0; total < low ;total+=BUFFSIZE)
			memcpy(b+total, tmp, BUFFSIZE);
	}
	if (low != s)
		memcpy(b+low, tmp, s-low);
}

static inline size_t
_elapsed_msec (struct timespec pre, struct timespec post)
{
	struct timespec diff;

	if (post.tv_nsec < pre.tv_nsec) {
		diff.tv_sec = post.tv_sec - pre.tv_sec - 1;
		diff.tv_nsec = (1000000000 + post.tv_nsec) - pre.tv_nsec;
	} else {
		diff.tv_sec = post.tv_sec - pre.tv_sec;
		diff.tv_nsec = post.tv_nsec - pre.tv_nsec;
	}
	return 1000 * diff.tv_sec + _upper_multiple (diff.tv_nsec, 1000000) / 1000000;
}

static inline unsigned int
_count_positives (int *tab)
{
	unsigned int count = 0;
	for (; tab[count]>= 0 ;++count) {}
	return count;
}

#endif // LIBRAIN_test_utils_h
