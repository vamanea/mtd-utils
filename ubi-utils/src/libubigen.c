/*
 * Copyright (c) International Business Machines Corp., 2006
 * Copyright (C) 2008 Nokia Corporation
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
 */

/*
 * Generating UBI images.
 *
 * Authors: Oliver Lohmann
 *          Artem Bityutskiy
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <mtd/ubi-header.h>
#include <libubigen.h>
#include "crc32.h"
#include "common.h"

#define PROGRAM_NAME "libubigen"

/**
 * ubigen_info_init - initialize libubigen.
 * @ui: libubigen information
 * @peb_size: flash physical eraseblock size
 * @min_io_size: flash minimum input/output unit size
 * @subpage_size: flash sub-page, if present (has to be equivalent to
 *                @min_io_size if does not exist)
 * @vid_hdr_offs: offset of the VID header
 * @ubi_ver: UBI version
 * @ec: initial erase counter
 */
void ubigen_info_init(struct ubigen_info *ui, int peb_size, int min_io_size,
		      int subpage_size, int vid_hdr_offs, int ubi_ver,
		      long long ec)
{
	if (!vid_hdr_offs)
		vid_hdr_offs = subpage_size;

	ui->peb_size = peb_size;
	ui->min_io_size = min_io_size;
	ui->vid_hdr_offs = vid_hdr_offs;
	ui->data_offs = vid_hdr_offs + UBI_VID_HDR_SIZE + min_io_size - 1;
	ui->data_offs /= min_io_size;
	ui->data_offs *= min_io_size;
	ui->leb_size = peb_size - ui->data_offs;
	ui->ubi_ver = ubi_ver;
	ui->ec = ec;

	ui->vtbl_size = ui->leb_size;
	if (ui->vtbl_size > UBI_MAX_VOLUMES * UBI_VTBL_RECORD_SIZE)
		ui->vtbl_size = UBI_MAX_VOLUMES * UBI_VTBL_RECORD_SIZE;
	ui->max_volumes = ui->vtbl_size / UBI_VTBL_RECORD_SIZE;
}

/**
 * ubigen_create_empty_vtbl - creates empty volume table.
 *
 * This function creates an empty volume table and returns a pointer to it in
 * case of success and %NULL in case of failure. The returned object has to be
 * freed with 'free()' call.
 */
struct ubi_vtbl_record *ubigen_create_empty_vtbl(const struct ubigen_info *ui)
{
	struct ubi_vtbl_record *vtbl;
	int i;

	vtbl = calloc(1, ui->vtbl_size);
	if (!vtbl) {
		errmsg("cannot allocate %d bytes of memory", ui->vtbl_size);
		return NULL;
	}

	for (i = 0; i < UBI_MAX_VOLUMES; i++) {
		uint32_t crc = crc32(UBI_CRC32_INIT, &vtbl[i],
				     UBI_VTBL_RECORD_SIZE_CRC);
		vtbl[i].crc = __cpu_to_be32(crc);
	}

	return vtbl;
}

/**
 * ubigen_add_volume - add a volume to the volume table.
 * @ui: libubigen information
 * @vi: volume information
 * @vtbl: volume table to add to
 *
 * This function adds volume described by input parameters to the volume table
 * @vtbl.
 */
int ubigen_add_volume(const struct ubigen_info *ui,
		       const struct ubigen_vol_info *vi,
		       struct ubi_vtbl_record *vtbl)
{
	struct ubi_vtbl_record *vtbl_rec = &vtbl[vi->id];
	uint32_t tmp;

	if (vi->id >= ui->max_volumes) {
		errmsg("too high volume id %d, max. volumes is %d",
		       vi->id, ui->max_volumes);
		return -1;
	}
	if (vi->alignment >= ui->leb_size) {
		errmsg("too large alignment %d, max is %d (LEB size)",
		       vi->alignment, ui->leb_size);
		return -1;
	}

