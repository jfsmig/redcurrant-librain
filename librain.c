#include <stdlib.h>
#include <string.h>

#include <jerasure/jerasure.h>
#include <jerasure/liberation.h>
#include <jerasure/cauchy.h>
#include <jerasure/galois.h>
#include <jerasure/reed_sol.h>
#include "librain.h"

int
get_overhead_percentage(int raw_data_size, int k, int chunk_size,
		const char* algo)
{
	/* Checking input values */
	if (1 > raw_data_size || NULL == algo || 1 > k || chunk_size < 1)
		return -1;
	/* ------- */

	/* Forcing the best input parameters values */
	if (!strcmp("liber8tion", algo)) {
		if (k < 2)
			k = 2;
		else if (k > 7)
			k = 7;
	}
	else if (strcmp("crs", algo))
		return -1;
	/* ------- */

	return (((k * chunk_size) - raw_data_size) * 100) / raw_data_size;
}

static int
get_best_crs_w_value(int k, int m)
{
	if (1 > k || 1 > m || k < m)
		return -1;

	int ref = k + m - 1;
	int power = 1;
	int product = 2;

	while (product < ref) {
		product *= 2;
		power++;
	}

	return power + 1;
}

static int
get_best_packet_size_value(int raw_data_size, int k, int w)
{
	if (raw_data_size <= 0 || k <= 0)
		return -1;

	int size = raw_data_size / k;
	int packet_size = w;

	if (1 != size) {
		int parity = 0;
		if (size % 2 != 0)
			parity = 1;

		if (!parity) {
			int diviz = size;
			while (1 < (diviz /= 2))
				packet_size++;
		}
		else {
			int diviz = size;
			do {
				packet_size++;
			}
			while (1 < (diviz /= 2));
		}

		while (packet_size % w != 0)
			packet_size++;
	}

	int pow_value = 2;
	while (pow_value < packet_size)
		pow_value *= 2;

	packet_size = pow_value;

	return packet_size;
}

static int
check_and_force_input_parameters(const char* algo, int raw_data_size,
		int* k, int* m, int* w, int* packet_size, int* buffer_size)
{
    if (!strcmp("liber8tion", algo)) {
        *w = 8;
        if (*m != 2)
            *m = 2;
        if (*k < 2)
            *k = 2;
        else if (*k > 7)
            *k = 7;
    }
    else if (!strcmp("crs", algo)) {
        if (-1 == (*w = get_best_crs_w_value(*k, *m)))
            return EXIT_FAILURE;
    }
    else
        return EXIT_FAILURE;

    *packet_size = get_best_packet_size_value(raw_data_size, *k, *w);
    /**packet_size = PACKET_VALUE;*/
    *buffer_size = BUFFER_VALUE;

    return EXIT_SUCCESS;
}

static int
buffer_size_calculation(int w, int k, int packet_size, int buffer_size)
{
    if (buffer_size != 0) {
        if ((packet_size != 0) && (buffer_size % (sizeof(int) * w * k * packet_size) != 0)) {
            int up = buffer_size;
            int down = buffer_size;
            while ((up % (sizeof(int) * w * k * packet_size) != 0) && (down % (sizeof(int) * w * k * packet_size) != 0)) {
                up++;
                if (down == 0)
                    down--;
            }
            if (up % (sizeof(int) * w * k * packet_size) == 0)
                buffer_size = up;
            else {
                if (down != 0)
                    buffer_size = down;
            }
        }
        else if ((packet_size == 0) && (buffer_size % (sizeof(int) * w * k) != 0)) {
            int up = buffer_size;
            int down = buffer_size;
            while ((up % (sizeof(int) * w * k) != 0) && (down % (sizeof(int) * w * k) != 0)) {
                up++;
                down--;
            }
            if (up % (sizeof(int) * w * k) == 0)
                buffer_size = up;
            else
                buffer_size = down;
        }
    }

	return buffer_size;
}

