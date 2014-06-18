#ifndef LIBRAIN_H
#define LIBRAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#define PACKET_VALUE 1024
#define BUFFER_VALUE 4096

/**
 * Returns the percentage of original data size overhead resulting from the
 * algorithm
 */
int get_overhead_percentage(int raw_data_size, int k, int chunk_size,
		const char* algo);

/**
 * Returns the size of data and coding chunks resulting from parity
 * calculation
 */
int get_chunk_size(int raw_data_size, int k, int m, const char* algo);

/**
 * Regenerates missing data or coding chunks and returns the original data
 * (with a possible overhead of numerous '0' at its end)
 */
char* rain_repair_and_get_raw_data(char** data, char** coding,
		int raw_data_size, int k, int m, const char* algo);

/**
 * Returns an array of coding chunks resulting from the parity calculation of
 * the original file previously stripped and overheaded with '0' at its end
 */
char** rain_get_coding_chunks(char* raw_data, int raw_data_size,
		int k, int m, const char* algo);

#ifdef __cplusplus
}
#endif

#endif // LIBRAIN_H
