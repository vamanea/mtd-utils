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
 * Author: Artem B. Bityutskiy
 *
 * UBI (Unsorted Block Images) library.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <limits.h>
#include "libubi.h"
#include "libubi_int.h"

libubi_t libubi_open(void)
{
	int fd, version;
	struct libubi *lib;

	lib = calloc(1, sizeof(struct libubi));
	if (!lib)
		return NULL;

	/* TODO: this must be discovered instead */
	lib->sysfs = strdup("/sys");
	if (!lib->sysfs)
		goto error;

	lib->sysfs_ubi = mkpath(lib->sysfs, SYSFS_UBI);
	if (!lib->sysfs_ubi)
		goto error;

	/* Make sure UBI is present */
	fd = open(lib->sysfs_ubi, O_RDONLY);
	if (fd == -1)
		goto error;
	close(fd);

	lib->ubi_dev = mkpath(lib->sysfs_ubi, UBI_DEV_NAME_PATT);
	if (!lib->ubi_dev)
		goto error;

	lib->ubi_version = mkpath(lib->sysfs_ubi, UBI_VER);
	if (!lib->ubi_version)
		goto error;

	lib->dev_dev = mkpath(lib->ubi_dev, DEV_DEV);
	if (!lib->dev_dev)
		goto error;

	lib->dev_avail_ebs = mkpath(lib->ubi_dev, DEV_AVAIL_EBS);
	if (!lib->dev_avail_ebs)
		goto error;

	lib->dev_total_ebs = mkpath(lib->ubi_dev, DEV_TOTAL_EBS);
	if (!lib->dev_total_ebs)
		goto error;

	lib->dev_bad_count = mkpath(lib->ubi_dev, DEV_BAD_COUNT);
	if (!lib->dev_bad_count)
		goto error;

	lib->dev_eb_size = mkpath(lib->ubi_dev, DEV_EB_SIZE);
	if (!lib->dev_eb_size)
		goto error;

	lib->dev_max_ec = mkpath(lib->ubi_dev, DEV_MAX_EC);
	if (!lib->dev_max_ec)
		goto error;

	lib->dev_bad_rsvd = mkpath(lib->ubi_dev, DEV_MAX_RSVD);
	if (!lib->dev_bad_rsvd)
		goto error;

	lib->dev_max_vols = mkpath(lib->ubi_dev, DEV_MAX_VOLS);
	if (!lib->dev_max_vols)
		goto error;

	lib->dev_min_io_size = mkpath(lib->ubi_dev, DEV_MIN_IO_SIZE);
	if (!lib->dev_min_io_size)
		goto error;

	lib->ubi_vol = mkpath(lib->sysfs_ubi, UBI_VOL_NAME_PATT);
	if (!lib->ubi_vol)
		goto error;

	lib->vol_type = mkpath(lib->ubi_vol, VOL_TYPE);
	if (!lib->vol_type)
		goto error;

	lib->vol_dev = mkpath(lib->ubi_vol, VOL_DEV);
	if (!lib->vol_dev)
		goto error;

	lib->vol_alignment = mkpath(lib->ubi_vol, VOL_ALIGNMENT);
	if (!lib->vol_alignment)
		goto error;

	lib->vol_data_bytes = mkpath(lib->ubi_vol, VOL_DATA_BYTES);
	if (!lib->vol_data_bytes)
		goto error;

	lib->vol_rsvd_ebs = mkpath(lib->ubi_vol, VOL_RSVD_EBS);
	if (!lib->vol_rsvd_ebs)
		goto error;

	lib->vol_eb_size = mkpath(lib->ubi_vol, VOL_EB_SIZE);
	if (!lib->vol_eb_size)
		goto error;

	lib->vol_corrupted = mkpath(lib->ubi_vol, VOL_CORRUPTED);
	if (!lib->vol_corrupted)
		goto error;

	lib->vol_name = mkpath(lib->ubi_vol, VOL_NAME);
	if (!lib->vol_name)
		goto error;

	if (read_int(lib->ubi_version, &version))
		goto error;
	if (version != LIBUBI_UBI_VERSION) {
		fprintf(stderr, "LIBUBI: this library was made for UBI version "
				"%d, but UBI version %d is detected\n",
			LIBUBI_UBI_VERSION, version);
		goto error;
	}

	return lib;

error:
	free(lib->vol_corrupted);
	free(lib->vol_eb_size);
	free(lib->vol_rsvd_ebs);
	free(lib->vol_data_bytes);
	free(lib->vol_alignment);
	free(lib->vol_dev);
	free(lib->vol_type);
	free(lib->ubi_vol);
	free(lib->dev_min_io_size);
	free(lib->dev_max_vols);
	free(lib->dev_bad_rsvd);
	free(lib->dev_max_ec);
	free(lib->dev_eb_size);
	free(lib->dev_bad_count);
	free(lib->dev_total_ebs);
	free(lib->dev_avail_ebs);
	free(lib->dev_dev);
	free(lib->ubi_version);
	free(lib->ubi_dev);
	free(lib->sysfs_ubi);
	free(lib->sysfs);
	free(lib);
	return NULL;
}