	memset(vtbl_rec, '\0', sizeof(struct ubi_vtbl_record));
	tmp = (vi->bytes + ui->leb_size - 1) / ui->leb_size;
	vtbl_rec->reserved_pebs = __cpu_to_be32(tmp);
	vtbl_rec->alignment = __cpu_to_be32(vi->alignment);
	vtbl_rec->vol_type = vi->type;
	tmp = ui->leb_size % vi->alignment;
	vtbl_rec->data_pad = __cpu_to_be32(tmp);
	vtbl_rec->flags = vi->flags;

	memcpy(vtbl_rec->name, vi->name, vi->name_len);
	vtbl_rec->name[vi->name_len] = '\0';
	vtbl_rec->name_len = __cpu_to_be16(vi->name_len);

	tmp = crc32(UBI_CRC32_INIT, vtbl_rec, UBI_VTBL_RECORD_SIZE_CRC);
	vtbl_rec->crc =	 __cpu_to_be32(tmp);
	return 0;
}

/**
 * init_ec_hdr - initialize EC header.
 * @ui: libubigen information
 * @hdr: the EC header to initialize
 */
static void init_ec_hdr(const struct ubigen_info *ui,
		        struct ubi_ec_hdr *hdr)
{
	uint32_t crc;

	memset(hdr, '\0', sizeof(struct ubi_ec_hdr));

	hdr->magic = __cpu_to_be32(UBI_EC_HDR_MAGIC);
	hdr->version = ui->ubi_ver;
	hdr->ec = __cpu_to_be64(ui->ec);
	hdr->vid_hdr_offset = __cpu_to_be32(ui->vid_hdr_offs);

	hdr->data_offset = __cpu_to_be32(ui->data_offs);

	crc = crc32(UBI_CRC32_INIT, hdr, UBI_EC_HDR_SIZE_CRC);
	hdr->hdr_crc = __cpu_to_be32(crc);
}

/**
 * init_vid_hdr - initialize VID header.
 * @ui: libubigen information
 * @vi: volume information
 * @hdr: the VID header to initialize
 * @lnum: logical eraseblock number
 * @data: the contents of the LEB (static volumes only)
 * @data_size: amount of data in this LEB (static volumes only)
 *
 * Note, @used_ebs, @data and @data_size are ignored in case of dynamic
 * volumes.
 */
static void init_vid_hdr(const struct ubigen_info *ui,
			 const struct ubigen_vol_info *vi,
			 struct ubi_vid_hdr *hdr, int lnum,
			 const void *data, int data_size)
{
	uint32_t crc;

	memset(hdr, '\0', sizeof(struct ubi_vid_hdr));

	hdr->magic = __cpu_to_be32(UBI_VID_HDR_MAGIC);
	hdr->version = ui->ubi_ver;
	hdr->vol_type = vi->type;
	hdr->vol_id = __cpu_to_be32(vi->id);
	hdr->lnum = __cpu_to_be32(lnum);
	hdr->data_pad = __cpu_to_be32(vi->data_pad);
	hdr->compat = vi->compat;

	if (vi->type == UBI_VID_STATIC) {
		hdr->data_size = __cpu_to_be32(data_size);
		hdr->used_ebs = __cpu_to_be32(vi->used_ebs);
		crc = crc32(UBI_CRC32_INIT, data, data_size);
		hdr->data_crc = __cpu_to_be32(crc);
	}

	crc = crc32(UBI_CRC32_INIT, hdr, UBI_VID_HDR_SIZE_CRC);
	hdr->hdr_crc = __cpu_to_be32(crc);
}

/**
 * ubigen_write_volume - write UBI volume.
 * @ui: libubigen information
 * @vi: volume information
 * @bytes: volume size in bytes
 * @in: input file descriptor (has to be properly seeked)
 * @out: output file descriptor
 *
 * This function reads the contents of the volume from the input file @in and
 * writes the UBI volume to the output file @out. Returns zero on success and
 * %-1 on failure.
 */
