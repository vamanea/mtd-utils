/*
 * Copyright (c) International Business Machines Corp., 2006
 * Copyright (C) 2009 Nokia Corporation
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

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>

#include <libmtd.h>
#include "libmtd_int.h"
#include "common.h"

/**
 * mkpath - compose full path from 2 given components.
 * @path: the first component
 * @name: the second component
 *
 * This function returns the resulting path in case of success and %NULL in
 * case of failure.
 */
static char *mkpath(const char *path, const char *name)
{
	char *n;
	int len1 = strlen(path);
	int len2 = strlen(name);

	n = malloc(len1 + len2 + 2);
	if (!n) {
		sys_errmsg("cannot allocate %d bytes", len1 + len2 + 2);
		return NULL;
	}

	memcpy(n, path, len1);
	if (n[len1 - 1] != '/')
		n[len1++] = '/';

	memcpy(n + len1, name, len2 + 1);
	return n;
}

/**
 * read_data - read data from a file.
 * @file: the file to read from
 * @buf: the buffer to read to
 * @buf_len: buffer length
 *
 * This function returns number of read bytes in case of success and %-1 in
 * case of failure. Note, if the file contains more then @buf_len bytes of
 * date, this function fails with %EINVAL error code.
 */
static int read_data(const char *file, void *buf, int buf_len)
{
	int fd, rd, tmp, tmp1;

	fd = open(file, O_RDONLY);
	if (fd == -1)
		return -1;

	rd = read(fd, buf, buf_len);
	if (rd == -1) {
		sys_errmsg("cannot read \"%s\"", file);
		goto out_error;
	}

	if (rd == buf_len) {
		errmsg("contents of \"%s\" is too long", file);
		errno = EINVAL;
		goto out_error;
	}

	((char *)buf)[rd] = '\0';

	/* Make sure all data is read */
	tmp1 = read(fd, &tmp, 1);
	if (tmp1 == 1) {
		sys_errmsg("cannot read \"%s\"", file);
		goto out_error;
	}
	if (tmp1) {
		errmsg("file \"%s\" contains too much data (> %d bytes)",
		       file, buf_len);
		errno = EINVAL;
		goto out_error;
	}

	if (close(fd)) {
		sys_errmsg("close failed on \"%s\"", file);
		return -1;
	}

	return rd;

out_error:
	close(fd);
	return -1;
}

/**
 * read_major - read major and minor numbers from a file.
 * @file: name of the file to read from
 * @major: major number is returned here
 * @minor: minor number is returned here
 *
 * This function returns % in case of success, and %-1 in case of failure.
 */
static int read_major(const char *file, int *major, int *minor)
{
	int ret;
	char buf[50];

	ret = read_data(file, buf, 50);
	if (ret < 0)
		return ret;

	ret = sscanf(buf, "%d:%d\n", major, minor);
	if (ret != 2) {
		errno = EINVAL;
		return errmsg("\"%s\" does not have major:minor format", file);
	}

	if (*major < 0 || *minor < 0) {
		errno = EINVAL;
		return errmsg("bad major:minor %d:%d in \"%s\"",
			      *major, *minor, file);
	}

	return 0;
}

/**
 * dev_get_major - get major and minor numbers of an MTD device.
 * @lib: libmtd descriptor
 * @dev_num: MTD device number
 * @major: major number is returned here
 * @minor: minor number is returned here
 *
 * This function returns zero in case of success and %-1 in case of failure.
 */
static int dev_get_major(struct libmtd *lib, int dev_num, int *major, int *minor)
{
	char file[strlen(lib->mtd_dev) + 50];

	sprintf(file, lib->mtd_dev, dev_num);
	return read_major(file, major, minor);
}

/**
 * dev_read_data - read data from an MTD device's sysfs file.
 * @patt: file pattern to read from
 * @dev_num: MTD device number
 * @buf: buffer to read to
 * @buf_len: buffer length
 *
 * This function returns number of read bytes in case of success and %-1 in
 * case of failure.
 */
static int dev_read_data(const char *patt, int dev_num, void *buf, int buf_len)
{
	char file[strlen(patt) + 100];

	sprintf(file, patt, dev_num);
	return read_data(file, buf, buf_len);
}

