#include <stdlib.h>
#include "minilzo/minilzo.h"

#define HEAP_ALLOC(var,size) \
    lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]
static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

size_t compress(void **dest, void *src, size_t in_len)
{
	static int init_flag = 0;
	if (!init_flag) { lzo_init(); init_flag = 1; }
	*dest = realloc(*dest, (in_len + in_len / 16 + 64 + 3) + sizeof(lzo_uint));
	lzo_uint out_len;
	lzo1x_1_compress((unsigned char *) src, in_len, (unsigned char *) *dest + sizeof(lzo_uint), &out_len, wrkmem);
	**(lzo_uint **) dest = in_len;
	return out_len + sizeof(lzo_uint);
}