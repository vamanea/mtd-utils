/*
 * Copyright (C) 2008 Nokia Corporation.
 * Copyright (C) 2008 University of Szeged, Hungary
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Artem Bityutskiy
 *          Adrian Hunter
 *          Zoltan Sogor
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>
#include <lzo/lzo1x.h>
#include <linux/types.h>

#include "compr.h"
#include "ubifs-media.h"

static void *lzo_mem;
static unsigned long long errcnt = 0;

#define DEFLATE_DEF_LEVEL     Z_DEFAULT_COMPRESSION
#define DEFLATE_DEF_WINBITS   11
#define DEFLATE_DEF_MEMLEVEL  8

static int zlib_deflate(void *in_buf, size_t in_len, void *out_buf,
			size_t *out_len)
{
	z_stream strm;

	strm.zalloc = NULL;
	strm.zfree = NULL;

	/*
	 * Match exactly the zlib parameters used by the linux kernel crypto
	 * API.
	 */
        if (deflateInit2(&strm, DEFLATE_DEF_LEVEL, Z_DEFLATED,
			 -DEFLATE_DEF_WINBITS, DEFLATE_DEF_MEMLEVEL,
			 Z_DEFAULT_STRATEGY)) {
		errcnt += 1;
		return -1;
	}

	strm.next_in = in_buf;
	strm.avail_in = in_len;
	strm.total_in = 0;

	strm.next_out = out_buf;
	strm.avail_out = *out_len;
	strm.total_out = 0;

	if (deflate(&strm, Z_FINISH) != Z_STREAM_END) {
		deflateEnd(&strm);
		errcnt += 1;
		return -1;
	}

	if (deflateEnd(&strm) != Z_OK) {
		errcnt += 1;
		return -1;
	}

	*out_len = strm.total_out;

	return 0;
}

static int lzo_compress(void *in_buf, size_t in_len, void *out_buf,
			size_t *out_len)
{
	lzo_uint len;
	int ret;

	len = *out_len;
	ret = lzo1x_999_compress(in_buf, in_len, out_buf, &len, lzo_mem);
	*out_len = len;

	if (ret != LZO_E_OK) {
		errcnt += 1;
		return -1;
	}

	return 0;
}

static int no_compress(void *in_buf, size_t in_len, void *out_buf,
		       size_t *out_len)
{
	memcpy(out_buf, in_buf, in_len);
	*out_len = in_len;
	return 0;
}

int compress_data(void *in_buf, size_t in_len, void *out_buf, size_t *out_len,
		  int type)
{
	int ret = 1;

	if (in_len < UBIFS_MIN_COMPR_LEN) {
		no_compress(in_buf, in_len, out_buf, out_len);
		return MKFS_UBIFS_COMPR_NONE;
	}

	switch (type) {
	case MKFS_UBIFS_COMPR_LZO:
		ret = lzo_compress(in_buf, in_len, out_buf, out_len);
		break;
	case MKFS_UBIFS_COMPR_ZLIB:
		ret = zlib_deflate(in_buf, in_len, out_buf, out_len);
		break;
	case MKFS_UBIFS_COMPR_NONE:
		ret = 1;
		break;
	default:
		errcnt += 1;
		break;
	}
	if (ret || *out_len >= in_len) {
		no_compress(in_buf, in_len, out_buf, out_len);
		return MKFS_UBIFS_COMPR_NONE;
	}
	return type;
}

int init_compression(void)
{
	lzo_mem = malloc(LZO1X_999_MEM_COMPRESS);
	if (!lzo_mem)
		return -1;
	return 0;
}

void destroy_compression(void)
{
	free(lzo_mem);
	if (errcnt)
		fprintf(stderr, "%llu compression errors occurred\n", errcnt);
}
