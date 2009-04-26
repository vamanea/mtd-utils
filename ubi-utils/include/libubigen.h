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
 * Authors: Frank Haverkamp
 *          Artem Bityutskiy
 */

#ifndef __LIBUBIGEN_H__
#define __LIBUBIGEN_H__

#include <stdint.h>
#include <mtd/ubi-media.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * struct ubigen_info - libubigen information.
 * @leb_size: logical eraseblock size
 * @peb_size: size of the physical eraseblock
 * @min_io_size: minimum input/output unit size
 * @vid_hdr_offs: offset of the VID header
 * @data_offs: data offset
 * @ubi_ver: UBI version
 * @vtbl_size: volume table size
 * @max_volumes: maximum amount of volumes
 */
struct ubigen_info
{
	int leb_size;
	int peb_size;
	int min_io_size;
	int vid_hdr_offs;
	int data_offs;
	int ubi_ver;
	int vtbl_size;
	int max_volumes;
};

/**
 * struct ubigen_vol_info - information about a volume.
 * @id: volume id
 * @type: volume type (%UBI_VID_DYNAMIC or %UBI_VID_STATIC)
 * @alignment: volume alignment
 * @data_pad: how many bytes are unused at the end of the each physical
 *            eraseblock to satisfy the requested alignment
 * @usable_leb_size: LEB size accessible for volume users
 * @name: volume name
 * @name_len: volume name length
 * @compat: compatibility of this volume (%0, %UBI_COMPAT_DELETE,
 *          %UBI_COMPAT_IGNORE, %UBI_COMPAT_PRESERVE, or %UBI_COMPAT_REJECT)
 * @used_ebs: total number of used logical eraseblocks in this volume (relevant
 *            for static volumes only)
 * @bytes: size of the volume contents in bytes (relevant for static volumes
 *         only)
 * @flags: volume flags (%UBI_VTBL_AUTORESIZE_FLG)
 */
struct ubigen_vol_info
{
	int id;
	int type;
	int alignment;
	int data_pad;
	int usable_leb_size;
	const char *name;
	int name_len;
	int compat;
	int used_ebs;
	long long bytes;
	uint8_t flags;
};

void ubigen_info_init(struct ubigen_info *ui, int peb_size, int min_io_size,
		      int subpage_size, int vid_hdr_offs, int ubi_ver);
struct ubi_vtbl_record *ubigen_create_empty_vtbl(const struct ubigen_info *ui);
void ubigen_init_ec_hdr(const struct ubigen_info *ui,
		        struct ubi_ec_hdr *hdr, long long ec);
int ubigen_get_vtbl_size(const struct ubigen_info *ui);
int ubigen_add_volume(const struct ubigen_info *ui,
		      const struct ubigen_vol_info *vi,
		      struct ubi_vtbl_record *vtbl);
int ubigen_write_volume(const struct ubigen_info *ui,
			const struct ubigen_vol_info *vi, long long ec,
			long long bytes, int in, int out);
int ubigen_write_layout_vol(const struct ubigen_info *ui, int peb1, int peb2,
			    long long ec1, long long ec2,
			    struct ubi_vtbl_record *vtbl, int fd);

#ifdef __cplusplus
}
#endif

#endif /* !__LIBUBIGEN_H__ */