int ubigen_write_volume(const struct ubigen_info *ui,
			const struct ubigen_vol_info *vi,
			long long bytes, FILE *in, FILE *out)
{
	int len = vi->usable_leb_size, rd, lnum = 0;
	char inbuf[ui->leb_size], outbuf[ui->peb_size];

	if (vi->id >= ui->max_volumes) {
		errmsg("too high volume id %d, max. volumes is %d",
		       vi->id, ui->max_volumes);
		return -1;
	}
	if (vi->alignment >= ui->leb_size) {
		errmsg("too large alignment %d, max is %d (LEB size)",
		       vi->alignment, ui->leb_size);
		return -1;
	}

	memset(outbuf, 0xFF, ui->data_offs);
	init_ec_hdr(ui, (struct ubi_ec_hdr *)outbuf);

	while (bytes) {
		int l;
		struct ubi_vid_hdr *vid_hdr;

		if (bytes < len)
			len = bytes;
		bytes -= len;

		l = len;
		do {
			rd = fread(inbuf + len - l, 1, l, in);
			if (rd == 0) {
				if (ferror(in))
					errmsg("cannot read %d bytes from the input"
					       " file", l);
				else
					errmsg("not enough data in the input file");
				return -1;
			}

			l -= rd;
		} while (l);

		vid_hdr = (struct ubi_vid_hdr *)(&outbuf[ui->vid_hdr_offs]);
		init_vid_hdr(ui, vi, vid_hdr, lnum, inbuf, len);

		memcpy(outbuf + ui->data_offs, inbuf, len);
		memset(outbuf + ui->data_offs + len, 0xFF,
		       ui->peb_size - ui->data_offs - len);

		if (fwrite(outbuf, 1, ui->peb_size, out) != ui->peb_size) {
			errmsg("cannot write %d bytes from the output"
			       " file", l);
			return -1;
		}

		lnum += 1;
	}

	return 0;
}

/**
 * ubigen_write_layout_vol - write UBI layout volume
 * @ui: libubigen information
 * @vtbl: volume table
 * @out: output file stream
 *
 * This function creates the UBI layout volume which contains 2 copies of the
 * volume table. Returns zero in case of success and %-1 in case of failure.
 */
int ubigen_write_layout_vol(const struct ubigen_info *ui,
			    struct ubi_vtbl_record *vtbl, FILE *out)
{
	int size = ui->leb_size;
	struct ubigen_vol_info vi;
	char outbuf[ui->peb_size];
	struct ubi_vid_hdr *vid_hdr;

	if (size > UBI_MAX_VOLUMES * UBI_VTBL_RECORD_SIZE)
		size = UBI_MAX_VOLUMES * UBI_VTBL_RECORD_SIZE;

	vi.bytes = ui->leb_size * UBI_LAYOUT_VOLUME_EBS;
	vi.id = UBI_LAYOUT_VOLUME_ID;
	vi.alignment = 1;
	vi.data_pad = 0;
	vi.usable_leb_size = ui->leb_size;
	vi.type = UBI_VID_DYNAMIC;
	vi.name = UBI_LAYOUT_VOLUME_NAME;
	vi.name_len = strlen(UBI_LAYOUT_VOLUME_NAME);
	vi.compat = UBI_LAYOUT_VOLUME_COMPAT;

	memset(outbuf, 0xFF, ui->data_offs);
	vid_hdr = (struct ubi_vid_hdr *)(&outbuf[ui->vid_hdr_offs]);
	init_ec_hdr(ui, (struct ubi_ec_hdr *)outbuf);
	memcpy(outbuf + ui->data_offs, vtbl, size);
	memset(outbuf + ui->data_offs + size, 0xFF,
	       ui->peb_size - ui->data_offs - size);

	init_vid_hdr(ui, &vi, vid_hdr, 0, NULL, 0);
	size = fwrite(outbuf, 1, ui->peb_size, out);
	if (size == ui->peb_size) {
		init_vid_hdr(ui, &vi, vid_hdr, 1, NULL, 0);
		size = fwrite(outbuf, 1, ui->peb_size, out);
		if (size != ui->peb_size) {
			errmsg("cannot write %d bytes", ui->peb_size);
			perror("write");
			return -1;
		}
	}

	return 0;
}
