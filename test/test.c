#include <glib.h> /* to easily compute an md5 through 'g_compute_checksum_for_string' */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

typedef int func0(int, int, int, const char*);
typedef char* func1(char**, char**, int, int, int, const char*);
typedef char** func2(char*, int, int, int, const char*);

void* librain_ptr = NULL;
func0* get_overhead_percentage = NULL;
func0* get_chunk_size = NULL;
func1* rain_repair_and_get_raw_data = NULL;
func2* rain_get_coding_chunks = NULL;

static int test_nb = 0;

int
load_librain_library()
{
	if ((librain_ptr = dlopen("../liblibrain.so", RTLD_LAZY)) == NULL) {
		printf("Test KO : Unable to load the library \"../librain.so\"\n");
		return EXIT_FAILURE;
	}

	if ((get_overhead_percentage = dlsym(librain_ptr, "get_overhead_percentage")) == NULL) {
		printf("Test KO : Unable to load the method \"get_overhead_percentage\"\n");
		return EXIT_FAILURE;
	}

	if ((get_chunk_size = dlsym(librain_ptr, "get_chunk_size")) == NULL) {
		printf("Test KO : Unable to load the method \"get_chunk_size\"\n");
		return EXIT_FAILURE;
	}

	if ((rain_repair_and_get_raw_data = dlsym(librain_ptr, "rain_repair_and_get_raw_data")) == NULL) {
		printf("Test KO : Unable to load the method \"rain_repair_and_get_raw_data\"\n");
		return EXIT_FAILURE;
	}

	if ((rain_get_coding_chunks = dlsym(librain_ptr, "rain_get_coding_chunks")) == NULL) {
        printf("Test KO : Unable to load the method \"rain_get_coding_chunks\"\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

void
unload_librain_library()
{
	if (librain_ptr)
		dlclose(librain_ptr);
}

int
foo_data_generator(char* rnd_data, int size)
{
	if (!rnd_data || size < 1)
		return EXIT_FAILURE;

	srand(time(NULL));

	int i;
	for (i = 0; i < size; i++)
		(rnd_data)[i] = (char)(rand() / (double)RAND_MAX * 255);

	return EXIT_SUCCESS;
}

int
get_next_test_params(char* line, int line_size)
{
	if (!line)
		return EXIT_FAILURE;

	FILE* fp = NULL;

	if ((fp = fopen("plan.txt", "r")) == NULL)
		return EXIT_FAILURE;

	int i = 0;
	while (i <= test_nb && fgets(line, line_size, fp) != NULL)
		i++;

	if (i != test_nb + 1) {
		if (fp)
			fclose(fp);
		return EXIT_FAILURE;
	}

	if (fp)
		fclose(fp);

	test_nb++;

	return EXIT_SUCCESS;
}

char**
get_data_chunks(char* raw_data, int size, int k, int chunk_size)
{
	if (!raw_data || k < 1 || size < 1 || chunk_size < 1)
		return NULL;

	char** ret = (char**)calloc(k, sizeof(char*));
	int i;
	for (i = 0; i < k; i++) {
		ret[i] = (char*)calloc(chunk_size, sizeof(char));

		int temp_buf = chunk_size;

		if (i * chunk_size + chunk_size > size)
			temp_buf = size - i * chunk_size;

		if (temp_buf > 0)
			memcpy(ret[i], raw_data + i * chunk_size, temp_buf);
	}

	return ret;
}

int
main (int argc, char** argv) {
	argc = argc;
	argv = argv;

	if (load_librain_library() == EXIT_FAILURE)
		return EXIT_FAILURE;

	printf("Test info : RAIN library has been successfully loaded\n");

	char test_params[256];
	while (get_next_test_params(test_params, 256) == EXIT_SUCCESS) {
		printf("***************************\n");
		printf("TEST %d :\n", test_nb);
		printf("***************************\n");
		int file_size = atoi(strtok(test_params, "|"));
		const char* algo = strtok(NULL, "|");
		int k = atoi(strtok(NULL, "|"));
		int m = atoi(strtok(NULL, "|"));
		int lost_data = atoi(strtok(NULL, "|"));
		int lost_coding = atoi(strtok(NULL, "|"));
		printf("Test file size = %d bytes\n", file_size);
		printf("Test algorythm = %s\n", algo);
		printf("k = %d\n", k);
		printf("m = %d\n", m);
		printf("Lost data chunk number = %d\n", lost_data);
		printf("Lost coding chunk number = %d\n", lost_coding);

		/* Creating the raw data */
		char* raw_data = (char*)calloc(file_size, sizeof(char));
		if (foo_data_generator(raw_data, file_size) == EXIT_FAILURE) {
			printf("Test KO : Failed to generate random raw data\n");
			return EXIT_FAILURE;
		}
		printf("Test info : Random raw data created\n");
		/* ------- */

		/* Original data MD5 calculation */
		gchar* md5 = g_compute_checksum_for_string(G_CHECKSUM_MD5, raw_data, file_size);
		printf("Test info : MD5 print generated \"%s\"\n", md5);
		/* ------- */

		/* Chunk size test */
		int chunk_size;
		if ((chunk_size = get_chunk_size(file_size, k, m, algo)) == -1) {
			printf("Test KO : \"get_chunk_size\" failed\n");
			return EXIT_FAILURE;
		}
		printf("Test OK : Chunk size of %d bytes\n", chunk_size);
		/* ------- */

		/* Overhead size test  */
		int overhead;
		if ((overhead = get_overhead_percentage(file_size, k, chunk_size, algo)) == -1) {
			printf("Test KO : \"get_overhead_percentage\" failed\n");
			return EXIT_FAILURE;
		}
		printf("Test OK : Overhead percentage of %d%%\n", overhead);
		/* ------- */

		/* Coding chunk test */
		char** coding_chunks;
		if ((coding_chunks = rain_get_coding_chunks(raw_data, file_size, k, m, algo)) == NULL) {
			printf("Test KO : Failed to generate the coding chunks\n");
			return EXIT_FAILURE;
		}
		printf("Test OK : Coding chunks generated\n");
		/* ------- */

		/* Getting raw data chunks */
		char** raw_data_chunks;
		if ((raw_data_chunks = get_data_chunks(raw_data, file_size, k, chunk_size)) == NULL) {
			printf("Test KO : Failed to get the raw data chunks\n");
			return EXIT_FAILURE;
		}
		printf("Test info : Raw data chunks got\n");
		/* ------- */

		/* Decoding test */
		int i;
		for (i = 0; i < lost_data && i < k; i++) {
			if (raw_data_chunks[i]) {
				free(raw_data_chunks[i]);
				raw_data_chunks[i] = NULL;
			}
		}
		for (i = 0; i < lost_coding && i < m; i++) {
			if (coding_chunks[i]) {
				free(coding_chunks[i]);
				coding_chunks[i] = NULL;
			}
        }

		char* reconstructed_data;
		if ((reconstructed_data = rain_repair_and_get_raw_data(raw_data_chunks, coding_chunks, file_size, k, m, algo)) == NULL) {
			printf("Test KO : Failed to get the reconstructed data\n");
			return EXIT_FAILURE;
		}

		for (i = 0; i < lost_data && i < k; i++) {
			if (!raw_data_chunks[i]) {
				printf("Test KO : Failed to reconstruct the original data chunk %d\n", i);
				return EXIT_FAILURE;
			}
		}
		printf("Test OK : Data chunks reconstructed\n");
		for (i = 0; i < lost_coding && i < m; i++) {
			if (!coding_chunks[i]) {
				printf("Test KO : Failed to reconstruct the original coding chunk %d\n", i);
                return EXIT_FAILURE;
            }
        }
		printf("Test OK : Coding chunks reconstructed\n");
		/* ------- */

		/* Reconstructed data MD5 calculation */
		gchar* reconstructed_md5 = g_compute_checksum_for_string(G_CHECKSUM_MD5, reconstructed_data, file_size);
		if (g_strcmp0(md5, reconstructed_md5)) {
			printf("Test KO : Reconstructed data MD5 \"%s\" print do not match with the original data MD5 print \"%s\"\n", reconstructed_md5, md5);

            return EXIT_FAILURE;
		}
		printf("Test OK : Reconstructed data MD5 print matches with the original data MD5 print\n");
		/* -------- */

		/* Cleaning */
		if (md5)
			g_free(md5);
		if (reconstructed_md5)
			g_free(reconstructed_md5);
		if (raw_data)
			free(raw_data);
		if (reconstructed_data)
			free(reconstructed_data);
		if (raw_data_chunks) {
			for (i = 0; i < k; i++) {
				if (raw_data_chunks[i])
					free(raw_data_chunks[i]);
			}
			free(raw_data_chunks);
		}
		if (coding_chunks) {
			for (i = 0; i < m; i++) {
				if (coding_chunks[i])
					free(coding_chunks[i]);
			}
			free(coding_chunks);
		}
		/* ------- */

		printf("-------\n");
	}

	unload_librain_library();

	return EXIT_SUCCESS;
}
