#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <alloca.h>

#include "./librain.h"
#include "./utils.h"

#define kiB 1024
#define MiB (kiB*kiB)

#define BUFFSIZE 512

static const char *
_algo_to_str (enum rain_algorithm_e algo)
{
	switch (algo) {
		case JALG_liberation: return "liber8tion";
		case JALG_crs: return "crs";
		default: return "invalid";
	}
}

static size_t
_elapsed_usec (struct timespec pre, struct timespec post)
{
	struct timespec diff;

	if (post.tv_nsec < pre.tv_nsec) {
		diff.tv_sec = post.tv_sec - pre.tv_sec - 1;
		diff.tv_nsec = (1000000000 + post.tv_nsec) - pre.tv_nsec;
	} else {
		diff.tv_sec = post.tv_sec - pre.tv_sec;
		diff.tv_nsec = post.tv_nsec - pre.tv_nsec;
	}
	return ((1000000000 * diff.tv_sec) + diff.tv_nsec) / 1000;
}

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
test_size (size_t length, const char *algo, unsigned int k, unsigned int m)
{
	struct rain_encoding_s enc;

	if (rain_get_encoding (&enc, length, k, m, algo)) {

		double overhead = (double)((k+m)*enc.block_size) / (double) enc.data_size;

		printf("SIZE %s %u+%u W=%u PS=%lu s=%lu DS=%lu PDS=%lu BS=%lu %f\n",
				algo, k, m , enc.w, enc.packet_size, enc.strip_size,
				enc.data_size, enc.padded_data_size, enc.block_size,
				overhead);
	} else {
		printf("SIZE %s %u+%u W=%u PS=%lu s=%lu DS=%lu PDS=%lu BS=%lu -\n",
				algo, k, m , enc.w, enc.packet_size, enc.strip_size,
				enc.data_size, enc.padded_data_size, enc.block_size);
	}
}

static void
test_sizes_around (size_t length, const char *algo,
		unsigned int k, unsigned int m)
{
	test_size (length-1, algo, k, m);
	test_size (length, algo, k, m);
	test_size (length+1, algo, k, m);
	test_size (length+2, algo, k, m);
	test_size (length+3, algo, k, m);
}

static void
test_encoding (size_t length, const char *algo, unsigned int k, unsigned int m)
{
	struct rain_encoding_s enc;
	struct timespec pre, post;
	int rc = 0;

	if (rain_get_encoding (&enc, length, k, m, algo)) {
		uint8_t *buf, *parity[m+1];

		posix_memalign((void**)&buf, sizeof(long), enc.padded_data_size);
		randomize(buf, length);
		memset(buf + length, 0, enc.padded_data_size - length);
		memset(parity, 0, (m+1) * sizeof(uint8_t*));

		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &pre);
		rc = rain_encode (buf, length, &enc, NULL, parity);
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &post);

		free(buf);
		assert(rc != 0);
		for (unsigned int i=0; i<m ;++i) {
			assert(parity[i] != NULL);
			free(parity[i]);
		}
		assert(parity[m] == NULL);
	}

	if (!rc) {
		printf("ENC %s %u+%u DS=%lu - -\n",
				algo, k, m, enc.data_size);
	} else {

		size_t elapsed = _elapsed_usec(pre, post);
		double throughput = ((double) enc.data_size / (double) elapsed) * 1000000.0;
		static const double DGiB = 1.0 * (1024 * MiB);

		printf("ENC %s %u+%u DS=%lu %lu %f\n",
				algo, k, m, enc.data_size, elapsed, throughput / DGiB);
	}
}

static unsigned int
_count_positives (int *tab)
{
	unsigned int count = 0;
	for (; tab[count]>= 0 ;++count) {}
	return count;
}

