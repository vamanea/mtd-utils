/*
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
 *
 * Author: Artem Bityutskiy
 *
 * MTD library.
 */

#ifndef __LIBMTD_H__
#define __LIBMTD_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * struct mtd_info - information about an MTD device.
 * @num: MTD device number
 * @major: major number of corresponding character device
 * @minor: minor number of corresponding character device
 * @type: flash type (constants like %MTD_NANDFLASH defined in mtd-abi.h)
 * @type_str: static R/O flash type string
 * @size: device size in bytes
 * @eb_cnt: count of eraseblocks
 * @eb_size: eraseblock size
 * @min_io_size: minimum input/output unit size
 * @subpage_size: sub-page size (not set by 'mtd_get_info()'!!!)
 * @rdonly: non-zero if the device is read-only
 * @allows_bb: non-zero if the MTD device may have bad eraseblocks
 * @fd: descriptor of the opened MTD character device node
 */
struct mtd_info
{
	int num;
	int major;
	int minor;
	int type;
	const char *type_str;
	long long size;
	int eb_cnt;
	int eb_size;
	int min_io_size;
	int subpage_size;
	unsigned int rdonly:1;
	unsigned int allows_bb:1;
	int fd;
};

int mtd_get_info(const char *node, struct mtd_info *mtd);
int mtd_erase(const struct mtd_info *mtd, int eb);
int mtd_is_bad(const struct mtd_info *mtd, int eb);
int mtd_read(const struct mtd_info *mtd, int eb, int offs, void *buf, int len);
int mtd_write(const struct mtd_info *mtd, int eb, int offs, void *buf, int len);

#ifdef __cplusplus
}
#endif

#endif /* __LIBMTD_H__ */