void libubi_close(libubi_t desc)
{
	struct libubi *lib = (struct libubi *)desc;

	free(lib->vol_name);
	free(lib->vol_corrupted);
	free(lib->vol_eb_size);
	free(lib->vol_rsvd_ebs);
	free(lib->vol_data_bytes);
	free(lib->vol_alignment);
	free(lib->vol_dev);
	free(lib->vol_type);
	free(lib->ubi_vol);
	free(lib->dev_min_io_size);
	free(lib->dev_max_vols);
	free(lib->dev_bad_rsvd);
	free(lib->dev_max_ec);
	free(lib->dev_eb_size);
	free(lib->dev_bad_count);
	free(lib->dev_total_ebs);
	free(lib->dev_avail_ebs);
	free(lib->dev_dev);
	free(lib->ubi_version);
	free(lib->ubi_dev);
	free(lib->sysfs_ubi);
	free(lib->sysfs);
	free(lib);
}

int ubi_get_info(libubi_t desc, struct ubi_info *info)
{
	DIR *sysfs_ubi;
	struct dirent *dirent;
	struct libubi *lib = (struct libubi *)desc;

	memset(info, '\0', sizeof(struct ubi_info));

	/*
	 * We have to scan the UBI sysfs directory to identify how many UBI
	 * devices are present.
	 */
	sysfs_ubi = opendir(lib->sysfs_ubi);
	if (!sysfs_ubi)
		return -1;

	info->lowest_dev_num = INT_MAX;
	while ((dirent = readdir(sysfs_ubi))) {
		char *name = &dirent->d_name[0];
		int dev_num, ret;

		ret = sscanf(name, UBI_DEV_NAME_PATT, &dev_num);
		if (ret == 1) {
			info->dev_count += 1;
			if (dev_num > info->highest_dev_num)
				info->highest_dev_num = dev_num;
			if (dev_num < info->lowest_dev_num)
				info->lowest_dev_num = dev_num;
		}
	}

	if (info->lowest_dev_num == INT_MAX)
		info->lowest_dev_num = 0;

	if (read_int(lib->ubi_version, &info->version))
		goto close;

	return closedir(sysfs_ubi);

close:
	closedir(sysfs_ubi);
	return -1;
}

int ubi_mkvol(libubi_t desc, const char *node, struct ubi_mkvol_request *req)
{
	int fd, ret;
	struct ubi_mkvol_req r;
	size_t n;

	desc = desc;
	r.vol_id = req->vol_id;
	r.alignment = req->alignment;
	r.bytes = req->bytes;
	r.vol_type = req->vol_type;

	n = strlen(req->name);
	if (n > UBI_MAX_VOLUME_NAME)
		return -1;

	strncpy(r.name, req->name, UBI_MAX_VOLUME_NAME + 1);
	r.name_len = n;

	fd = open(node, O_RDONLY);
	if (fd == -1)
		return -1;

	ret = ioctl(fd, UBI_IOCMKVOL, &r);
	if (!ret)
		req->vol_id = r.vol_id;

	close(fd);
	return ret;
}