/**
 * read_hex_ll - read a hex 'long long' value from a file.
 * @file: the file to read from
 * @value: the result is stored here
 *
 * This function reads file @file and interprets its contents as hexadecimal
 * 'long long' integer. If this is not true, it fails with %EINVAL error code.
 * Returns %0 in case of success and %-1 in case of failure.
 */
static int read_hex_ll(const char *file, long long *value)
{
	int fd, rd;
	char buf[50];

	fd = open(file, O_RDONLY);
	if (fd == -1)
		return -1;

	rd = read(fd, buf, sizeof(buf));
	if (rd == -1) {
		sys_errmsg("cannot read \"%s\"", file);
		goto out_error;
	}
	if (rd == sizeof(buf)) {
		errmsg("contents of \"%s\" is too long", file);
		errno = EINVAL;
		goto out_error;
	}
	buf[rd] = '\0';

	if (sscanf(buf, "%llx\n", value) != 1) {
		errmsg("cannot read integer from \"%s\"\n", file);
		errno = EINVAL;
		goto out_error;
	}

	if (*value < 0) {
		errmsg("negative value %lld in \"%s\"", *value, file);
		errno = EINVAL;
		goto out_error;
	}

	if (close(fd))
		return sys_errmsg("close failed on \"%s\"", file);

	return 0;

out_error:
	close(fd);
	return -1;
}

/**
 * read_pos_ll - read a positive 'long long' value from a file.
 * @file: the file to read from
 * @value: the result is stored here
 *
 * This function reads file @file and interprets its contents as a positive
 * 'long long' integer. If this is not true, it fails with %EINVAL error code.
 * Returns %0 in case of success and %-1 in case of failure.
 */
static int read_pos_ll(const char *file, long long *value)
{
	int fd, rd;
	char buf[50];

	fd = open(file, O_RDONLY);
	if (fd == -1)
		return -1;

	rd = read(fd, buf, 50);
	if (rd == -1) {
		sys_errmsg("cannot read \"%s\"", file);
		goto out_error;
	}
	if (rd == 50) {
		errmsg("contents of \"%s\" is too long", file);
		errno = EINVAL;
		goto out_error;
	}

	if (sscanf(buf, "%lld\n", value) != 1) {
		errmsg("cannot read integer from \"%s\"\n", file);
		errno = EINVAL;
		goto out_error;
	}

	if (*value < 0) {
		errmsg("negative value %lld in \"%s\"", *value, file);
		errno = EINVAL;
		goto out_error;
	}

	if (close(fd))
		return sys_errmsg("close failed on \"%s\"", file);

	return 0;

out_error:
	close(fd);
	return -1;
}

/**
 * read_hex_int - read an 'int' value from a file.
 * @file: the file to read from
 * @value: the result is stored here
 *
 * This function is the same as 'read_pos_ll()', but it reads an 'int'
 * value, not 'long long'.
 */
static int read_hex_int(const char *file, int *value)
{
	long long res;

	if (read_hex_ll(file, &res))
		return -1;

	/* Make sure the value has correct range */
	if (res > INT_MAX || res < INT_MIN) {
		errmsg("value %lld read from file \"%s\" is out of range",
		       res, file);
		errno = EINVAL;
		return -1;
	}

	*value = res;
	return 0;
}

/**
 * read_pos_int - read a positive 'int' value from a file.
 * @file: the file to read from
 * @value: the result is stored here
 *
 * This function is the same as 'read_pos_ll()', but it reads an 'int'
 * value, not 'long long'.
 */
static int read_pos_int(const char *file, int *value)
{
	long long res;

	if (read_pos_ll(file, &res))
		return -1;

	/* Make sure the value is not too big */
	if (res > INT_MAX) {
		errmsg("value %lld read from file \"%s\" is out of range",
		       res, file);
		errno = EINVAL;
		return -1;
	}

	*value = res;
	return 0;
}