static void
test_rehydrate (struct rain_encoding_s *enc, uint8_t **data, uint8_t **parity,
		int *deleted)
{
	unsigned int count_deleted = _count_positives(deleted);
	assert(count_deleted < enc->k + enc->m);

	// save the blocs to be restored
	uint8_t *saved[count_deleted];
	for (unsigned int i=0; i<count_deleted ;++i) {
		if (i < enc->k) {
			saved[i] = data[i];
			data[i] = NULL;
		}
		else {
			saved[i] = parity[i - enc->k];
			parity[i - enc->k] = NULL;
		}
	}

	fprintf(stderr, "REHYDRATE %s %u %u lost=%u %lu", _algo_to_str(enc->algo),
			enc->k, enc->m, count_deleted, enc->data_size);

	struct timespec pre, post;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &pre);
	int rc = rain_rehydrate (data, parity, enc, NULL);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &post);
	assert(rc != 0);

	size_t elapsed = _elapsed_usec(pre, post);
	double throughput = ((double)enc->data_size) / (double)elapsed;
	throughput *= 1000000.0;
	fprintf(stderr, " %f %f\n", ((double)elapsed)/1000000.0, throughput);

	// Check the blocks are correct
	for (unsigned int i=0; i<count_deleted ;++i) {
		if (i < enc->k) {
			assert(data[i] != NULL);
			assert(0 == memcmp(data[i], saved[i], enc->block_size));
		} else {
			assert(parity[i - enc->k] != NULL);
			assert(0 == memcmp(parity[i - enc->block_size], saved[i], enc->block_size));
		}
	}

	// restore the blocks to be restored
	for (unsigned int i=0; i<count_deleted ;++i) {
		if (i < enc->k) {
			free(data[i]);
			data[i] = saved[i];
		} else {
			free(parity[i - enc->k]);
			parity[i - enc->k] = saved[i];
		}
	}
}

struct rehydrator_s
{
	struct rain_encoding_s *enc;
	uint8_t **data;
	uint8_t **parity;
	int *deleted;
};

static void
_generator_rehydrate(struct rehydrator_s *gen,
		unsigned int tail, unsigned int remaining)
{
	if (remaining <= 0 || tail >= gen->enc->k + gen->enc->m) {
		if (tail <= 0 || remaining >= gen->enc->m) // nothing deleted
			return;
		test_rehydrate (gen->enc, gen->data, gen->parity, gen->deleted);
	}
	else {
		// Do not delete block[tail] and recurse
		_generator_rehydrate(gen, tail+1, remaining);

		// Delete block[tail], recurse, and undelete
		unsigned int count = _count_positives (gen->deleted);
		gen->deleted[count] = tail;
		_generator_rehydrate(gen, tail+1, remaining-1);
		gen->deleted[count] = -1;
	}
}

static void
test_roundtrip (size_t length, const char *algo, unsigned int k, unsigned int m)
{
	struct rain_encoding_s enc;
	struct rehydrator_s gen;
	int rc;

	rc = rain_get_encoding (&enc, length, k, m, algo);
	if (!rc)
		return;

	// Prepare a padded buffer
	uint8_t *buf = NULL;
	posix_memalign((void**)&buf, sizeof(long), enc.padded_data_size);
	randomize(buf, length);
	memset(buf + length, 0, enc.padded_data_size - length);

	// Prepare the recursion context
	memset(&gen, 0, sizeof(gen));
	gen.enc = &enc;
	gen.data = alloca(k * sizeof(uint8_t*));
	gen.parity = alloca(m * sizeof(uint8_t*));
	gen.deleted = alloca((k+m+1) * sizeof(int));
	for (size_t i=0; i<m ;++i)
		gen.parity[i] = NULL;
	for (size_t i=0; i<k ;++i)
		gen.data[i] = buf + (i * enc.block_size);
	for (unsigned int i=0; i<(k+m+1) ;++i)
		gen.deleted[i] = -1;

	// Encode once and try all the combinations of lost blocks
	rc = rain_encode (buf, length, &enc, NULL, gen.parity);
	assert(rc != 0);
	_generator_rehydrate(&gen, 0, m);

	free(buf);
}

int
main(int argc, char **argv)
{
	(void) argc, (void) argv;

	// Benchmark the rehydrating throughput
	for (size_t length = 1*kiB; length <= 512*MiB ; length*=4) {
		for (unsigned int k=2; k<8 ;++k)
			test_roundtrip (length, "liber8tion", k, 2);
		for (unsigned int k=4; k<11 ;++k)
			test_roundtrip (length, "crs", k, 4);
	}

	// Checks
	for (size_t length = 1*kiB; length <= 1048*MiB ; length*=2) {
		for (unsigned int k=2; k<8 ;++k)
			test_sizes_around (length, "liber8tion", k, 2);
		for (unsigned int k=4; k<11 ;++k)
			test_sizes_around (length, "crs", k, 4);
	}

	// Benchmark the encoding throughput
	for (size_t length = 1*kiB; length <= 512*MiB ; length*=4) {
		for (unsigned int k=2; k<8 ;++k)
			test_encoding (length, "liber8tion", k, 2);
		for (unsigned int k=4; k<11 ;++k)
			test_encoding (length, "crs", k, 4);
	}

	return 0;
}