static int
new_size_calculation(int w, int k, int packet_size, int buffer_size,
		int raw_data_size)
{
	int new_size = raw_data_size;

	if (packet_size != 0) {
		if (raw_data_size % (k * w * packet_size * sizeof(int)) != 0) {
			while (new_size % (k * w * packet_size * sizeof(int)) != 0)
				new_size++;
		}
	}
	else {
		if (raw_data_size % (k * w * sizeof(int)) != 0) {
			while (new_size % (k * w * sizeof(int)) != 0)
				new_size++;
		}
	}

	if (buffer_size != 0) {
		while (new_size % buffer_size != 0)
			new_size++;
	}

	return new_size;
}

int
get_chunk_size(int raw_data_size, int k, int m, const char* algo)
{
	/* Checking input values */
	if (1 > raw_data_size || NULL == algo || 1 > k || 1 > m || k < m)
		return -1;
	/* ------- */

	/* Forcing the best input parameters values */
	int w;
	int packet_size;
	int buffer_size;
	if (EXIT_FAILURE == check_and_force_input_parameters(algo, raw_data_size, &k, &m, &w, &packet_size, &buffer_size))
		return -1;
	/* ------- */

	/* Determining proper buffersize by finding the closest valid buffersize to the input value  */
	buffer_size = buffer_size_calculation(w, k, packet_size, buffer_size);
	/* ------- */

	/* Finding the new size by determining the next closest multiple */
	int new_size = new_size_calculation(w, k, packet_size, buffer_size, raw_data_size);
	/* ------- */

	/* Determining size of k + m files */
	int block_size = new_size / k;
	/* ------- */

	return block_size;
}