int ubi_rmvol(libubi_t desc, const char *node, int vol_id)
{
	int fd, ret;

	desc = desc;
	fd = open(node, O_RDONLY);
	if (fd == -1)
		return -1;

	ret = ioctl(fd, UBI_IOCRMVOL, &vol_id);
	close(fd);
	return ret;
}

int ubi_rsvol(libubi_t desc, const char *node, int vol_id, long long bytes)
{
	int fd, ret;
	struct ubi_rsvol_req req;

	desc = desc;
	fd = open(node, O_RDONLY);
	if (fd == -1)
		return -1;

	req.bytes = bytes;
	req.vol_id = vol_id;

	ret = ioctl(fd, UBI_IOCRSVOL, &req);
	close(fd);
	return ret;
}

int ubi_update_start(libubi_t desc, int fd, long long bytes)
{
	desc = desc;
	if (ioctl(fd, UBI_IOCVOLUP, &bytes))
		return -1;
	return 0;
}

int ubi_get_dev_info(libubi_t desc, const char *node, struct ubi_dev_info *info)
{
	int dev_num;
	struct libubi *lib = (struct libubi *)desc;

	dev_num = find_dev_num(lib, node);
	if (dev_num == -1)
		return -1;

	return ubi_get_dev_info1(desc, dev_num, info);
}

int ubi_get_dev_info1(libubi_t desc, int dev_num, struct ubi_dev_info *info)
{
	DIR *sysfs_ubi;
	struct dirent *dirent;
	struct libubi *lib = (struct libubi *)desc;

	memset(info, '\0', sizeof(struct ubi_dev_info));
	info->dev_num = dev_num;

	sysfs_ubi = opendir(lib->sysfs_ubi);
	if (!sysfs_ubi)
		return -1;

	info->lowest_vol_num = INT_MAX;
	while ((dirent = readdir(sysfs_ubi))) {
		char *name = &dirent->d_name[0];
		int vol_id, ret, devno;

		ret = sscanf(name, UBI_VOL_NAME_PATT, &devno, &vol_id);
		if (ret == 2 && devno == dev_num) {
			info->vol_count += 1;
			if (vol_id > info->highest_vol_num)
				info->highest_vol_num = vol_id;
			if (vol_id < info->lowest_vol_num)
				info->lowest_vol_num = vol_id;
		}
	}

	closedir(sysfs_ubi);

	if (info->lowest_vol_num == INT_MAX)
		info->lowest_vol_num = 0;

	if (dev_read_int(lib->dev_avail_ebs, dev_num, &info->avail_ebs))
		return -1;
	if (dev_read_int(lib->dev_total_ebs, dev_num, &info->total_ebs))
		return -1;
	if (dev_read_int(lib->dev_bad_count, dev_num, &info->bad_count))
		return -1;
	if (dev_read_int(lib->dev_eb_size, dev_num, &info->eb_size))
		return -1;
	if (dev_read_int(lib->dev_bad_rsvd, dev_num, &info->bad_rsvd))
		return -1;
	if (dev_read_ll(lib->dev_max_ec, dev_num, &info->max_ec))
		return -1;
	if (dev_read_int(lib->dev_max_vols, dev_num, &info->max_vol_count))
		return -1;
	if (dev_read_int(lib->dev_min_io_size, dev_num, &info->min_io_size))
		return -1;

	info->avail_bytes = info->avail_ebs * info->eb_size;
	info->total_bytes = info->total_ebs * info->eb_size;

	return 0;
}

int ubi_get_vol_info(libubi_t desc, const char *node, struct ubi_vol_info *info)
{
	int vol_id, dev_num;
	struct libubi *lib = (struct libubi *)desc;

	dev_num = find_dev_num_vol(lib, node);
	if (dev_num == -1)
		return -1;

	vol_id = find_vol_num(lib, dev_num, node);
	if (vol_id == -1)
		return -1;

	return ubi_get_vol_info1(desc, dev_num, vol_id, info);
}

int ubi_get_vol_info1(libubi_t desc, int dev_num, int vol_id,
		      struct ubi_vol_info *info)
{
	int ret;
	struct libubi *lib = (struct libubi *)desc;
	char buf[50];

