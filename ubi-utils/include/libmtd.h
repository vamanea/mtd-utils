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
 * @subpage_size: sub-page size
 * @rdonly: non-zero if the device is read-only
 * @allows_bb: non-zero if the MTD device may have bad eraseblocks
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
};

/**
 * mtd_get_dev_info - get information about an MTD device.
 * @node: name of the MTD device node
 * @mtd: the MTD device information is returned here
 *
 * This function gets information about MTD device defined by the @node device
 * node file and saves this information in the @mtd object. Returns %0 in case
 * of success and %-1 in case of failure.
 */
int mtd_get_dev_info(const char *node, struct mtd_info *mtd);

/**
 * mtd_erase - erase an eraseblock.
 * @mtd: MTD device description object
 * @fd: MTD device node file descriptor
 * @eb: eraseblock to erase
 *
 * This function erases eraseblock @eb of MTD device decribed by @fd. Returns
 * %0 in case of success and %-1 in case of failure.
 */
int mtd_erase(const struct mtd_info *mtd, int fd, int eb);

/**
 * mtd_is_bad - check if eraseblock is bad.
 * @mtd: MTD device description object
 * @fd: MTD device node file descriptor
 * @eb: eraseblock to check
 *
 * This function checks if eraseblock @eb is bad. Returns %0 if not, %1 if yes,
 * and %-1 in case of failure.
 */
int mtd_is_bad(const struct mtd_info *mtd, int fd, int eb);

/**
 * mtd_mark_bad - marks the block as bad.
 * @mtd: MTD device description object
 * @fd: MTD device node file descriptor
 * @eb: eraseblock to mark bad
 *
 * This function marks the eraseblock @eb as bad. Returns %0 if success
 * %-1 if failure
 */
int mtd_mark_bad(const struct mtd_info *mtd, int fd, int eb);

/**
 * mtd_read - read data from an MTD device.
 * @mtd: MTD device description object
 * @fd: MTD device node file descriptor
 * @eb: eraseblock to read from
 * @offs: offset withing the eraseblock to read from
 * @buf: buffer to read data to
 * @len: how many bytes to read
 *
 * This function reads @len bytes of data from eraseblock @eb and offset @offs
 * of the MTD device defined by @mtd and stores the read data at buffer @buf.
 * Returns %0 in case of success and %-1 in case of failure.
 */
int mtd_read(const struct mtd_info *mtd, int fd, int eb, int offs, void *buf,
	     int len);

/**
 * mtd_write - write data to an MTD device.
 * @mtd: MTD device description object
 * @fd: MTD device node file descriptor
 * @eb: eraseblock to write to
 * @offs: offset withing the eraseblock to write to
 * @buf: buffer to write
 * @len: how many bytes to write
 *
 * This function writes @len bytes of data to eraseblock @eb and offset @offs
 * of the MTD device defined by @mtd. Returns %0 in case of success and %-1 in
 * case of failure.
 */
int mtd_write(const struct mtd_info *mtd, int fd, int eb, int offs, void *buf,
	      int len);

#ifdef __cplusplus
}
#endif

#endif /* __LIBMTD_H__ */
