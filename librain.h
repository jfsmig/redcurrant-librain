#ifndef LIBRAIN_H
#define LIBRAIN_H
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long size_t;
typedef signed long ssize_t;
typedef unsigned char uint8_t;

/**
 * Returns the percentage of original data size overhead resulting from the
 * algorithm
 */
int get_overhead_percentage (size_t raw_data_size, unsigned int k,
		size_t chunk_size, const char* algo);

/**
 * Returns the size of data and coding chunks resulting from parity
 * calculation
 */
ssize_t get_chunk_size (size_t raw_data_size, unsigned int k, unsigned int m,
		const char* algo);

/**
 * Regenerates missing data or coding chunks and returns the original data
 * (with a possible overhead of numerous '0' at its end)
 */
int rain_repair_and_get_raw_data (uint8_t **data, uint8_t **coding,
		size_t raw_data_size, unsigned int k, unsigned int m,
		const char* algo);

/**
 * Returns an array of coding chunks resulting from the parity calculation of
 * the original file previously stripped and overheaded with '0' at its end
 */
uint8_t** rain_get_coding_chunks (uint8_t* raw_data, size_t raw_data_size,
		unsigned int k, unsigned int m,
		const char* algo);

#ifdef __cplusplus
}
#endif

#endif // LIBRAIN_H