	memset(info, '\0', sizeof(struct ubi_vol_info));
	info->dev_num = dev_num;
	info->vol_id = vol_id;

	ret = vol_read_data(lib->vol_type, dev_num, vol_id, &buf[0], 50);
	if (ret < 0)
		return -1;

	if (strncmp(&buf[0], "static\n", ret) == 0)
		info->type = UBI_STATIC_VOLUME;
	else if (strncmp(&buf[0], "dynamic\n", ret) == 0)
		info->type = UBI_DYNAMIC_VOLUME;
	else {
		fprintf(stderr, "LIBUBI: bad value at sysfs file\n");
		errno = EINVAL;
		return -1;
	}

	ret = vol_read_int(lib->vol_alignment, dev_num, vol_id,
			   &info->alignment);
	if (ret)
		return -1;
	ret = vol_read_ll(lib->vol_data_bytes, dev_num, vol_id,
			  &info->data_bytes);
	if (ret)
		return -1;
	ret = vol_read_int(lib->vol_rsvd_ebs, dev_num, vol_id, &info->rsvd_ebs);
	if (ret)
		return -1;
	ret = vol_read_int(lib->vol_eb_size, dev_num, vol_id, &info->eb_size);
	if (ret)
		return -1;
	ret = vol_read_int(lib->vol_corrupted, dev_num, vol_id,
			   &info->corrupted);
	if (ret)
		return -1;
	info->rsvd_bytes = info->eb_size * info->rsvd_ebs;

	ret = vol_read_data(lib->vol_name, dev_num, vol_id, &info->name,
			    UBI_VOL_NAME_MAX + 2);
	if (ret < 0)
		return -1;

	info->name[ret - 1] = '\0';
	return 0;
}

/**
 * read_int - read an 'int' value from a file.
 *
 * @file   the file to read from
 * @value  the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int read_int(const char *file, int *value)
{
	int fd, rd;
	char buf[50];

	fd = open(file, O_RDONLY);
	if (fd == -1)
		return -1;

	rd = read(fd, &buf[0], 50);
	if (rd == -1)
		goto error;

	if (sscanf(&buf[0], "%d\n", value) != 1) {
		/* This must be a UBI bug */
		fprintf(stderr, "LIBUBI: bad value at sysfs file\n");
		errno = EINVAL;
		goto error;
	}

	close(fd);
	return 0;

error:
	close(fd);
	return -1;
}

/**
 * dev_read_int - read an 'int' value from an UBI device's sysfs file.
 *
 * @patt     the file pattern to read from
 * @dev_num  UBI device number
 * @value    the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int dev_read_int(const char *patt, int dev_num, int *value)
{
	int fd, rd;
	char buf[50];
	char file[strlen(patt) + 50];

	sprintf(&file[0], patt, dev_num);
	fd = open(&file[0], O_RDONLY);
	if (fd == -1)
		return -1;

	rd = read(fd, &buf[0], 50);
	if (rd == -1)
		goto error;

	if (sscanf(&buf[0], "%d\n", value) != 1) {
		/* This must be a UBI bug */
		fprintf(stderr, "LIBUBI: bad value at sysfs file\n");
		errno = EINVAL;
		goto error;
	}

	close(fd);
	return 0;

error:
	close(fd);
	return -1;
}

/**
 * dev_read_ll - read a 'long long' value from an UBI device's sysfs file.
 *
 * @patt     the file pattern to read from
 * @dev_num  UBI device number
 * @value    the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int dev_read_ll(const char *patt, int dev_num, long long *value)
{
	int fd, rd;
	char buf[50];
	char file[strlen(patt) + 50];

	sprintf(&file[0], patt, dev_num);
	fd = open(&file[0], O_RDONLY);
	if (fd == -1)
		return -1;

	rd = read(fd, &buf[0], 50);
	if (rd == -1)
		goto error;

	if (sscanf(&buf[0], "%lld\n", value) != 1) {
		/* This must be a UBI bug */
		fprintf(stderr, "LIBUBI: bad value at sysfs file\n");
		errno = EINVAL;
		goto error;
	}

	close(fd);
	return 0;