/**
 * dev_read_hex_int - read an hex 'int' value from an MTD device sysfs file.
 * @patt: file pattern to read from
 * @dev_num: MTD device number
 * @value: the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int dev_read_hex_int(const char *patt, int dev_num, int *value)
{
	char file[strlen(patt) + 50];

	sprintf(file, patt, dev_num);
	return read_hex_int(file, value);
}

/**
 * dev_read_pos_int - read a positive 'int' value from an MTD device sysfs file.
 * @patt: file pattern to read from
 * @dev_num: MTD device number
 * @value: the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int dev_read_pos_int(const char *patt, int dev_num, int *value)
{
	char file[strlen(patt) + 50];

	sprintf(file, patt, dev_num);
	return read_pos_int(file, value);
}

/**
 * dev_read_pos_ll - read a positive 'long long' value from an MTD device sysfs file.
 * @patt: file pattern to read from
 * @dev_num: MTD device number
 * @value: the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int dev_read_pos_ll(const char *patt, int dev_num, long long *value)
{
	char file[strlen(patt) + 50];

	sprintf(file, patt, dev_num);
	return read_pos_ll(file, value);
}

/**
 * type_str2int - convert MTD device type to integer.
 * @str: MTD device type string to convert
 *
 * This function converts MTD device type string @str, read from sysfs, into an
 * integer.
 */
static int type_str2int(const char *str)
{
	if (!strcmp(str, "nand"))
		return MTD_NANDFLASH;
	if (!strcmp(str, "nor"))
		return MTD_NORFLASH;
	if (!strcmp(str, "rom"))
		return MTD_ROM;
	if (!strcmp(str, "absent"))
		return MTD_ABSENT;
	if (!strcmp(str, "dataflash"))
		return MTD_DATAFLASH;
	if (!strcmp(str, "ram"))
		return MTD_RAM;
	if (!strcmp(str, "ubi"))
		return MTD_UBIVOLUME;
	return -1;
}

/**
 * dev_node2num - find UBI device number by its character device node.
 * @lib: MTD library descriptor
 * @node: name of the MTD device node
 * @dev_num: MTD device number is returned here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int dev_node2num(struct libmtd *lib, const char *node, int *dev_num)
{
	struct stat st;
	int i, major, minor;
	struct mtd_info info;

	if (stat(node, &st))
		return sys_errmsg("cannot get information about \"%s\"", node);

	if (!S_ISCHR(st.st_mode)) {
		errmsg("\"%s\" is not a character device", node);
		errno = EINVAL;
		return -1;
	}

	major = major(st.st_rdev);
	minor = minor(st.st_rdev);

	if (mtd_get_info((libmtd_t *)lib, &info))
		return -1;

	for (i = info.lowest_dev_num; i <= info.highest_dev_num; i++) {
		int major1, minor1, ret;

		ret = dev_get_major(lib, i, &major1, &minor1);
		if (ret) {
			if (errno == ENOENT)
				continue;
			if (!errno)
				break;
			return -1;
		}

		if (major1 == major && minor1 == minor) {
			errno = 0;
			*dev_num = i;
			return 0;
		}
	}

	errno = ENODEV;
	return -1;
}

libmtd_t libmtd_open(void)
{
	int fd;
	struct libmtd *lib;

	lib = calloc(1, sizeof(struct libmtd));
	if (!lib)
		return NULL;

	lib->sysfs_mtd = mkpath("/sys", SYSFS_MTD);
	if (!lib->sysfs_mtd)
		goto out_error;

	lib->mtd = mkpath(lib->sysfs_mtd, MTD_NAME_PATT);
	if (!lib->mtd)
		goto out_error;

	/* Make sure MTD is recent enough and supports sysfs */
	fd = open(lib->sysfs_mtd, O_RDONLY);
	if (fd == -1) {
		free(lib->mtd);
		free(lib->sysfs_mtd);
		if (errno != ENOENT || legacy_libmtd_open()) {
			free(lib);
			return NULL;
		}
		lib->mtd_name = lib->mtd = lib->sysfs_mtd = NULL;
		return lib;
	}

	if (close(fd)) {
		sys_errmsg("close failed on \"%s\"", lib->sysfs_mtd);
		goto out_error;
	}

	lib->mtd_name = mkpath(lib->mtd, MTD_NAME);
	if (!lib->mtd_name)
		goto out_error;

	lib->mtd_dev = mkpath(lib->mtd, MTD_DEV);
	if (!lib->mtd_dev)
		goto out_error;

	lib->mtd_type = mkpath(lib->mtd, MTD_TYPE);
	if (!lib->mtd_type)
		goto out_error;

	lib->mtd_eb_size = mkpath(lib->mtd, MTD_EB_SIZE);
	if (!lib->mtd_eb_size)
		goto out_error;

	lib->mtd_size = mkpath(lib->mtd, MTD_SIZE);
	if (!lib->mtd_size)
		goto out_error;

	lib->mtd_min_io_size = mkpath(lib->mtd, MTD_MIN_IO_SIZE);
	if (!lib->mtd_min_io_size)
		goto out_error;

	lib->mtd_subpage_size = mkpath(lib->mtd, MTD_SUBPAGE_SIZE);
	if (!lib->mtd_subpage_size)
		goto out_error;

	lib->mtd_oob_size = mkpath(lib->mtd, MTD_OOB_SIZE);
	if (!lib->mtd_oob_size)
		goto out_error;

	lib->mtd_region_cnt = mkpath(lib->mtd, MTD_REGION_CNT);
	if (!lib->mtd_region_cnt)
		goto out_error;

	lib->mtd_flags = mkpath(lib->mtd, MTD_FLAGS);
	if (!lib->mtd_flags)
		goto out_error;

	lib->sysfs_supported = 1;
	return lib;

