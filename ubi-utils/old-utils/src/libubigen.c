/*
 * Copyright (c) International Business Machines Corp., 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Oliver Lohmann
 *
 * Add UBI headers to binary data.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <mtd/ubi-media.h>
#include <mtd_swab.h>

#include "config.h"
#include "ubigen.h"
#include "crc32.h"

#define UBI_NAME_SIZE		256
#define DEFAULT_VID_OFFSET	((DEFAULT_PAGESIZE) - (UBI_VID_HDR_SIZE))
#define MIN(a,b)		((a) < (b) ? (a) : (b))

static uint32_t crc32_table[256];

struct ubi_info {
	struct ubi_vid_hdr* v;	/* Volume ID header */
	struct ubi_ec_hdr* ec;	/* Erase count header */

	FILE* fp_in;		/* Input Stream */
	FILE* fp_out;		/* Output stream */

	size_t eb_size;		/* Physical EB size in bytes */
	size_t leb_size;	/* Size of a logical EB in a physical EB */
	size_t leb_total;	/* Total input size in logical EB */
	size_t alignment;	/* Block alignment */
	size_t data_pad;	/* Size of padding in each physical EB */

	size_t bytes_total;	/* Total input size in bytes */
	size_t bytes_read;	/* Nymber of read bytes (total) */

	uint32_t blks_written;	/* Number of written logical EB */

	uint8_t* buf;		/* Allocated buffer */
	uint8_t* ptr_ec_hdr;	/* Pointer to EC hdr in buf */
	uint8_t* ptr_vid_hdr;	/* Pointer to VID hdr in buf */
	uint8_t* ptr_data;	/* Pointer to data region in buf */
};


static uint32_t
byte_to_blk(uint64_t byte, uint32_t eb_size)
{
	return (byte % eb_size) == 0
		? (byte / eb_size)
		: (byte / eb_size) + 1;
}

static int
validate_ubi_info(ubi_info_t u)
{
	if ((u->v->vol_type != UBI_VID_DYNAMIC) &&
	    (u->v->vol_type != UBI_VID_STATIC)) {
		return EUBIGEN_INVALID_TYPE;
	}

	if (be32_to_cpu(u->ec->vid_hdr_offset) < UBI_VID_HDR_SIZE) {
		return EUBIGEN_INVALID_HDR_OFFSET;
	}

	return 0;
}

static int
skip_blks(ubi_info_t u, uint32_t blks)
{
	uint32_t i;
	size_t read = 0, to_read = 0;

	/* Step to a maximum of leb_total - 1 to keep the
	   restrictions. */
	for (i = 0; i < MIN(blks, u->leb_total-1); i++) {
		/* Read in data */
		to_read = MIN(u->leb_size,
			      (u->bytes_total - u->bytes_read));
		read = fread(u->ptr_data, 1, to_read, u->fp_in);
		if (read != to_read) {
			return -EIO;
		}
		u->bytes_read += read;
		u->blks_written++;
	}

	return 0;
}

static void
clear_buf(ubi_info_t u)
{
	memset(u->buf, 0xff, u->eb_size);
}

static void
write_ec_hdr(ubi_info_t u)
{
	memcpy(u->ptr_ec_hdr, u->ec, UBI_EC_HDR_SIZE);
}

static int
fill_data_buffer_from_file(ubi_info_t u, size_t* read)
{
	size_t to_read = 0;

	if (u-> fp_in == NULL)
		return -EIO;

	to_read = MIN(u->leb_size, (u->bytes_total - u->bytes_read));
	*read = fread(u->ptr_data, 1, to_read, u->fp_in);
	if (*read != to_read) {
		return -EIO;
	}
	return 0;
}

static void
add_static_info(ubi_info_t u, size_t data_size, ubigen_action_t action)
{
	uint32_t crc = clc_crc32(crc32_table, UBI_CRC32_INIT,
				 u->ptr_data, data_size);

	u->v->data_size = cpu_to_be32(data_size);
	u->v->data_crc = cpu_to_be32(crc);

	if (action & BROKEN_DATA_CRC) {
		u->v->data_crc =
			cpu_to_be32(be32_to_cpu(u->v->data_crc) + 1);
	}
	if (action & BROKEN_DATA_SIZE) {
		u->v->data_size =
			cpu_to_be32(be32_to_cpu(u->v->data_size) + 1);
	}
}

static void
write_vid_hdr(ubi_info_t u, ubigen_action_t action)
{
	uint32_t crc = clc_crc32(crc32_table, UBI_CRC32_INIT,
				 u->v, UBI_VID_HDR_SIZE_CRC);
	/* Write VID header */
	u->v->hdr_crc = cpu_to_be32(crc);
	if (action & BROKEN_HDR_CRC) {
		u->v->hdr_crc = cpu_to_be32(be32_to_cpu(u->v->hdr_crc) + 1);
	}
	memcpy(u->ptr_vid_hdr, u->v, UBI_VID_HDR_SIZE);
}