char*
rain_repair_and_get_raw_data(char** data, char** coding, int raw_data_size, int k, int m, const char* algo)
{
	/* Checking input values */
	if (NULL == data || NULL == coding || 1 > raw_data_size || NULL == algo || 1 > k || 1 > m || k < m)
		return NULL;
	/* ------- */

	/* Forcing the best input parameters values */
	int w;
	int packet_size;
	int buffer_size;
	if (EXIT_FAILURE == check_and_force_input_parameters(algo, raw_data_size, &k, &m, &w, &packet_size, &buffer_size))
		return NULL;
	/* ------- */

	/* Determining proper buffersize by finding the closest valid buffersize to the input value  */
	buffer_size = buffer_size_calculation(w, k, packet_size, buffer_size);
	/* ------- */

	/* Finding the new size by determining the next closest multiple */
	int new_size = new_size_calculation(w, k, packet_size, buffer_size, raw_data_size);
	/* ------- */

	/* Determining size of k + m files */
	int block_size = new_size / k;
	/* ------- */

	/* Allocating memory */
	int* erased = (int *)calloc(k + m, sizeof(int));
	int* erasures = (int *)calloc(k + m, sizeof(int));

	char** temp_data = (char **)calloc(k, sizeof(char *));
	char** temp_coding = (char **)calloc(m, sizeof(char *));

	int i;

	/*for (i = 0; i < k; i++)
		temp_data[i] = (char *)calloc(raw_data_size / k, sizeof(char));
	for (i = 0; i < m; i++)
		temp_coding[i] = (char *)calloc(raw_data_size / k, sizeof(char));*/
	/* ------- */

	/* Creating coding matrix or bitmatrix */
	int* bit_matrix = NULL;
	int* matrix;
	if (!strcmp("liber8tion", algo))
		bit_matrix = liber8tion_coding_bitmatrix(k);
	else if (!strcmp("crs", algo)) {
		matrix = cauchy_good_general_coding_matrix(k, m, w);
		bit_matrix = jerasure_matrix_to_bitmatrix(k, m, w, matrix);
	}
	/* ------- */

	/* Allocating memory for output file reconstruction */
	char* output_file = (char *)calloc(new_size, sizeof(char));
	/* ------- */

	/* Finding erased chunks */
	int num_erased = 0;
	for (i = 0; i < k; i++) {
		if (data[i] == NULL) {
			erased[i] = 1;
			erasures[num_erased] = i;
			num_erased++;
		}
	}

	for (i = 0; i < m; i++) {
		if (coding[i] == NULL) {
			erased[k + i] = 1;
			erasures[num_erased] = k + i;
			num_erased++;
		}
	}

	erasures[num_erased] = -1;

	if (num_erased > m) {
		if (erasures != NULL)
			free(erasures);
		if (erased != NULL)
			free(erased);

		if (temp_data != NULL) {
			for (i = 0; i < k; i++) {
				if (temp_data[i] != NULL)
					free(temp_data[i]);
			}
			free(temp_data);
		}
		if (temp_coding != NULL) {
			for (i = 0; i < m; i++) {
				if (temp_coding[i] != NULL)
					free(temp_coding[i]);
			}
			free(temp_coding);
		}
		if (output_file != NULL)
			free(output_file);

		return NULL;
	}
	/* ------- */

	/* Open files, check for erasures, read in data/coding */
	for (i = 0; i < k; i++) {
		if (erased[i] != 1) {
			temp_data[i] = (char *)calloc(block_size, sizeof(char));
			memcpy(temp_data[i], data[i], block_size);
		}
	}
	for (i = 0; i < m; i++) {
		if (erased[k + i] != 1) {
			temp_coding[i] = (char *)calloc(block_size, sizeof(char));
			memcpy(temp_coding[i], coding[i], block_size);
		}
	}
	/* ------- */

	/* Finish allocating data/coding if needed */
	for (i = 0; i < num_erased; i++) {
		if (erasures[i] < k) {
			if (temp_data[erasures[i]])
				free(temp_data[erasures[i]]);
			temp_data[erasures[i]] = (char *)calloc(block_size, sizeof(char));
		}
		else {
			if (temp_coding[erasures[i] - k])
				free(temp_coding[erasures[i] - k]);
			temp_coding[erasures[i] - k] = (char *)calloc(block_size, sizeof(char));
		}
	}
	/* ------- */

	/* Choose proper decoding method */
	jerasure_schedule_decode_lazy(k, m, w, bit_matrix, erasures, temp_data, temp_coding, block_size, packet_size, 1);
	/* ------- */

	/* Create decoded data */
	for (i = 0; i < k; i++) {
		if (erased[i] == 1) {
			if (data[i] == NULL)
				data[i] = (char *)calloc(block_size, sizeof(char));
			memcpy(data[i], temp_data[i], block_size);
		}

		memcpy(output_file + i * block_size, temp_data[i], block_size);
	}

	for (i = 0; i < m; i++) {
		if (erased[k + i] == 1) {
			if (coding[i] == NULL)
				coding[i] = (char *)calloc(block_size, sizeof(char));
			memcpy(coding[i], temp_coding[i], block_size);
		}
	}
	/* ------- */

	/* Freeing previously allocated memory */
	if (erasures != NULL)
		free(erasures);
	if (erased != NULL)
		free(erased);
	if (temp_data != NULL) {
		for (i = 0; i < k; i++) {
			if (temp_data[i] != NULL)
				free(temp_data[i]);
		}
		free(temp_data);
	}
	if (temp_coding != NULL) {
		for (i = 0; i < m; i++) {
			if (temp_coding[i] != NULL)
				free(temp_coding[i]);
		}
		free(temp_coding);
	}
	if (bit_matrix)
		free(bit_matrix);
	/* ------- */

	return output_file;
}