out_error:
	libmtd_close((libmtd_t)lib);
	return NULL;
}

void libmtd_close(libmtd_t desc)
{
	struct libmtd *lib = (struct libmtd *)desc;

	free(lib->mtd_flags);
	free(lib->mtd_region_cnt);
	free(lib->mtd_oob_size);
	free(lib->mtd_subpage_size);
	free(lib->mtd_min_io_size);
	free(lib->mtd_size);
	free(lib->mtd_eb_size);
	free(lib->mtd_type);
	free(lib->mtd_dev);
	free(lib->mtd_name);
	free(lib->mtd);
	free(lib->sysfs_mtd);
	free(lib);
}

int mtd_get_info(libmtd_t desc, struct mtd_info *info)
{
	DIR *sysfs_mtd;
	struct dirent *dirent;
	struct libmtd *lib = (struct libmtd *)desc;

	memset(info, 0, sizeof(struct mtd_info));

	if (!lib->sysfs_supported)
		return legacy_mtd_get_info(info);

	info->sysfs_supported = 1;

	/*
	 * We have to scan the MTD sysfs directory to identify how many MTD
	 * devices are present.
	 */
	sysfs_mtd = opendir(lib->sysfs_mtd);
	if (!sysfs_mtd) {
		if (errno == ENOENT) {
			errno = ENODEV;
			return -1;
		}
		return sys_errmsg("cannot open \"%s\"", lib->sysfs_mtd);
	}

	info->lowest_dev_num = INT_MAX;
	while (1) {
		int dev_num, ret;
		char tmp_buf[256];

		errno = 0;
		dirent = readdir(sysfs_mtd);
		if (!dirent)
			break;

		if (strlen(dirent->d_name) >= 255) {
			errmsg("invalid entry in %s: \"%s\"",
			       lib->sysfs_mtd, dirent->d_name);
			errno = EINVAL;
			goto out_close;
		}

		ret = sscanf(dirent->d_name, MTD_NAME_PATT"%s",
			     &dev_num, tmp_buf);
		if (ret == 1) {
			info->dev_count += 1;
			if (dev_num > info->highest_dev_num)
				info->highest_dev_num = dev_num;
			if (dev_num < info->lowest_dev_num)
				info->lowest_dev_num = dev_num;
		}
	}

	if (!dirent && errno) {
		sys_errmsg("readdir failed on \"%s\"", lib->sysfs_mtd);
		goto out_close;
	}

	if (closedir(sysfs_mtd))
		return sys_errmsg("closedir failed on \"%s\"", lib->sysfs_mtd);

	if (info->lowest_dev_num == INT_MAX)
		info->lowest_dev_num = 0;

	return 0;

out_close:
	closedir(sysfs_mtd);
	return -1;
}