static int
write_to_output_stream(ubi_info_t u)
{
	size_t written;

	written = fwrite(u->buf, 1, u->eb_size, u->fp_out);
	if (written != u->eb_size) {
		return -EIO;
	}
	return 0;
}

int
ubigen_write_leb(ubi_info_t u, ubigen_action_t action)
{
	int rc = 0;
	size_t read = 0;

	clear_buf(u);
	write_ec_hdr(u);

	rc = fill_data_buffer_from_file(u, &read);
	if (rc != 0)
		return rc;

	if (u->v->vol_type == UBI_VID_STATIC)  {
		add_static_info(u, read, action);
	}

	u->v->lnum = cpu_to_be32(u->blks_written);

	if (action & MARK_AS_UPDATE) {
		u->v->copy_flag = (u->v->copy_flag)++;
	}

	write_vid_hdr(u, action);
	rc = write_to_output_stream(u);
	if (rc != 0)
		return rc;

	/* Update current handle */
	u->bytes_read += read;
	u->blks_written++;
	return 0;
}

int
ubigen_write_complete(ubi_info_t u)
{
	size_t i;
	int rc = 0;

	for (i = 0; i < u->leb_total; i++) {
		rc = ubigen_write_leb(u,  NO_ERROR);
		if (rc != 0)
			return rc;
	}

	return 0;
}

int
ubigen_write_broken_update(ubi_info_t u, uint32_t blk)
{
	int rc = 0;

	rc = skip_blks(u, blk);
	if (rc != 0)
		return rc;

	rc = ubigen_write_leb(u, MARK_AS_UPDATE | BROKEN_DATA_CRC);
	if (rc != 0)
		return rc;


	return 0;
}

void
dump_info(ubi_info_t u ubi_unused)
{
#ifdef DEBUG
	int err = 0;
	if (!u) {
		fprintf(stderr, "<empty>");
		return;
	}
	if (!u->ec) {
		fprintf(stderr, "<ec-empty>");
		err = 1;
	}
	if (!u->v) {
		fprintf(stderr, "<v-empty>");
		err = 1;
	}
	if (err) return;

	fprintf(stderr, "ubi volume\n");
	fprintf(stderr, "version      :	  %8d\n", u->v->version);
	fprintf(stderr, "vol_id	      :	  %8d\n", be32_to_cpu(u->v->vol_id));
	fprintf(stderr, "vol_type     :	  %8s\n",
		u->v->vol_type == UBI_VID_STATIC ?
		"static" : "dynamic");
	fprintf(stderr, "used_ebs     :	  %8d\n",
		be32_to_cpu(u->v->used_ebs));
	fprintf(stderr, "eb_size      : 0x%08x\n", u->eb_size);
	fprintf(stderr, "leb_size     : 0x%08x\n", u->leb_size);
	fprintf(stderr, "data_pad     : 0x%08x\n",
		be32_to_cpu(u->v->data_pad));
	fprintf(stderr, "leb_total    :	  %8d\n", u->leb_total);
	fprintf(stderr, "header offs  : 0x%08x\n",
		be32_to_cpu(u->ec->vid_hdr_offset));
	fprintf(stderr, "bytes_total  :	  %8d\n", u->bytes_total);
	fprintf(stderr, "  +  in MiB  : %8.2f M\n",
		((float)(u->bytes_total)) / 1024 / 1024);
	fprintf(stderr, "-------------------------------\n\n");
#else
	return;
#endif
}

int
ubigen_destroy(ubi_info_t *u)
{
	if (u == NULL)
		return -EINVAL;

	ubi_info_t tmp = *u;

	if (tmp) {
		if (tmp->v)
			free(tmp->v);
		if (tmp->ec)
			free(tmp->ec);
		if (tmp->buf)
			free(tmp->buf);
		free(tmp);
	}
	*u = NULL;
	return 0;
}

void
ubigen_init(void)
{
	init_crc32_table(crc32_table);
}

