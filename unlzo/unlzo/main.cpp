#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <io.h>

#include "minilzo/minilzo.h"

int main()
{
	void *in_buf = NULL;
	size_t in_size = 0;
	size_t in_buf_size = 4096;
	size_t in_read_size;
	size_t in_ret;
	
	if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
		fprintf(stderr, "can't set mode! (stdin)\n");
		return 1;
	}
	if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
		fprintf(stderr, "can't set mode! (stdout)\n");
		return 1;
	}

	while (1) {
		in_buf = realloc(in_buf, in_buf_size);
		if (!in_buf) {
			fprintf(stderr, "malloc failed!\n");
			return 1;
		}
		in_read_size = in_buf_size - in_size;
		in_size += in_ret = fread((char *) in_buf + in_size, 1, in_read_size, stdin);
		if (feof(stdin)) break;
		if (in_ret == in_read_size) in_buf_size *= 2;
	}

	if (in_size < sizeof(lzo_uint)) {
		fprintf(stderr, "invalid input!\n");
		free(in_buf);
		return 1;
	}

	lzo_uint data_size = (*(lzo_uint *) in_buf);
	lzo_uint out_size = (data_size + data_size / 16 + 64 + 3) + sizeof(lzo_uint);
	void *out_buf = malloc(out_size);
	if (!out_buf) {
		fprintf(stderr, "out of memory!\n");
		free(in_buf);
		return 1;
	}

	lzo_init();
	lzo_uint new_size = data_size;
	int r = lzo1x_decompress_safe((unsigned char *) in_buf + sizeof(lzo_uint), in_size - sizeof(lzo_uint), (unsigned char *) out_buf, &new_size, NULL);
	if (r != LZO_E_OK || new_size != data_size) {
		fprintf(stderr, "can't decompress! err = %d\n", r);
		free(out_buf);
		free(in_buf);
		return 1;
	}

	fwrite(out_buf, 1, new_size, stdout);

	free(out_buf);
	free(in_buf);
	return 0;
}
