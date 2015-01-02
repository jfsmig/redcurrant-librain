#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <jerasure/jerasure.h>
#include <jerasure/liberation.h>
#include <jerasure/cauchy.h>
#include <jerasure/galois.h>
#include <jerasure/reed_sol.h>

#include "librain.h"
#include "utils.h"

static struct rain_env_s env_DEFAULT = { malloc, calloc, free };

static int
encoding_prepare (struct rain_encoding_s *enc,
		const char *algo, unsigned int k, unsigned int m,
		size_t length)
{
	assert(algo != NULL);
	assert(k > 0);
	assert(m > 0);
	assert(enc != NULL);

	memset(enc, 0, sizeof(struct rain_encoding_s));

    if (!strcmp("liber8tion", algo)) {
        if (m != 2 || k < 2 || k > 7) {
			errno = EINVAL;
            return 0;
		}
		enc->algo = JALG_liberation;
    }
    else if (!strcmp("crs", algo)) {
		enc->algo = JALG_crs;
    }
    else {
		errno  = EINVAL;
        return 0;
	}

	enc->data_size = length;
	enc->k = k;
	enc->m = m;

	// Set "w" and "packet_size" : for erasure codes computations, the data
	// is divided into "k" blocks and each block is divided into "s" strips.
	// Each strip is then computed as "w" packets of size "packet_size" (PS).
	// The <w,PS> combination has to be decided the empiric way (after
	// benchmarks) because the result can vary a lot, depending on the CPU's
	// architecture, CPU caches, etc.
	// cf. https://www.usenix.org/legacy/events/fast09/tech/full_papers/plank/plank_html/

	if (enc->algo == JALG_liberation) {
		enc->w = 8;
	} else if (enc->algo == JALG_crs) {
		enc->w = 4;
	}

	// Retry with intermediate values (this loop can be optimized)
	for (size_t start = 2048; start >= 1280; start -= 256) {
		for (size_t p_size = start; p_size >= 64; p_size /= 2) {
			enc->packet_size = p_size;
			enc->strip_size = enc->packet_size * enc->w;
			const size_t ks = enc->k * enc->strip_size;
			enc->padded_data_size = _upper_multiple(enc->data_size, ks);
			enc->block_size = enc->padded_data_size / enc->k;

			// More than a block of padding ?
			if ((enc->padded_data_size != enc->data_size)
				&& (enc->data_size < (enc->padded_data_size - enc->block_size)))
					continue;
			return 1;
		}
	}

	// We will have more than one block of padding, but it will work
	enc->packet_size = 64;
	enc->strip_size = enc->packet_size * enc->w;
	const size_t ks = enc->k * enc->strip_size;
	enc->padded_data_size = _upper_multiple(enc->data_size, ks);
	enc->block_size = enc->padded_data_size / enc->k;

	return 1;
}

/* ------------------------------------------------------------------------- */

int
rain_get_encoding (struct rain_encoding_s *encoding, size_t rawlength,
		unsigned int k, unsigned int m, const char *algo)
{
	assert(encoding != NULL);
	return encoding_prepare(encoding, algo, k, m, rawlength);
}

int
rain_rehydrate(uint8_t **data, uint8_t **coding,
		struct rain_encoding_s *enc, struct rain_env_s *env)
{
	assert(data != NULL);
	assert(coding != NULL);
	assert(enc != NULL);
	if (!env)
		env = &env_DEFAULT;

	/* Creating coding matrix or bitmatrix */
	int *bit_matrix=NULL, *matrix=NULL;
	if (enc->algo == JALG_liberation)
		bit_matrix = liber8tion_coding_bitmatrix(enc->k);
	else if (enc->algo == JALG_crs) {
		matrix = cauchy_good_general_coding_matrix(enc->k, enc->m, enc->w);
		bit_matrix = jerasure_matrix_to_bitmatrix(enc->k, enc->m, enc->w, matrix);
	}

	/* Finding erased chunks */
	const unsigned int sum = enc->k + enc->m;
	int erased[sum], erasures[sum];
	unsigned int num_erased = 0;
	size_t cur_size = 0;
	memset(erased, -1, sizeof(int)*(sum));
	memset(erasures, -1, sizeof(int)*(sum));
	for (unsigned int i = 0; i < enc->k; i++) {
		if (data[i] == NULL) {
			if (cur_size < enc->data_size) {
				erased[i] = 1;
				erasures[num_erased] = i;
				num_erased++;
			} else {
				// Full block of padding
				data[i] = env->calloc(enc->block_size, sizeof(uint8_t));
			}
		}
		cur_size += enc->block_size;
	}
	for (unsigned int i = 0; i < enc->m; i++) {
		if (coding[i] == NULL) {
			erased[enc->k + i] = 1;
			erasures[num_erased] = enc->k + i;
			num_erased++;
		}
	}
	if (num_erased > enc->m) // so sad ... not recoverable
		return EXIT_FAILURE;

	/* Now allocate data & coding for missing parts */
	for (unsigned int i=0; i < num_erased; i++) {
		unsigned int idx = (unsigned int) erasures[i];
		uint8_t *block = (uint8_t*) env->calloc(enc->block_size, sizeof(uint8_t));
		if (enc->k > idx) {
			assert(data[idx] == NULL);
			data[idx] = block;
		} else {
			assert(coding[idx - enc->k] == NULL);
			coding[idx - enc->k] = block;
		}
	}

