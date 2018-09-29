#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lzma.h>

const char* g_program = NULL;

#if 0
#define dprintf(...) printf(__VA_ARGS__)
#else
#define dprintf(...) do { } while (0)
#endif

/* ------------------------------------------------------------------------- */
#define exit_printf(_exit_code, ...)			\
	do {										\
		fprintf(stderr, "%s: ", g_program);     \
		fprintf(stderr, __VA_ARGS__);			\
		fprintf(stderr, "\n");					\
		exit(_exit_code);						\
	} while (0)

/* ------------------------------------------------------------------------- */
uint8_t* file_contents(const char* filename, size_t* filesize) {
	FILE* f = fopen(filename, "rb");
	if (NULL == f)
		exit_printf(2, "could not open \"%s\"", filename);
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t* data = malloc(size);
	if (NULL == data)
		exit_printf(3, "could not allocate %zu bytes for file contents of \"%s\"", size, filename);
	size_t bytes_read = 0;
	while (bytes_read < size) {
		size_t bytes = fread(&data[bytes_read], 1, size - bytes_read, f);
		if (bytes == 0)
			break;
		bytes_read += bytes;
	}
	fclose(f);
	*filesize = size;
	return data;
}   /* file_contents() */

/* ------------------------------------------------------------------------- */
size_t decompress_buffer(void* out, size_t out_max_size, const void* in, size_t in_size) {
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret result = LZMA_OK;
	strm.next_in = in;
	strm.avail_in = in_size;
	strm.next_out = out;
	strm.avail_out = out_max_size;
	result = lzma_auto_decoder(&strm, 0x10 * out_max_size, 0);
	result = lzma_code(&strm, LZMA_RUN);
	if (LZMA_STREAM_END != result)
		exit_printf(5, "lzma_code(LZMA_RUN) returned %d, not LZMA_STREAM_END=%d\n", result, LZMA_STREAM_END);
	lzma_end(&strm);
	return strm.total_out;
}   /* decompress_buffer() */

/* ------------------------------------------------------------------------- */
int main(int argc, char* argv[]) {
	int i = 0;
	size_t size = 0;
	uint8_t* data = NULL;
	const size_t MAX_OUT = 0x01000000;
	uint8_t* out = NULL;
	const uint8_t ELF_MAGIC[] = { 0x7F, 0x45, 0x4C, 0x46 }; /*, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; */

	g_program = argv[0];

	if (argc < 2)
		exit_printf(1, "usage: %s <file-to-decompress>", g_program);

	out = malloc(MAX_OUT);
	if (NULL == out)
		exit_printf(3, "could not allocate %zu bytes for output", MAX_OUT);

	for (i = 1; i < argc; ++i) {
		const char* filename = argv[i];
		size_t out_size = 0;
		data = file_contents(filename, &size);
		dprintf("read %zu bytes from \"%s\".\n", size, filename);
		out_size = decompress_buffer(out, MAX_OUT, data, size);

		dprintf("Decompressed from %zu to %llu bytes.\n", size, (long long) out_size);
		dprintf("First 8 bytes of output: %02X %02X %02X %02X %02X %02X %02X %02X\n",
		        (out_size > 0) ? out[0] : 0,
		        (out_size > 1) ? out[1] : 0,
		        (out_size > 2) ? out[2] : 0,
		        (out_size > 3) ? out[3] : 0,
		        (out_size > 4) ? out[4] : 0,
		        (out_size > 5) ? out[5] : 0,
		        (out_size > 6) ? out[6] : 0,
		        (out_size > 7) ? out[7] : 0);
		if ((out_size >= sizeof(ELF_MAGIC)) &&
			(0 == memcmp(out, ELF_MAGIC, sizeof(ELF_MAGIC)))) {
			printf("ELF inside: %s\n", filename);
		} else {
			printf("Is not ELF: %s\n", filename);
		}
		free(data);
	}

	free(out);
	return 0;
}   /* main() */
