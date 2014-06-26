#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <stdio.h>
#define DEBUG(X,...) fprintf(stderr,X,##__VA_ARGS__)

#include <jerasure/jerasure.h>
#include <jerasure/liberation.h>
#include <jerasure/cauchy.h>
#include <jerasure/galois.h>
#include <jerasure/reed_sol.h>
#include "librain.h"

#define PACKET_VALUE 1024
#define BUFFER_VALUE 4096

#define MACRO_COND(C,A,B) ((B) ^ (((A)^(B)) & -(C)))

#define B0 0x5555555555555555
#define B1 0x3333333333333333
#define B2 0x0F0F0F0F0F0F0F0F
#define B3 0x0000000000FF00FF
#define B4 0x0000FFFF0000FFFF
#define B5 0x00000000FFFFFFFF

static inline size_t
_upper_power (register size_t v)
{
	// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	v --;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;
	return ++v;
}

static inline size_t
_upper_multiple (size_t v, size_t m)
{
	size_t mod = v % m;
	return v + MACRO_COND(mod!=0,m-mod,0);
}

static inline size_t
_lower_multiple (size_t v, size_t m)
{
	size_t mod = v % m;
	return v - MACRO_COND(mod!=0,mod,0);
}

static inline size_t
_closest_multiple(size_t v, size_t m)
{
	size_t up = _upper_multiple(v, m);
	size_t down = _upper_multiple(v, m);
	size_t mid = down + (m/2);
	return MACRO_COND(v<mid,down,up);
}

static inline unsigned int
_count_bits (register size_t c)
{
	c = c - ((c >> 1) & B0);
	c = ((c >> 2) & B1) + (c & B1);
	c = ((c >> 4) + c)  & B2;
	c = ((c >> 8) + c)  & B3;
	c = ((c >> 16) + c) & B4;
	c = ((c >> 32) + c) & B5;
	return c;
}

static inline uint8_t*
memdup(const void *src, size_t len)
{
	void *dst = malloc(len);
	memcpy(dst, src, len);
	return dst;
}

/* ------------------------------------------------------------------------- */

enum algorithm_e { JALG_unset, JALG_liberation, JALG_crs };

struct encoding_s
{
	size_t packet_size;
	size_t block_size;
	unsigned int w;
	enum algorithm_e algo;
};

static int
encoding_prepare (struct encoding_s *encoding,
		const char *algo, unsigned int k, unsigned int m,
		size_t len)
{
	assert(algo != NULL);
	assert(k > 0);
	assert(m > 0);
	assert(encoding != NULL);

	memset(encoding, 0, sizeof(struct encoding_s));
	encoding->algo = JALG_unset;
	encoding->w = 0;

    if (!strcmp("liber8tion", algo)) {
        if (m != 2 || k < 2 || k > 7) {
			errno = EINVAL;
            return 0;
		}
		encoding->algo = JALG_liberation;
    }
    else if (!strcmp("crs", algo)) {
        if (((unsigned int)-1) == encoding->w) {
			errno  = EINVAL;
            return 0;
		}
		encoding->algo = JALG_crs;
    }
    else {
		errno  = EINVAL;
        return 0;
	}

	// Set "w"
	if (encoding->algo == JALG_liberation) {
        encoding->w = 1 * sizeof(long);
	} else if (encoding->algo == JALG_crs) {
		encoding->w = _count_bits(_upper_power(k+m-1) - 1);
	}

	// Set PacketSize
	encoding->packet_size = encoding->w;
	encoding->packet_size = _upper_multiple(encoding->packet_size, sizeof(long));
	encoding->packet_size = _upper_power(encoding->packet_size);

	// Deduct buffer_size
	const int block = sizeof(int) * k * encoding->w;

	size_t buffer_size = _closest_multiple (BUFFER_VALUE, block * encoding->packet_size);

	// Deduct the real block size
    size_t new_size = encoding->packet_size
		? _upper_multiple (len, block * encoding->packet_size)
		: _upper_multiple (len, block);
	new_size = _upper_multiple(new_size, buffer_size);
	encoding->block_size = new_size / k;

	return 1;
}

/* ------------------------------------------------------------------------- */

int
get_overhead_percentage(size_t raw_data_size, unsigned int k,
		size_t chunk_size, const char* algo)
{
	assert(algo != NULL);
	assert(raw_data_size > 0);
	assert(k > 0);
	assert(chunk_size > 0);

	if (0 == strcmp("liber8tion", algo)) {
		if (k < 2 || k > 7)
			return -1;
	}
	else if (0 != strcmp("crs", algo))
		return 0;

	return (((k * chunk_size) - raw_data_size) * 100) / raw_data_size;
}

ssize_t
get_chunk_size(size_t len, unsigned int k, unsigned int m, const char* algo)
{
	struct encoding_s encoding;
	if (!encoding_prepare(&encoding, algo, k, m, len))
		return -1;
	return encoding.block_size;
}

