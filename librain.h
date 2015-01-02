#ifndef LIBRAIN_H
#define LIBRAIN_H 1

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long size_t;
typedef signed long ssize_t;
typedef unsigned char uint8_t;

enum rain_algorithm_e {
	JALG_unset = 0,
	JALG_liberation,
	JALG_crs
};

struct rain_env_s
{
	void* (*malloc) (size_t size);
	void* (*calloc) (size_t nmemb, size_t size);
	void (*free) (void *ptr);
};

/** Encoding parameters */
struct rain_encoding_s
{
	size_t packet_size;
	size_t block_size;
	unsigned int w, k, m;
	enum rain_algorithm_e algo;

	// for debug purpose
	size_t data_size;
	size_t padded_data_size;
	size_t strip_size;
};


int rain_get_encoding (struct rain_encoding_s *encoding, size_t rawlength,
		unsigned int k, unsigned int m, const char *algo);

/** Fills 'out' with an array of coding chunks resulting from the parity
 * computation of the original file previously stripped and overheaded with
 * '0' at its end.
 * @param rawdata cannot be NULL and must be 'rawlength' long. No padding expected.
 * @param rawlength must be > 0
 * @param enc cannot be NULL
 * @param env can be NULL
 * @param out must have at least enc->m slots
 * @return a boolean value, false if it failed
 */
int rain_encode (uint8_t *rawdata, size_t rawlength,
		struct rain_encoding_s *enc, struct rain_env_s *env,
		uint8_t **out);

/** Regenerates missing data or coding chunks and returns the original data
 * (with a possible overhead of numerous '0' at its end).
 * @param data is expected to have at least enc->k slots
 * @param coding is expected to have at least enc->m slots
 * @param enc cannot be NULL
 * @param env can be NULL
 * @return a boolean value, false if it failed
 */
int rain_rehydrate(uint8_t **data, uint8_t **coding,
		struct rain_encoding_s *enc, struct rain_env_s *env);

#ifndef HAVE_NOLEGACY
/* Legacy interface */

int rain_repair_and_get_raw_data (uint8_t **data, uint8_t **coding,
		size_t raw_data_size, unsigned int k, unsigned int m,
		const char* algo);

uint8_t** rain_get_coding_chunks (uint8_t* raw_data, size_t raw_data_size,
		unsigned int k, unsigned int m,
		const char* algo);

int get_chunk_size(int raw_data_size, int k, int m, const char* algo);

// TODO FIXME Why is here no 'm' ?
int get_overhead_percentage(int raw_data_size, int k, int chunk_size,
		const char* algo);

#endif

#ifdef __cplusplus
}
#endif

#endif // LIBRAIN_H
