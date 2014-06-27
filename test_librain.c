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
test_encoding (size_t length, const char *algo,
		unsigned int k, unsigned int m)
{
	struct rain_encoding_s enc;
	struct timespec pre, post, diff={0,0};
	uint8_t *buf, *parity[m];
	int rc = 0;

	memset(parity, 0, m * sizeof(uint8_t*));
	posix_memalign((void**)&buf, sizeof(long), length);
	randomize(buf, length);

	if (rain_get_encoding (&enc, length, k, m, algo)) {
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &pre);
		rc = rain_encode (buf, length, &enc, NULL, parity);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &post);
		assert(rc != 0);
	}

	free(buf);

	if (rc) {
		for (unsigned int i=0; i<m ;++i) {
			assert(parity[i] != NULL);
			free(parity[i]);
		}

		if (post.tv_nsec < pre.tv_nsec) {
			diff.tv_sec = post.tv_sec - pre.tv_sec - 1;
			diff.tv_nsec = (1000000000 + post.tv_nsec) - pre.tv_nsec;
		} else {
			diff.tv_sec = post.tv_sec - pre.tv_sec;
			diff.tv_nsec = post.tv_nsec - pre.tv_nsec;
		}

		size_t elapsed = ((1000000000 * diff.tv_sec) + diff.tv_nsec) / 1000;
		double overhead = (double)((k+m)*enc.block_size) / (double) enc.data_size;
		double throughput = ((double) enc.data_size / (double) elapsed) * 1000000.0;
		static const double DGiB = 1.0 * (1024 * MiB);

		fprintf(stderr, "%s %u %u W=%u PS=%lu s=%lu"
				" DS=%lu PDS=%lu BS=%lu"
				" %f %lu %f\n",
				algo, k, m , enc.w, enc.packet_size, enc.strip_size,
				enc.data_size, enc.padded_data_size, enc.block_size,
				overhead, elapsed, throughput / DGiB);
	} else {
		fprintf(stderr, "%s %u %u W=%u PS=%lu s=%lu"
				" DS=%lu PDS=%lu BS=%lu"
				" - - -\n",
				algo, k, m , enc.w, enc.packet_size, enc.strip_size,
				enc.data_size, enc.padded_data_size, enc.block_size);
	}
}

static void
test_encoding_around (size_t length, const char *algo,
		unsigned int k, unsigned int m)
{
	test_encoding (length-1, algo, k, m);
	test_encoding (length, algo, k, m);
	test_encoding (length+1, algo, k, m);
}

int
main(int argc, char **argv)
{
	(void) argc, (void) argv;

	for (size_t length = 1*kiB; length <= 256*MiB ; length*=2) {
		for (unsigned int k=2; k<8 ;++k)
			test_encoding_around (length, "liber8tion", k, 2);
		for (unsigned int k=4; k<11 ;++k)
			test_encoding_around (length, "crs", k, 4);
	}

	return 0;
}