	/* Choose proper decoding method */
	jerasure_schedule_decode_lazy(enc->k, enc->m, enc->w,
			bit_matrix, erasures, (char**)data, (char**)coding,
			enc->block_size, enc->packet_size, 1);

	/* Freeing previously allocated memory */
	if (bit_matrix)
		free(bit_matrix);
	if (matrix)
		free(matrix);

	return 1;
}

int
rain_encode (uint8_t *rawdata, size_t rawlength,
		struct rain_encoding_s *encoding, struct rain_env_s *env,
		uint8_t **out)
{
	assert(encoding != NULL);
	assert(rawdata != NULL);
	assert(rawlength > 0);

	if (!env)
		env = &env_DEFAULT;

	// Prepare the jerasure structures
	int *bit_matrix=NULL, *matrix=NULL, **schedule=NULL;
	if (encoding->algo == JALG_liberation) {
		matrix = NULL;
		bit_matrix = liber8tion_coding_bitmatrix(encoding->k);
		schedule = jerasure_smart_bitmatrix_to_schedule(encoding->k, encoding->m,
				encoding->w, bit_matrix);
	}
	else if (encoding->algo == JALG_crs) {
		matrix = cauchy_good_general_coding_matrix(
				encoding->k, encoding->m, encoding->w);
		bit_matrix = jerasure_matrix_to_bitmatrix(
				encoding->k, encoding->m, encoding->w, matrix);
		schedule = jerasure_smart_bitmatrix_to_schedule(
				encoding->k, encoding->m, encoding->w, bit_matrix);
	}

	// Prepare the empty parity blocks
	uint8_t *parity [encoding->m];
	for (unsigned int i=0; i < encoding->m ;++i) 
		parity[i] = (uint8_t*) env->calloc(sizeof(uint8_t), encoding->block_size);

	// Prepare the data blocks (no copy, point to the original)
	uint8_t *data [encoding->k];
	size_t srcoffset = 0;
	unsigned int cur_chunk = 0;
	for (; cur_chunk < encoding->k && srcoffset < rawlength; ++cur_chunk) {
		data[cur_chunk] = rawdata + srcoffset;
		srcoffset += encoding->block_size;
	}
	// Point on the last chunk with actual data
	--cur_chunk;

	// If a padding is necessary, replace the last provided data block
	// with a padded copy (no way to check if the original copy
	// is long enough.
	uint8_t *last_with_padding = NULL;
	size_t tail_length = rawlength % encoding->block_size;
	if (tail_length != 0) {
		size_t tail_offset = _lower_multiple(rawlength, encoding->block_size);
		last_with_padding = (uint8_t*) env->calloc(encoding->block_size, sizeof(uint8_t));
		assert(last_with_padding != NULL);

		memcpy(last_with_padding, rawdata+tail_offset, tail_length);
		data[cur_chunk] = last_with_padding;
	}
	// Allocate missing padding chunks
	for (++cur_chunk; cur_chunk < encoding->k; ++cur_chunk) {
		data[cur_chunk] = (uint8_t*) env->calloc(encoding->block_size, sizeof(uint8_t));
	}

	// Compute now ... damned, no return code to check
	jerasure_schedule_encode(encoding->k, encoding->m, encoding->w, schedule,
			(char**) data, (char**) parity,
			encoding->block_size, encoding->packet_size);

	// Free allocated structures
	if (last_with_padding)
		env->free(last_with_padding);
	if (schedule)
		jerasure_free_schedule(schedule);
	if (bit_matrix)
		free(bit_matrix);
	if (matrix)
		free(matrix);

	for (unsigned int i=0; i<encoding->m ;++i)
		out[i] = parity[i];

	return 1;
}

#ifndef HAVE_NOLEGACY
/* ------------------------------------------------------------------------- */

uint8_t**
rain_get_coding_chunks(uint8_t *data, size_t length,
		unsigned int k, unsigned int m, const char* algo)
{
	if (length == 0)
		return NULL;

	struct rain_encoding_s encoding;
	if (!encoding_prepare(&encoding, algo, k, m, length))
		return NULL;

	uint8_t *out[m];
	if (!rain_encode(data, length, &encoding, &env_DEFAULT, out))
		return NULL;

	uint8_t **result = malloc(m * sizeof(uint8_t*));
	memcpy(result, out, m * sizeof(uint8_t*));
	return result;
}

int
rain_repair_and_get_raw_data(uint8_t **data, uint8_t **coding,
		size_t rawlength, unsigned int k, unsigned int m,
		const char* algo)
{
	struct rain_encoding_s enc;
	if (!encoding_prepare(&enc, algo, k, m, rawlength))
		return EXIT_FAILURE;
	int rc = rain_rehydrate(data, coding, &enc, &env_DEFAULT);
	return MACRO_COND(rc!=0,EXIT_SUCCESS,EXIT_FAILURE);
}

int
get_overhead_percentage(int raw_data_size, int k, int chunk_size,
		const char* algo)
{
	(void) algo;
	return ((k * chunk_size) * 100) / raw_data_size;
}

int
get_chunk_size(int length, int k, int m, const char* algo)
{
	struct rain_encoding_s enc;
	if (length < 0 || k < 0 || m < 0)
		return -1;
	if (!encoding_prepare(&enc, algo, k, m, length))
		return -1;
	return enc.block_size;
}

#endif