int mtd_get_dev_info1(libmtd_t desc, int dev_num, struct mtd_dev_info *mtd)
{
	int ret;
	struct stat st;
	struct libmtd *lib = (struct libmtd *)desc;

	memset(mtd, 0, sizeof(struct mtd_dev_info));
	mtd->dev_num = dev_num;

	if (!lib->sysfs_supported)
		return legacy_get_dev_info1(dev_num, mtd);
	else {
		char file[strlen(lib->mtd) + 10];

		sprintf(file, lib->mtd, dev_num);
		if (stat(file, &st)) {
			if (errno == ENOENT)
				errno = ENODEV;
			return -1;
		}
	}

	if (dev_get_major(lib, dev_num, &mtd->major, &mtd->minor))
		return -1;

	ret = dev_read_data(lib->mtd_name, dev_num, &mtd->name,
			    MTD_NAME_MAX + 1);
	if (ret < 0)
		return -1;
	((char *)mtd->name)[ret - 1] = '\0';

	ret = dev_read_data(lib->mtd_type, dev_num, &mtd->type_str,
			    MTD_TYPE_MAX + 1);
	if (ret < 0)
		return -1;
	((char *)mtd->type_str)[ret - 1] = '\0';

	if (dev_read_pos_int(lib->mtd_eb_size, dev_num, &mtd->eb_size))
		return -1;
	if (dev_read_pos_ll(lib->mtd_size, dev_num, &mtd->size))
		return -1;
	if (dev_read_pos_int(lib->mtd_min_io_size, dev_num, &mtd->min_io_size))
		return -1;
	if (dev_read_pos_int(lib->mtd_subpage_size, dev_num, &mtd->subpage_size))
		return -1;
	if (dev_read_pos_int(lib->mtd_oob_size, dev_num, &mtd->oob_size))
		return -1;
	if (dev_read_pos_int(lib->mtd_region_cnt, dev_num, &mtd->region_cnt))
		return -1;
	if (dev_read_hex_int(lib->mtd_flags, dev_num, &ret))
		return -1;
	mtd->writable = !!(ret & MTD_WRITEABLE);

	mtd->eb_cnt = mtd->size / mtd->eb_size;
	mtd->type = type_str2int(mtd->type_str);
	mtd->bb_allowed = !!(mtd->type == MTD_NANDFLASH);

	return 0;
}

int mtd_get_dev_info(libmtd_t desc, const char *node, struct mtd_dev_info *mtd)
{
	int dev_num;
	struct libmtd *lib = (struct libmtd *)desc;

	if (!lib->sysfs_supported)
		return legacy_get_dev_info(node, mtd);

	if (dev_node2num(lib, node, &dev_num))
		return -1;

	return mtd_get_dev_info1(desc, dev_num, mtd);
}

int mtd_erase(const struct mtd_dev_info *mtd, int fd, int eb)
{
	struct erase_info_user ei;

	ei.start = eb * mtd->eb_size;;
	ei.length = mtd->eb_size;
	return ioctl(fd, MEMERASE, &ei);
}

int mtd_is_bad(const struct mtd_dev_info *mtd, int fd, int eb)
{
	int ret;
	loff_t seek;

	if (eb < 0 || eb >= mtd->eb_cnt) {
		errmsg("bad eraseblock number %d, mtd%d has %d eraseblocks",
		       eb, mtd->dev_num, mtd->eb_cnt);
		errno = EINVAL;
		return -1;
	}

	if (!mtd->bb_allowed)
		return 0;

	seek = (loff_t)eb * mtd->eb_size;
	ret = ioctl(fd, MEMGETBADBLOCK, &seek);
	if (ret == -1)
		return sys_errmsg("MEMGETBADBLOCK ioctl failed for "
				  "eraseblock %d (mtd%d)", eb, mtd->dev_num);
	return ret;
}

int mtd_mark_bad(const struct mtd_dev_info *mtd, int fd, int eb)
{
	int ret;
	loff_t seek;

	if (!mtd->bb_allowed) {
		errno = EINVAL;
		return -1;
	}

	if (eb < 0 || eb >= mtd->eb_cnt) {
		errmsg("bad eraseblock number %d, mtd%d has %d eraseblocks",
		       eb, mtd->dev_num, mtd->eb_cnt);
		errno = EINVAL;
		return -1;
	}

	seek = (loff_t)eb * mtd->eb_size;
	ret = ioctl(fd, MEMSETBADBLOCK, &seek);
	if (ret == -1)
		return sys_errmsg("MEMSETBADBLOCK ioctl failed for "
			          "eraseblock %d (mtd%d)", eb, mtd->dev_num);
	return 0;
}