error:
	close(fd);
	return -1;
}

/**
 * dev_read_data - read data from an UBI device's sysfs file.
 *
 * @patt     the file pattern to read from
 * @dev_num  UBI device number
 * @buf      buffer to read data to
 * @buf_len  buffer length
 *
 * This function returns number of read bytes in case of success and %-1 in
 * case of failure.
 */
static int dev_read_data(const char *patt, int dev_num, void *buf, int buf_len)
{
	int fd, rd;
	char file[strlen(patt) + 50];

	sprintf(&file[0], patt, dev_num);
	fd = open(&file[0], O_RDONLY);
	if (fd == -1)
		return -1;

	rd = read(fd, buf, buf_len);
	if (rd == -1) {
		close(fd);
		return -1;
	}

	close(fd);
	return rd;
}

/**
 * vol_read_int - read an 'int' value from an UBI volume's sysfs file.
 *
 * @patt     the file pattern to read from
 * @dev_num  UBI device number
 * @vol_id   volume identifier
 * @value    the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int vol_read_int(const char *patt, int dev_num, int vol_id, int *value)
{
	int fd, rd;
	char buf[50];
	char file[strlen(patt) + 100];

	sprintf(&file[0], patt, dev_num, vol_id);
	fd = open(&file[0], O_RDONLY);
	if (fd == -1)
		return -1;

	rd = read(fd, &buf[0], 50);
	if (rd == -1)
		goto error;

	if (sscanf(&buf[0], "%d\n", value) != 1) {
		/* This must be a UBI bug */
		fprintf(stderr, "LIBUBI: bad value at sysfs file\n");
		errno = EINVAL;
		goto error;
	}

	close(fd);
	return 0;

error:
	close(fd);
	return -1;
}

/**
 * vol_read_ll - read a 'long long' value from an UBI volume's sysfs file.
 *
 * @patt     the file pattern to read from
 * @dev_num  UBI device number
 * @vol_id   volume identifier
 * @value    the result is stored here
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int vol_read_ll(const char *patt, int dev_num, int vol_id,
		       long long *value)
{
	int fd, rd;
	char buf[50];
	char file[strlen(patt) + 100];

	sprintf(&file[0], patt, dev_num, vol_id);
	fd = open(&file[0], O_RDONLY);
	if (fd == -1)
		return -1;

	rd = read(fd, &buf[0], 50);
	if (rd == -1)
		goto error;

	if (sscanf(&buf[0], "%lld\n", value) != 1) {
		/* This must be a UBI bug */
		fprintf(stderr, "LIBUBI: bad value at sysfs file\n");
		errno = EINVAL;
		goto error;
	}

	close(fd);
	return 0;

error:
	close(fd);
	return -1;
}

/**
 * vol_read_data - read data from an UBI volume's sysfs file.
 *
 * @patt     the file pattern to read from
 * @dev_num  UBI device number
 * @vol_id   volume identifier
 * @buf      buffer to read to
 * @buf_len  buffer length
 *
 * This function returns number of read bytes in case of success and %-1 in
 * case of failure.
 */
static int vol_read_data(const char *patt, int dev_num, int vol_id, void *buf,
			 int buf_len)
{
	int fd, rd;
	char file[strlen(patt) + 100];

	sprintf(&file[0], patt, dev_num, vol_id);
	fd = open(&file[0], O_RDONLY);
	if (fd == -1)
		return -1;

	rd = read(fd, buf, buf_len);
	if (rd == -1) {
		close(fd);
		return -1;
	}

	close(fd);
	return rd;
}

/**
 * mkpath - compose full path from 2 given components.
 *
 * @path  first component
 * @name  second component
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
	if (!n)
		return NULL;

	memcpy(n, path, len1);
	if (n[len1 - 1] != '/')
		n[len1++] = '/';

	memcpy(n + len1, name, len2 + 1);
	return n;
}

/**
 * find_dev_num - find UBI device number by its character device node.
 *
 * @lib   UBI library descriptor
 * @node  UBI character device node name
 *
 * This function returns positive UBI device number in case of success and %-1
 * in case of failure.
 */