int
rain_repair_and_get_raw_data(uint8_t **data, uint8_t **coding,
		size_t raw_data_size, unsigned int k, unsigned int m,
		const char* algo)
{
	assert(data != NULL);
	assert(coding != NULL);

	struct encoding_s encoding;
	if (!encoding_prepare(&encoding, algo, k, m, raw_data_size))
		return EXIT_FAILURE;

	/* Creating coding matrix or bitmatrix */
	int *bit_matrix=NULL, *matrix=NULL;
	if (encoding.algo == JALG_liberation)
		bit_matrix = liber8tion_coding_bitmatrix(k);
	else if (encoding.algo == JALG_crs) {
		matrix = cauchy_good_general_coding_matrix(k, m, encoding.w);
		bit_matrix = jerasure_matrix_to_bitmatrix(k, m, encoding.w, matrix);
	}

	int erased[k+m], erasures[k+m];
	memset(erased, -1, sizeof(int)*(k+m));
	memset(erasures, -1, sizeof(int)*(k+m));

	/* Finding erased chunks */
	unsigned int num_erased = 0;
	for (unsigned int i = 0; i < k; i++) {
		if (data[i] == NULL) {
			erased[i] = 1;
			erasures[num_erased] = i;
			num_erased++;
		}
	}
	for (unsigned int i = 0; i < m; i++) {
		if (coding[i] == NULL) {
			erased[k + i] = 1;
			erasures[num_erased] = k + i;
			num_erased++;
		}
	}
	if (num_erased > m) // situation not recoverable
		return EXIT_FAILURE;

	/* Now allocate data & coding for missing parts */
	for (unsigned int i=0; i < num_erased; i++) {
		unsigned int idx = (unsigned int) erasures[i];
		uint8_t *block = (uint8_t*) calloc(encoding.block_size, sizeof(uint8_t));
		if (k > idx) {
			assert(data[idx] == NULL);
			data[idx] = block;
		} else {
			assert(coding[idx] == NULL);
			coding[idx-k] = block;
		}
	}

	/* Choose proper decoding method */
	jerasure_schedule_decode_lazy(k, m, encoding.w,
			bit_matrix, erasures, (char**)data, (char**)coding,
			encoding.block_size, encoding.packet_size, 1);

	/* Freeing previously allocated memory */
	if (bit_matrix)
		free(bit_matrix);

	return EXIT_SUCCESS;
}


uint8_t**
rain_get_coding_chunks(uint8_t *raw_data, size_t raw_data_size,
		unsigned int k, unsigned int m, const char* algo)
{
	// Parse the arguments and compute derivated values
	assert(raw_data != NULL);

	if (!raw_data_size)
		return NULL;

	struct encoding_s encoding;
	if (!encoding_prepare(&encoding, algo, k, m, raw_data_size))
		return NULL;

	// Prepare the jerasure structures
	int *bit_matrix=NULL, *matrix=NULL, **schedule=NULL;
	if (encoding.algo == JALG_liberation) {
		matrix = NULL;
		bit_matrix = liber8tion_coding_bitmatrix(k);
		schedule = jerasure_smart_bitmatrix_to_schedule(k, m, encoding.w, bit_matrix);
	}
	else if (encoding.algo == JALG_crs) {
		matrix = cauchy_good_general_coding_matrix(k, m, encoding.w);
		bit_matrix = jerasure_matrix_to_bitmatrix(k, m, encoding.w, matrix);
		schedule = jerasure_smart_bitmatrix_to_schedule(k, m, encoding.w, bit_matrix);
	}

	// Prepare the empty parity blocks
	uint8_t *parity [m];
	for (unsigned int i=0; i<m ;++i)
		parity[i] = (uint8_t*) calloc(encoding.block_size, sizeof(uint8_t));

	// Prepare the data blocks (no copy, point to the original)
	uint8_t *data [k];
	size_t srcoffset = 0;
	for (unsigned int i=0; i<k ;++i) {
		data[i] = raw_data + srcoffset;
		srcoffset += encoding.block_size;
	}

	// If a padding is  necessary, replace the last data block
	// with a padded copy (no way to check if the original copy
	// is long enough.
	uint8_t *last_with_padding = NULL;
	size_t tail_length = raw_data_size % encoding.block_size;
	if (tail_length != 0) {
		size_t tail_offset = _lower_multiple(raw_data_size, encoding.block_size);
		last_with_padding = (uint8_t*) calloc(encoding.block_size, sizeof(uint8_t));
		assert(last_with_padding != NULL);

		memcpy(last_with_padding, raw_data+tail_offset, tail_length);
		data[k-1] = last_with_padding;
	}

	// Compute now!
	jerasure_schedule_encode(k, m, encoding.w, schedule,
			(char**) data, (char**) parity,
			encoding.block_size, encoding.packet_size);

	// Free allocated structures
	if (last_with_padding)
		free(last_with_padding);
	if (schedule)
		jerasure_free_schedule(schedule);
	if (bit_matrix)
		free(bit_matrix);

	return (uint8_t**) memdup(parity, sizeof(uint8_t*)*m);
}