int
ubigen_create(ubi_info_t* u, uint32_t vol_id, uint8_t vol_type,
	      uint32_t eb_size, uint64_t ec, uint32_t alignment,
	      uint8_t version, uint32_t vid_hdr_offset, uint8_t compat_flag,
	      size_t data_size, FILE* fp_in, FILE* fp_out)
{
	int rc = 0;
	ubi_info_t res = NULL;
	uint32_t crc;
	uint32_t data_offset;

	if (alignment == 0) {
		rc = EUBIGEN_INVALID_ALIGNMENT;
		goto ubigen_create_err;
	}
	if ((fp_in == NULL) || (fp_out == NULL)) {
		rc = -EINVAL;
		goto ubigen_create_err;
	}

	res = (ubi_info_t) calloc(1, sizeof(struct ubi_info));
	if (res == NULL) {
		rc = -ENOMEM;
		goto ubigen_create_err;
	}

	res->v = (struct ubi_vid_hdr*) calloc(1, sizeof(struct ubi_vid_hdr));
	if (res->v == NULL) {
		rc = -ENOMEM;
		goto ubigen_create_err;
	}

	res->ec = (struct ubi_ec_hdr*) calloc(1, sizeof(struct ubi_ec_hdr));
	if (res->ec == NULL) {
		rc = -ENOMEM;
		goto ubigen_create_err;
	}

	/* data which is needed in the general process */
	vid_hdr_offset = vid_hdr_offset ? vid_hdr_offset : DEFAULT_VID_OFFSET;
	data_offset = vid_hdr_offset + UBI_VID_HDR_SIZE;
	res->bytes_total = data_size;
	res->eb_size = eb_size ? eb_size : DEFAULT_BLOCKSIZE;
	res->data_pad = (res->eb_size - data_offset) % alignment;
	res->leb_size = res->eb_size - data_offset - res->data_pad;
	res->leb_total = byte_to_blk(data_size, res->leb_size);
	res->alignment = alignment;

	if ((res->eb_size < (vid_hdr_offset + UBI_VID_HDR_SIZE))) {
		rc = EUBIGEN_TOO_SMALL_EB;
		goto ubigen_create_err;
	}
	res->fp_in = fp_in;
	res->fp_out = fp_out;

	/* vid hdr data which doesn't change */
	res->v->magic = cpu_to_be32(UBI_VID_HDR_MAGIC);
	res->v->version = version ? version : UBI_VERSION;
	res->v->vol_type = vol_type;
	res->v->vol_id = cpu_to_be32(vol_id);
	res->v->compat = compat_flag;
	res->v->data_pad = cpu_to_be32(res->data_pad);

	/* static only: used_ebs */
	if (res->v->vol_type == UBI_VID_STATIC) {
		res->v->used_ebs = cpu_to_be32(byte_to_blk
						(res->bytes_total,
						 res->leb_size));
	}

	/* ec hdr (fixed, doesn't change) */
	res->ec->magic = cpu_to_be32(UBI_EC_HDR_MAGIC);
	res->ec->version = version ? version : UBI_VERSION;
	res->ec->ec = cpu_to_be64(ec);
	res->ec->vid_hdr_offset = cpu_to_be32(vid_hdr_offset);

	res->ec->data_offset = cpu_to_be32(data_offset);

	crc = clc_crc32(crc32_table, UBI_CRC32_INIT, res->ec,
			UBI_EC_HDR_SIZE_CRC);
	res->ec->hdr_crc = cpu_to_be32(crc);

	/* prepare a read buffer */
	res->buf = (uint8_t*) malloc (res->eb_size * sizeof(uint8_t));
	if (res->buf == NULL) {
		rc = -ENOMEM;
		goto ubigen_create_err;
	}

	/* point to distinct regions within the buffer */
	res->ptr_ec_hdr = res->buf;
	res->ptr_vid_hdr = res->buf + be32_to_cpu(res->ec->vid_hdr_offset);
	res->ptr_data = res->buf + be32_to_cpu(res->ec->vid_hdr_offset)
		+ UBI_VID_HDR_SIZE;

	rc = validate_ubi_info(res);
	if (rc != 0) {
		fprintf(stderr, "Volume validation failed: %d\n", rc);
		goto ubigen_create_err;
	}

	dump_info(res);
	*u = res;
	return rc;

 ubigen_create_err:
	if (res) {
		if (res->v)
			free(res->v);
		if (res->ec)
			free(res->ec);
		if (res->buf)
			free(res->buf);
		free(res);
	}
	*u = NULL;
	return rc;
}

int
ubigen_get_leb_size(ubi_info_t u, size_t* size)
{
	if (u == NULL)
		return -EINVAL;

	*size = u->leb_size;
	return 0;
}


int
ubigen_get_leb_total(ubi_info_t u, size_t* total)
{
	if (u == NULL)
		return -EINVAL;

	*total = u->leb_total;
	return 0;
}

int
ubigen_set_lvol_rec(ubi_info_t u, size_t reserved_bytes,
		    const char* vol_name, struct ubi_vtbl_record *lvol_rec)
{
	uint32_t crc;

	if ((u == NULL) || (vol_name == NULL))
		return -EINVAL;

	memset(lvol_rec, 0x0, UBI_VTBL_RECORD_SIZE);

	lvol_rec->reserved_pebs =
		cpu_to_be32(byte_to_blk(reserved_bytes, u->leb_size));
	lvol_rec->alignment = cpu_to_be32(u->alignment);
	lvol_rec->data_pad = u->v->data_pad;
	lvol_rec->vol_type = u->v->vol_type;

	lvol_rec->name_len =
		cpu_to_be16((uint16_t)strlen((const char*)vol_name));

	memcpy(lvol_rec->name, vol_name, UBI_VOL_NAME_MAX + 1);

	crc = clc_crc32(crc32_table, UBI_CRC32_INIT,
			lvol_rec, UBI_VTBL_RECORD_SIZE_CRC);
	lvol_rec->crc =	 cpu_to_be32(crc);

	return 0;
}