static int find_dev_num(struct libubi *lib, const char *node)
{
	struct stat st;
	struct ubi_info info;
	int i, major, minor;

	if (stat(node, &st))
		return -1;

	if (!S_ISCHR(st.st_mode)) {
		errno = EINVAL;
		return -1;
	}

	major = major(st.st_rdev);
	minor = minor(st.st_rdev);

	if (minor != 0) {
		errno = -EINVAL;
		return -1;
	}

	if (ubi_get_info((libubi_t *)lib, &info))
		return -1;

	for (i = info.lowest_dev_num; i <= info.highest_dev_num; i++) {
		int major1, minor1, ret;
		char buf[50];

		ret = dev_read_data(lib->dev_dev, i, &buf[0], 50);
		if (ret < 0)
			return -1;

		ret = sscanf(&buf[0], "%d:%d\n", &major1, &minor1);
		if (ret != 2) {
			fprintf(stderr, "LIBUBI: bad value at sysfs file\n");
			errno = EINVAL;
			return -1;
		}

		if (minor1 == minor && major1 == major)
			return i;
	}

	errno = ENOENT;
	return -1;
}

/**
 * find_dev_num_vol - find UBI device number by volume character device node.
 *
 * @lib   UBI library descriptor
 * @node  UBI character device node name
 *
 * This function returns positive UBI device number in case of success and %-1
 * in case of failure.
 */
static int find_dev_num_vol(struct libubi *lib, const char *node)
{
	struct stat st;
	struct ubi_info info;
	int i, major;

	if (stat(node, &st))
		return -1;

	if (!S_ISCHR(st.st_mode)) {
		errno = EINVAL;
		return -1;
	}

	major = major(st.st_rdev);

	if (minor(st.st_rdev) == 0) {
		errno = -EINVAL;
		return -1;
	}

	if (ubi_get_info((libubi_t *)lib, &info))
		return -1;

	for (i = info.lowest_dev_num; i <= info.highest_dev_num; i++) {
		int major1, minor1, ret;
		char buf[50];

		ret = dev_read_data(lib->dev_dev, i, &buf[0], 50);
		if (ret < 0)
			return -1;

		ret = sscanf(&buf[0], "%d:%d\n", &major1, &minor1);
		if (ret != 2) {
			fprintf(stderr, "LIBUBI: bad value at sysfs file\n");
			errno = EINVAL;
			return -1;
		}

		if (major1 == major)
			return i;
	}

	errno = ENOENT;
	return -1;
}

/**
 * find_vol_num - find UBI volume number by its character device node.
 *
 * @lib      UBI library descriptor
 * @dev_num  UBI device number
 * @node     UBI volume character device node name
 *
 * This function returns positive UBI volume number in case of success and %-1
 * in case of failure.
 */
static int find_vol_num(struct libubi *lib, int dev_num, const char *node)
{
	struct stat st;
	struct ubi_dev_info info;
	int i, major, minor;

	if (stat(node, &st))
		return -1;

	if (!S_ISCHR(st.st_mode)) {
		errno = EINVAL;
		return -1;
	}

	major = major(st.st_rdev);
	minor = minor(st.st_rdev);

	if (minor == 0) {
		errno = -EINVAL;
		return -1;
	}

	if (ubi_get_dev_info1((libubi_t *)lib, dev_num, &info))
		return -1;

	for (i = info.lowest_vol_num; i <= info.highest_vol_num; i++) {
		int major1, minor1, ret;
		char buf[50];

		ret = vol_read_data(lib->vol_dev,  dev_num, i, &buf[0], 50);
		if (ret < 0)
			return -1;

		ret = sscanf(&buf[0], "%d:%d\n", &major1, &minor1);
		if (ret != 2) {
			fprintf(stderr, "LIBUBI: bad value at sysfs file\n");
			errno = EINVAL;
			return -1;
		}

		if (minor1 == minor && major1 == major)
			return i;
	}

	errno = ENOENT;
	return -1;
}
