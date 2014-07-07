#include <stdlib.h>
#include <string.h>

#include "./librain.h"
#include "./test_utils.h"

static void
test_erasure_code_recovery (size_t length,
		const char *algo, unsigned int k, unsigned int m)
{
	uint8_t *buf0=NULL, *buf1=NULL;
	int rc;

	rc = posix_memalign((void**)&buf0, sizeof(long), length);
	assert(rc == 0 && buf0 != NULL);
	rc = posix_memalign((void**)&buf1, sizeof(long), length);
	assert(rc == 0 && buf1 != NULL);
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
	struct timespec pre, post;
	uint8_t *buf;

	int rc = posix_memalign((void**)&buf, sizeof(long), length);
	assert(rc == 0 && buf != NULL);
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
	free(buf);

	PRINTF("%-10s %2u %2u %9lu %lu\n", algo, k, m, length, _elapsed_msec(pre, post));
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

