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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

#include <mtd/mtd-user.h>
#include <libmtd.h>
#include "common.h"

#define PROGRAM_NAME "libmtd"
#define MTD_DEV_MAJOR 90

/**
 * mtd_get_info - get information about an MTD device.
 * @node: name of the MTD device node
 * @mtd: the MTD device information is returned here
 *
 * This function gets information about MTD device defined by the @node device
 * node file and saves this information in the @mtd object. Returns %0 in case
 * of success and %-1 in case of failure.
 */
int mtd_get_info(const char *node, struct mtd_info *mtd)
{
	struct stat st;
	struct mtd_info_user ui;
	int ret;
	loff_t offs = 0;

	if (stat(node, &st))
		return sys_errmsg("cannot open \"%s\"", node);

	if (!S_ISCHR(st.st_mode)) {
		errno = EINVAL;
		return errmsg("\"%s\" is not a character device", node);
	}

	mtd->major = major(st.st_rdev);
	mtd->minor = minor(st.st_rdev);

	if (mtd->major != MTD_DEV_MAJOR) {
		errno = EINVAL;
		return errmsg("\"%s\" has major number %d, MTD devices have "
			      "major %d", node, mtd->major, MTD_DEV_MAJOR);
	}

	mtd->num = mtd->minor / 2;
	mtd->rdonly = mtd->minor & 1;

	mtd->fd = open(node, O_RDWR);
	if (mtd->fd == -1)
		return sys_errmsg("cannot open \"%s\"", node);

	if (ioctl(mtd->fd, MEMGETINFO, &ui)) {
		sys_errmsg("MEMGETINFO ioctl request failed");
		goto out_close;
	}

	ret = ioctl(mtd->fd, MEMGETBADBLOCK, &offs);
	if (ret == -1) {
		if (errno != EOPNOTSUPP) {
			sys_errmsg("MEMGETBADBLOCK ioctl failed");
			goto out_close;
		}
		errno = 0;
		mtd->allows_bb = 0;
	} else
		mtd->allows_bb = 1;

	mtd->type = ui.type;
	mtd->size = ui.size;
	mtd->eb_size = ui.erasesize;
	mtd->min_io_size = ui.writesize;

	if (mtd->min_io_size <= 0) {
		errmsg("mtd%d (%s) has insane min. I/O unit size %d",
		       mtd->num, node, mtd->min_io_size);
		goto out_close;
	}
	if (mtd->eb_size <= 0 || mtd->eb_size < mtd->min_io_size) {
		errmsg("mtd%d (%s) has insane eraseblock size %d",
		       mtd->num, node, mtd->eb_size);
		goto out_close;
	}
	if (mtd->size <= 0 || mtd->size < mtd->eb_size) {
		errmsg("mtd%d (%s) has insane size %lld",
		       mtd->num, node, mtd->size);
		goto out_close;
	}
	mtd->eb_cnt = mtd->size / mtd->eb_size;

	switch(mtd->type) {
	case MTD_ABSENT:
		errmsg("mtd%d (%s) is removable and is not present",
		       mtd->num, node);
		goto out_close;
	case MTD_RAM:
		mtd->type_str = "RAM-based";
		break;
	case MTD_ROM:
		mtd->type_str = "ROM";
		break;
	case MTD_NORFLASH:
		mtd->type_str = "NOR";
		break;
	case MTD_NANDFLASH:
		mtd->type_str = "NAND";
		break;
	case MTD_DATAFLASH:
		mtd->type_str = "DataFlash";
		break;
	case MTD_UBIVOLUME:
		mtd->type_str = "UBI-emulated MTD";
		break;
	default:
		mtd->type_str = "Unknown flash type";
		break;
	}

	if (!(ui.flags & MTD_WRITEABLE))
		mtd->rdonly = 1;

	return 0;

out_close:
	close(mtd->fd);
	return -1;
}

/**
 * mtd_erase - erase an eraseblock.
 * @mtd: MTD device description object
 * @eb: eraseblock to erase
 *
 * This function erases the eraseblock and returns %0 in case of success and
 * %-1 in case of failure.
 */
int mtd_erase(const struct mtd_info *mtd, int eb)
{
	struct erase_info_user ei;

	ei.start = eb * mtd->eb_size;;
	ei.length = mtd->eb_size;
	return ioctl(mtd->fd, MEMERASE, &ei);
}

/**
 * mtd_is_bad - check if eraseblock is bad.
 * @mtd: MTD device description object
 * @eb: eraseblock to check
 *
 * This function checks if eraseblock @eb is bad. Returns %0 if not, %1 if yes,
 * and %-1 in case of failure.
 */
