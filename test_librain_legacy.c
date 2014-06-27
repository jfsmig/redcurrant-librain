#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "./librain.h"
#include "./utils.h"

#define kiB 1024
#define MiB (kiB*kiB)

#define BUFFSIZE 512

static void
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

static void
test_erasure_code_recovery (size_t length,
		const char *algo, unsigned int k, unsigned int m)
{
	uint8_t *buf0, *buf1;
	posix_memalign((void**)&buf0, sizeof(long), length);
	posix_memalign((void**)&buf1, sizeof(long), length);
	randomize(buf0, length);
	memcpy(buf1, buf0, length);

	uint8_t **parity = rain_get_coding_chunks(buf0, length, k, m, algo);

	assert(parity != NULL);
	for (unsigned int i=0; i<m ;++i) {
		assert(parity[i] != NULL);
		free(parity[i]);
	}
	free(parity);

	assert(0 == memcmp(buf0, buf1, length));

	free(buf0);
	free(buf1);
}

static void
test_erasure_code_sizes (size_t length, const char *algo,
		unsigned int k, unsigned int m)
{
	struct timespec pre, post, diff;
	uint8_t *buf;

	posix_memalign((void**)&buf, sizeof(long), length);
	randomize(buf, length);

	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &pre);
	uint8_t **parity = rain_get_coding_chunks(buf, length, k, m, algo);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &post);

	assert(parity != NULL);
	for (unsigned int i=0; i<m ;++i) {
		assert(parity[i] != NULL);
		free(parity[i]);
	}
	free(parity);

	if (post.tv_nsec < pre.tv_nsec) {
		diff.tv_sec = post.tv_sec - pre.tv_sec - 1;
		diff.tv_nsec = (1000000000 + post.tv_nsec) - pre.tv_nsec;
	} else {
		diff.tv_sec = post.tv_sec - pre.tv_sec;
		diff.tv_nsec = post.tv_nsec - pre.tv_nsec;
	}

	printf("%-10s %2u %2u %9lu %lu.%09lu\n",
			algo, k, m, length, diff.tv_sec, diff.tv_nsec);

	free(buf);
}

int
main(int argc, char **argv)
{
	(void) argc, (void) argv;

	for (size_t length = 64*kiB; length <= 8*MiB ; length*=2) {
		for (unsigned int k=2; k<8 ;++k)
			test_erasure_code_recovery (length, "liber8tion", k, 2);
		for (unsigned int k=4; k<11 ;++k)
			test_erasure_code_recovery (length, "crs", k, 4);
	}

	for (size_t length = 1*MiB; length <= 256*MiB ; length*=2) {
		for (unsigned int k=2; k<8 ;++k)
			test_erasure_code_sizes (length, "liber8tion", k, 2);
		for (unsigned int k=4; k<11 ;++k)
			test_erasure_code_sizes (length, "crs", k, 4);
	}

	return 0;
}

