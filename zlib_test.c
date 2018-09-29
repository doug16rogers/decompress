#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

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
struct gzip_file_header_s {
	uint16_t magic;    /**< 0x1F 0x8B => 0x1F8B bigE == 0x8B1F lilE */
	uint8_t method;
	uint8_t flags;
	uint32_t modification_time;   /**< POSIX timestamp. */
	uint8_t extra_flags;
	uint8_t os;
} __attribute__((packed));

typedef struct gzip_file_header_s gzip_file_header_t;

//#pragma pack(struct gzip_file_header_s)

#define GZIP_METHOD_DEFLATE 8

#define GZIP_FLAG_FTEXT    (1 << 0)
#define GZIP_FLAG_FHCRC    (1 << 1)
#define GZIP_FLAG_FEXTRA   (1 << 2)
#define GZIP_FLAG_FNAME    (1 << 3)
#define GZIP_FLAG_FCOMMENT (1 << 4)

#define GZIP_DEFLATE_FLAG_MAX_COMPRESS  (1 << 1)
#define GZIP_DEFLATE_FLAG_MAX_SPEED     (1 << 2)

/* ------------------------------------------------------------------------- */
const char* zlib_return_name(int ret) {
	static char shared_text[0x100] = "";
	switch (ret) {
	case Z_OK:            return "Z_OK";
	case Z_STREAM_END:    return "Z_STREAM_END";
	case Z_NEED_DICT:     return "Z_NEED_DICT";
	case Z_ERRNO:         return "Z_ERRNO";
	case Z_STREAM_ERROR:  return "Z_STREAM_ERROR";
	case Z_DATA_ERROR:    return "Z_DATA_ERROR";
	case Z_MEM_ERROR:     return "Z_MEM_ERROR";
	case Z_BUF_ERROR:     return "Z_BUF_ERROR";
	case Z_VERSION_ERROR: return "Z_VERSION_ERROR";
	}
	snprintf(shared_text, sizeof(shared_text), "Z-UNKNOWN-ERROR(%d)", ret);
	return shared_text;
}   /* zlib_return_name() */

/* ------------------------------------------------------------------------- */
size_t gzip_to_zlib(uint8_t* data, size_t size) {
	size_t offset = sizeof(gzip_file_header_t);
	gzip_file_header_t* gzfh = NULL;

	if (size < sizeof(gzip_file_header_t))
		exit_printf(4, "too few bytes %zu < %zu for gzip header", size, sizeof(gzip_file_header_t));

	gzfh = (gzip_file_header_t*) data;
	if (gzfh->flags & GZIP_FLAG_FEXTRA) {
		if ((offset + 2) > size)
			exit_printf(4, "too few bytes %zu for gzip header extra field length", size);
		size_t extra_fields_size = *(uint16_t*) &data[offset];  /* little-endian by spec */
		if ((offset + extra_fields_size) >= size)
			exit_printf(4, "too few bytes %zu for gzip header %zu bytes of extra field data",
						size, extra_fields_size);
		offset += sizeof(uint16_t);
		offset += extra_fields_size;
	}
	if (gzfh->flags & GZIP_FLAG_FNAME) {
		size_t fname_size = strnlen(&data[offset], size - offset);
		fname_size++;    /* include terminator */
		if ((offset + fname_size) > size)
			exit_printf(4, "too few bytes %zu for gzip header %zu bytes of file name",
						size, fname_size);
		offset += fname_size;
	}
	if (gzfh->flags & GZIP_FLAG_FCOMMENT) {
		size_t comment_size = strnlen(&data[offset], size - offset);
		comment_size++;    /* include terminator */
		if ((offset + comment_size) > size)
			exit_printf(4, "too few bytes %zu for gzip header %zu bytes of comment",
						size, comment_size);
		offset += comment_size;
	}
	if (gzfh->flags & GZIP_FLAG_FHCRC) {
		if ((offset + 2) > size)
			exit_printf(4, "too few bytes %zu for gzip header %zu bytes of CRC",
						size, sizeof(uint16_t));
		offset += sizeof(uint16_t);
	}

	uint8_t cmf = 0x70 | GZIP_METHOD_DEFLATE;
	uint8_t flg = (2 << 6);     /* Default algorithm. */
	if (gzfh->extra_flags & GZIP_DEFLATE_FLAG_MAX_COMPRESS)
		flg = (3 << 6);
	else if (gzfh->extra_flags & GZIP_DEFLATE_FLAG_MAX_SPEED)
		flg = (0 << 6);
	// flg |= (1 << 5);    /* preset table present; yields Z_NEED_DICT return code */
	uint16_t sum = (((uint16_t) cmf) << 8) + flg;
	sum += 31 - (sum % 31);
	flg = sum & 0xFF;
	offset -= 2;
	data[offset + 0] = cmf;
	data[offset + 1] = flg;
	dprintf("Setting zlib header to %02X %02X.\n", cmf, flg);
	dprintf("Prepared zlib header at offset: %zu (0x%zX)\n", offset, offset);
	return offset;
}   /* gzip_to_zlib() */

/* ------------------------------------------------------------------------- */
size_t decompress_buffer(void* out, size_t out_max_size, uint8_t* in, size_t in_size) {
	size_t offset = 0;
	z_stream strm = {0};
	int result = 0;
	offset = gzip_to_zlib(in, in_size);
	result = inflateInit(&strm);
//	result = inflateInit2(&strm, 16 + MAX_WBITS);
	if (Z_OK != result)
		exit_printf(4, "inflateInit() returned %s, not Z_OK=%d, msg=\"%s\"",
					zlib_return_name(result), Z_OK, (NULL == strm.msg) ? "" : strm.msg);

	strm.next_in = (Bytef*) in + offset;
	strm.avail_in = in_size - offset - 8;   /* Remove gzip CRC and ISIZE */
	strm.next_out = out;
	strm.avail_out = out_max_size;
	result = inflate(&strm, Z_NO_FLUSH);
	if ((Z_OK != result) && (Z_STREAM_END != result))
		exit_printf(5, "inflate(Z_FINISH) returned %s, not Z_OK or Z_STREAM_END, msg=\"%s\"",
					zlib_return_name(result), Z_STREAM_END, (NULL == strm.msg) ? "" : strm.msg);
	inflateEnd(&strm);
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

	assert(10 == sizeof(gzip_file_header_t));

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
		dprintf("Read %zu bytes from \"%s\".\n", size, filename);

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