int mtd_read(const struct mtd_dev_info *mtd, int fd, int eb, int offs,
	     void *buf, int len)
{
	int ret, rd = 0;
	off_t seek;

	if (eb < 0 || eb >= mtd->eb_cnt) {
		errmsg("bad eraseblock number %d, mtd%d has %d eraseblocks",
		       eb, mtd->dev_num, mtd->eb_cnt);
		errno = EINVAL;
		return -1;
	}
	if (offs < 0 || offs + len > mtd->eb_size) {
		errmsg("bad offset %d or length %d, mtd%d eraseblock size is %d",
		       offs, len, mtd->dev_num, mtd->eb_size);
		errno = EINVAL;
		return -1;
	}

	/* Seek to the beginning of the eraseblock */
	seek = (off_t)eb * mtd->eb_size + offs;
	if (lseek(fd, seek, SEEK_SET) != seek)
		return sys_errmsg("cannot seek mtd%d to offset %llu",
				  mtd->dev_num, (unsigned long long)seek);

	while (rd < len) {
		ret = read(fd, buf, len);
		if (ret < 0)
			return sys_errmsg("cannot read %d bytes from mtd%d (eraseblock %d, offset %d)",
					  len, mtd->dev_num, eb, offs);
		rd += ret;
	}

	return 0;
}

int mtd_write(const struct mtd_dev_info *mtd, int fd, int eb, int offs,
	      void *buf, int len)
{
	int ret;
	off_t seek;

	if (eb < 0 || eb >= mtd->eb_cnt) {
		errmsg("bad eraseblock number %d, mtd%d has %d eraseblocks",
		       eb, mtd->dev_num, mtd->eb_cnt);
		errno = EINVAL;
		return -1;
	}
	if (offs < 0 || offs + len > mtd->eb_size) {
		errmsg("bad offset %d or length %d, mtd%d eraseblock size is %d",
		       offs, len, mtd->dev_num, mtd->eb_size);
		errno = EINVAL;
		return -1;
	}

	if (offs % mtd->subpage_size) {
		errmsg("write offset %d is not aligned to mtd%d min. I/O size %d",
		       offs, mtd->dev_num, mtd->subpage_size);
		errno = EINVAL;
		return -1;
	}
	if (len % mtd->subpage_size) {
		errmsg("write length %d is not aligned to mtd%d min. I/O size %d",
		       len, mtd->dev_num, mtd->subpage_size);
		errno = EINVAL;
		return -1;
	}

	/* Seek to the beginning of the eraseblock */
	seek = (off_t)eb * mtd->eb_size + offs;
	if (lseek(fd, seek, SEEK_SET) != seek)
		return sys_errmsg("cannot seek mtd%d to offset %llu",
				  mtd->dev_num, (unsigned long long)seek);

	ret = write(fd, buf, len);
	if (ret != len)
		return sys_errmsg("cannot write %d bytes to mtd%d (eraseblock %d, offset %d)",
				  len, mtd->dev_num, eb, offs);

	return 0;
}

int mtd_probe_node(libmtd_t desc, const char *node)
{
	struct stat st;
	struct mtd_info info;
	int i, major, minor;
	struct libmtd *lib = (struct libmtd *)desc;

	if (stat(node, &st))
		return sys_errmsg("cannot get information about \"%s\"", node);

	if (!S_ISCHR(st.st_mode)) {
		errmsg("\"%s\" is not a character device", node);
		errno = EINVAL;
		return -1;
	}

	major = major(st.st_rdev);
	minor = minor(st.st_rdev);

	if (mtd_get_info((libmtd_t *)lib, &info))
		return -1;

	if (!lib->sysfs_supported)
		return 0;

	for (i = info.lowest_dev_num; i <= info.highest_dev_num; i++) {
		int major1, minor1, ret;

		ret = dev_get_major(lib, i, &major1, &minor1);
		if (ret) {
			if (errno == ENOENT)
				continue;
			if (!errno)
				break;
			return -1;
		}

		if (major1 == major && minor1 == minor)
			return 1;
	}

	errno = 0;
	return -1;
}