int mtd_is_bad(const struct mtd_info *mtd, int eb)
{
	int ret;
	loff_t seek;

	if (eb < 0 || eb >= mtd->eb_cnt) {
		errmsg("bad eraseblock number %d, mtd%d has %d eraseblocks",
		       eb, mtd->num, mtd->eb_cnt);
		errno = EINVAL;
		return -1;
	}

	if (!mtd->allows_bb)
		return 0;

	seek = eb * mtd->eb_size;
	ret = ioctl(mtd->fd, MEMGETBADBLOCK, &seek);
	if (ret == -1) {
		sys_errmsg("MEMGETBADBLOCK ioctl failed for "
			   "eraseblock %d (mtd%d)", eb, mtd->num);
		return -1;
	}

	return ret;
}

/**
 * mtd_read - read data from an MTD device.
 * @mtd: MTD device description object
 * @eb: eraseblock to read from
 * @offs: offset withing the eraseblock to read from
 * @buf: buffer to read data to
 * @len: how many bytes to read
 *
 * This function reads @len bytes of data from eraseblock @eb and offset @offs
 * of the MTD device defined by @mtd and stores the read data at buffer @buf.
 * Returns %0 in case of success and %-1 in case of failure.
 */
int mtd_read(const struct mtd_info *mtd, int eb, int offs, void *buf, int len)
{
	int ret, rd = 0;
	off_t seek;

	if (eb < 0 || eb >= mtd->eb_cnt) {
		errmsg("bad eraseblock number %d, mtd%d has %d eraseblocks",
		       eb, mtd->num, mtd->eb_cnt);
		errno = EINVAL;
		return -1;
	}
	if (offs < 0 || offs + len > mtd->eb_size) {
		errmsg("bad offset %d or length %d, mtd%d eraseblock size is %d",
		       offs, len, mtd->num, mtd->eb_size);
		errno = EINVAL;
		return -1;
	}

	/* Seek to the beginning of the eraseblock */
	seek = eb * mtd->eb_size + offs;
	if (lseek(mtd->fd, seek, SEEK_SET) != seek) {
		sys_errmsg("cannot seek mtd%d to offset %llu",
			   mtd->num, (unsigned long long)seek);
		return -1;
	}

	while (rd < len) {
		ret = read(mtd->fd, buf, len);
		if (ret < 0) {
			sys_errmsg("cannot read %d bytes from mtd%d (eraseblock %d, offset %d)",
				   len, mtd->num, eb, offs);
			return -1;
		}
		rd += ret;
	}

	return 0;
}

/**
 * mtd_write - write data to an MTD device.
 * @mtd: MTD device description object
 * @eb: eraseblock to write to
 * @offs: offset withing the eraseblock to write to
 * @buf: buffer to write
 * @len: how many bytes to write
 *
 * This function writes @len bytes of data to eraseblock @eb and offset @offs
 * of the MTD device defined by @mtd. Returns %0 in case of success and %-1 in
 * case of failure.
 */
int mtd_write(const struct mtd_info *mtd, int eb, int offs, void *buf, int len)
{
	int ret;
	off_t seek;

	if (eb < 0 || eb >= mtd->eb_cnt) {
		errmsg("bad eraseblock number %d, mtd%d has %d eraseblocks",
		       eb, mtd->num, mtd->eb_cnt);
		errno = EINVAL;
		return -1;
	}
	if (offs < 0 || offs + len > mtd->eb_size) {
		errmsg("bad offset %d or length %d, mtd%d eraseblock size is %d",
		       offs, len, mtd->num, mtd->eb_size);
		errno = EINVAL;
		return -1;
	}
#if 0
	if (offs % mtd->subpage_size) {
		errmsg("write offset %d is not aligned to mtd%d min. I/O size %d",
		       offs, mtd->num, mtd->subpage_size);
		errno = EINVAL;
		return -1;
	}
	if (len % mtd->subpage_size) {
		errmsg("write length %d is not aligned to mtd%d min. I/O size %d",
		       len, mtd->num, mtd->subpage_size);
		errno = EINVAL;
		return -1;
	}
#endif

	/* Seek to the beginning of the eraseblock */
	seek = eb * mtd->eb_size + offs;
	if (lseek(mtd->fd, seek, SEEK_SET) != seek) {
		sys_errmsg("cannot seek mtd%d to offset %llu",
			   mtd->num, (unsigned long long)seek);
		return -1;
	}

	ret = write(mtd->fd, buf, len);
	if (ret != len) {
		sys_errmsg("cannot write %d bytes to mtd%d (eraseblock %d, offset %d)",
			   len, mtd->num, eb, offs);
		return -1;
	}

	return 0;
}