char**
rain_get_coding_chunks(char* raw_data, int raw_data_size, int k, int m, const char* algo)
{
	/* Checking input values */
	if (NULL == raw_data || 1 > raw_data_size || NULL == algo || 1 > k || 1 > m || k < m)
		return NULL;
	/* ------- */

	/* Forcing the best input parameters values */
	int w;
	int packet_size;
	int buffer_size;
	if (EXIT_FAILURE == check_and_force_input_parameters(algo, raw_data_size, &k, &m, &w, &packet_size, &buffer_size))
		return NULL;
	/* ------- */

	/* Determining proper buffersize by finding the closest valid buffersize to the input value  */
	buffer_size = buffer_size_calculation(w, k, packet_size, buffer_size);
	/* ------- */

	/* Finding the new size by determining the next closest multiple */
    int new_size = new_size_calculation(w, k, packet_size, buffer_size, raw_data_size);
    /* ------- */

	/* Determining size of k + m files */
	int block_size = new_size / k;
	/* ------- */

	/* Allocating memory for buffer */
	char* block = (char *)calloc(new_size, sizeof(char));
	/* ------- */

	/* Allocating memory for data and coding */
	char** data = (char **)calloc(k, sizeof(char*));
	char** coding = (char **)calloc(m, sizeof(char*));
	int i;
	for (i = 0; i < m; i++)
		coding[i] = (char *)calloc(block_size, sizeof(char));
	/* ------- */

	/* Creating coding matrix or bitmatrix and schedule */
	int* bit_matrix = NULL;
	int* matrix = NULL;
	int** schedule = NULL;
	if (!strcmp("liber8tion", algo)) {
		bit_matrix = liber8tion_coding_bitmatrix(k);
		schedule = jerasure_smart_bitmatrix_to_schedule(k, m, w, bit_matrix);
	}
	else if (!strcmp("crs", algo)) {
		matrix = cauchy_good_general_coding_matrix(k, m, w);
		bit_matrix = jerasure_matrix_to_bitmatrix(k, m, w, matrix);
		schedule = jerasure_smart_bitmatrix_to_schedule(k, m, w, bit_matrix);
	}
	/* ------- */

	/* Allocating final results */
	char** final_data = (char **)calloc(k, sizeof(char*));
	for (i = 0; i < k; i++)
		final_data[i] = (char *)calloc(block_size, sizeof(char));
	char** final_coding = (char **)calloc(m, sizeof(char*));
	for (i = 0; i < m; i++)
		final_coding[i] = (char *)calloc(block_size, sizeof(char));
	/* ------- */

	/* Checking if padding is needed */
	for (i = 0; i < k; i++) {
		int temp_block_size = block_size;
		if (i * block_size + block_size > raw_data_size)
			temp_block_size = raw_data_size - i * block_size;

		if (temp_block_size > 0)
			memcpy(block + (i * block_size), raw_data + i * block_size, temp_block_size);
	}
	/* ------- */

	/* Setting pointers to point to data */
	for (i = 0; i < k; i++)
		data[i] = block + (i * block_size);
	/* ------- */

	/* Encoding */
	jerasure_schedule_encode(k, m, w, schedule, data, coding, block_size, packet_size);
	/* ------- */

	/* Writing data and encoded data to k + m memory slots */
	for (i = 0; i < k; i++)
		memcpy(final_data[i], data[i], block_size);
	for (i = 0; i < m; i++)
		memcpy(final_coding[i], coding[i], block_size);
	/* ------- */

	/* Freeing previously allocated memory */
	if (block != NULL)
		free(block);
	if (data != NULL)
		free(data);
	if (coding != NULL) {
		for (i = 0; i < m; i++) {
			if (coding[i] != NULL)
				free(coding[i]);
		}
		free(coding);
	}
	if (final_data != NULL) {
		for (i = 0; i < k; i++) {
			if (final_data[i] != NULL)
				free(final_data[i]);
		}
		free(final_data);
	}
	if (schedule)
		jerasure_free_schedule(schedule);
	if (bit_matrix)
		free(bit_matrix);
	/* ------- */

	return final_coding;
}
