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
 */

/*
 * UBI (Unsorted Block Images) library.
 *
 * Author: Artem B. Bityutskiy
 *         Oliver Lohmann
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <mtd/ubi-user.h>
#include <mtd/ubi-header.h>

#include "libubi.h"
#include "libubi_int.h"
#include "libubi_sysfs.h"

/**
 * struct ubi_lib - UBI library descriptor.
 *
 * @ubi		    general UBI information
 *
 * @sysfs_root	    sysfs root directory
 * @ubi_root	    UBI root directory in sysfs
 *
 * @version	    full path to the "UBI version" sysfs file
 *
 * @cdev_path	    path pattern to UBI character devices
 * @cdev_path_len   maximum length of the @cdev_path string after substitution
 * @udev_path	    path to sysfs directories corresponding to UBI devices
 * @wear_path	    path to sysfs file containing UBI wear information
 * @vol_count_path  path to sysfs file containing the number of volumes in an
 *		    UBI device
 * @tot_ebs_path    path to sysfs file containing the total number of
 *		    eraseblock on an UBI device
 * @avail_ebs_path  path to sysfs file containing the number of unused
 *		    eraseblocks on an UBI device, available for new volumes
 * @eb_size_path    path to sysfs file containing size of UBI eraseblocks
 * @nums_path	    path to sysfs file containing major and minor number of an
 *		    UBI device
 * @vol_cdev_path   path to UBI volume character devices
 * @vdev_path	    path to sysfs directories corresponding to UBI volume
 *		    devices
 * @vol_nums_path   path to sysfs file containing major and minor number of an
 *		    UBI volume device
 * @vol_bytes_path  path to sysfs file containing size of an UBI volume device
 *		    in bytes
 * @vol_ebs_path    path to sysfs file containing the number of eraseblocks in
 *		    an UBI volume device
 * @vol_type_path   path to sysfs file containing type of an UBI volume
 * @vol_name_path   @FIXME: Describe me.
 *
 * This structure is created and initialized by 'ubi_init()' and is passed to
 * all UBI library calls.
 */
struct ubi_lib
{
	struct ubi_info ubi;

	char *sysfs_root;
	char *ubi_root;

	char *version;
	char *cdev_path;
	int  cdev_path_len;
	char *udev_path;
	char *wear_path;
	char *vol_count_path;
	char *tot_ebs_path;
	char *avail_ebs_path;
	char *eb_size_path;
	char *nums_path;
	int  vol_cdev_path_len;
	char *vol_cdev_path;
	char *vdev_path;
	char *vol_nums_path;
	char *vol_bytes_path;
	char *vol_ebs_path;
	char *vol_type_path;
	char *vol_name_path;
};


/**
 * mkpath - compose full path from 2 given components.
 *
 * @path  first component @name	 second component
 *
 * Returns the resulting path in case of success and %NULL in case of failure.
 * Callers have to take care the resulting path is freed.
 */
static char*
mkpath(const char *path, const char *name)
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


static int
get_ubi_info(ubi_lib_t desc, struct ubi_info *ubi)
{
	int err;
	int n = 1;
	char *path;
	struct stat stat;

	err = sysfs_read_int(desc->version, (int*) &ubi->version);
	if (err)
		return -1;

	/* Calculate number of UBI devices */
	do {
		char dir[20];

		sprintf(&dir[0], "ubi%d", n);
		path = mkpath(desc->sysfs_root, dir);
		if (!path)
			return ENOMEM;

		err = lstat(path, &stat);
		if (err == 0)
			n += 1;
		free(path);
	} while (err == 0);

	if (errno != ENOENT)
		return -1;

	if (n == 0) {
		ubi_err("no UBI devices found");
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	ubi->dev_count = n;
	return 0;
}

void
ubi_dump_handler(ubi_lib_t desc)
{
	ubi_lib_t d = desc;
	printf(	"UBI Library Descriptor:\n"
		"ubi_root:	 %s\n"
		"version:	 %s\n"
		"cdev_path:	 %s\n"
		"udev_path:	 %s\n"
		"wear_path:	 %s\n"
		"vol_count_path: %s\n"
		"tot_ebs_path:	 %s\n"
		"avail_ebs_path: %s\n"
		"eb_size_path:	 %s\n"
		"nums_path:	 %s\n"
		"vol_cdev_path:	 %s\n"
		"vdev_path:	 %s\n"
		"vol_nums_path:	 %s\n"
		"vol_bytes_path: %s\n"
		"vol_ebs_path:	 %s\n"
		"vol_type_path:	 %s\n"
		"vol_name_path:	 %s\n"
		"cdev_path_len:	 %d\n\n",
	       d->ubi_root, d->version, d->cdev_path, d->udev_path,
	       d->wear_path, d->vol_count_path, d->tot_ebs_path,
	       d->avail_ebs_path, d->eb_size_path, d->nums_path,
	       d->vol_cdev_path, d->vdev_path, d->vol_nums_path,
	       d->vol_bytes_path, d->vol_ebs_path, d->vol_type_path,
	       d->vol_name_path, d->cdev_path_len);
}

int
ubi_set_cdev_pattern(ubi_lib_t desc, const char *pattern)
{
	char *patt;

	patt = strdup(pattern);
	if (!patt) {
		ubi_err("cannot allocate memory");
		return -1;
	}

	if (desc->cdev_path)
		free(desc->cdev_path);

	desc->cdev_path = patt;
	desc->cdev_path_len = strlen(patt) + 1 + UBI_MAX_ID_SIZE;

	ubi_dbg("ubi dev pattern is now \"%s\"", patt);

	return 0;
}

int
ubi_set_vol_cdev_pattern(ubi_lib_t desc, const char *pattern)
{
	char *patt;

	patt = strdup(pattern);
	if (!patt) {
		ubi_err("cannot allocate memory");
		return -1;
	}

	free(desc->vol_cdev_path);
	desc->vol_cdev_path = patt;
	desc->vol_cdev_path_len = strlen(patt) + 1 + 2 * UBI_MAX_ID_SIZE;

	ubi_dbg("ubi volume dev pattern is now \"%s\"", patt);

	return 0;
}

int
ubi_open(ubi_lib_t *desc)
{
	int err = -1;
	ubi_lib_t res;
	struct stat stat;

	res = calloc(1, sizeof(struct ubi_lib));
	if (!res) {
		ubi_err("cannot allocate memory");
		return -1;
	}

	res->cdev_path = NULL;
	err = ubi_set_cdev_pattern(res, UBI_CDEV_PATH);
	if (err)
		goto error;

	/* TODO: this actually has to be discovered */
	res->sysfs_root = strdup(UBI_SYSFS_ROOT);
	if (!res->sysfs_root)
		goto error;

	res->ubi_root = mkpath(res->sysfs_root, UBI_ROOT);
	if (!res->ubi_root)
		goto error;

	res->version =	mkpath(res->ubi_root, UBI_VER);
	if (!res->version)
		goto error;

	res->udev_path = mkpath(res->ubi_root, "ubi%d/");
	if (!res->udev_path)
		goto error;

	res->wear_path = mkpath(res->udev_path, UBI_WEAR);
	if (!res->wear_path)
		goto error;

	res->vol_count_path = mkpath(res->udev_path, UBI_VOL_COUNT);
	if (!res->vol_count_path)
		goto error;

	res->tot_ebs_path = mkpath(res->udev_path, UBI_AVAIL_EBS);
	if (!res->tot_ebs_path)
		goto error;

	res->avail_ebs_path = mkpath(res->udev_path, UBI_TOT_EBS);
	if (!res->avail_ebs_path)
		goto error;

	res->eb_size_path = mkpath(res->udev_path, UBI_EB_SIZE);
	if (!res->eb_size_path)
		goto error;

	res->nums_path = mkpath(res->udev_path, UBI_NUMS);
	if (!res->nums_path)
		goto error;

	err = ubi_set_vol_cdev_pattern(res, UBI_VOL_CDEV_PATH);
	if (err)
		goto error;

	res->vdev_path = mkpath(res->udev_path, "%d/");
	if (!res->vdev_path)
		goto error;

	res->vol_nums_path = mkpath(res->vdev_path, UBI_NUMS);
	if (!res->vol_nums_path)
		goto error;

	res->vol_bytes_path = mkpath(res->vdev_path, UBI_VBYTES);
	if (!res->vol_bytes_path)
		goto error;

	res->vol_ebs_path = mkpath(res->vdev_path, UBI_VEBS);
	if (!res->vol_ebs_path)
		goto error;

	res->vol_type_path = mkpath(res->vdev_path, UBI_VTYPE);
	if (!res->vol_type_path)
		goto error;

	res->vol_name_path = mkpath(res->vdev_path, UBI_VNAME);
	if (!res->vol_name_path)
		goto error;

	/* Check if UBI exists in the system */
	err = lstat(res->ubi_root, &stat);
	if (err) {
		perror("lstat");
		fprintf(stderr, "%s\n", res->ubi_root);
		err = UBI_ENOTFOUND;
		goto error;
	}

	err = get_ubi_info(res, &res->ubi);
	if (err)
		goto error;

	*desc = res;

	ubi_dbg("opened library successfully.");

	return 0;

error:
	ubi_close(&res);

	if (err == -1 && errno == ENOMEM)
		ubi_err("Cannot allocate memory");

	return err;
}

int
ubi_close(ubi_lib_t *desc)
{
	ubi_lib_t tmp = *desc;

	free(tmp->vol_name_path);
	free(tmp->vol_type_path);
	free(tmp->vol_ebs_path);
	free(tmp->vol_bytes_path);
	free(tmp->vol_nums_path);
	free(tmp->vdev_path);
	free(tmp->vol_cdev_path);
	free(tmp->nums_path);
	free(tmp->eb_size_path);
	free(tmp->avail_ebs_path);
	free(tmp->tot_ebs_path);
	free(tmp->vol_count_path);
	free(tmp->wear_path);
	free(tmp->udev_path);
	free(tmp->cdev_path);
	free(tmp->version);
	free(tmp->ubi_root);
	free(tmp->sysfs_root);
	free(tmp);

	*desc = NULL;

	return 0;
}

void
ubi_perror(const char *prefix, int code)
{
	if (code == 0)
		return;

	fprintf(stderr, "%s: ", prefix);

	switch (code) {
	case UBI_ENOTFOUND:
		fprintf(stderr, "UBI was not found in system\n");
		break;
	case UBI_EBUG:
		fprintf(stderr, "an UBI or UBI library bug\n");
		break;
	case UBI_EINVAL:
		fprintf(stderr, "invalid parameter\n");
		break;
	case -1:
		perror(prefix);
		break;
	default:
		ubi_err("unknown error code %d", code);
		break;
	}
}

int
ubi_get_dev_info(ubi_lib_t desc, unsigned int devn, struct ubi_dev_info *di)
{
	int err;

	if (devn >= desc->ubi.dev_count) {
		ubi_err("bad device number, max is %d\n",
			desc->ubi.dev_count - 1);
		return UBI_EINVAL;
	}

	err = sysfs_read_dev_subst(desc->nums_path, &di->major,
				   &di->minor, 1, devn);
	if (err)
		return -1;

	err = sysfs_read_ull_subst(desc->wear_path, &di->wear, 1, devn);
	if (err)
		return -1;

	err = sysfs_read_uint_subst(desc->vol_count_path,
				    &di->vol_count, 1, devn);
	if (err)
		return -1;

	err = sysfs_read_uint_subst(desc->eb_size_path, &di->eb_size, 1, devn);
	if (err)
		return -1;

	err = sysfs_read_uint_subst(desc->tot_ebs_path, &di->total_ebs, 1, devn);
	if (err)
		return -1;

	err = sysfs_read_uint_subst(desc->avail_ebs_path,
				    &di->avail_ebs, 1, devn);
	if (err)
		return -1;

#if 0
	ubi_dbg("major:minor %d:%d, wear %llu, EB size %d, "
		"vol. count %d, tot. EBs %d, avail. EBs %d",
		di->major, di->minor, di->wear, di->eb_size,
		di->vol_count, di->total_ebs, di->avail_ebs);
#endif

	return err;
}

int
ubi_get_vol_info(ubi_lib_t desc, unsigned int devn, unsigned int vol_id,
		struct ubi_vol_info *req)
{
	int err;
	int len;
	char buf1[10];
	char buf2[UBI_MAX_VOLUME_NAME];

	err = sysfs_read_dev_subst(desc->vol_nums_path, &req->major,
				   &req->minor, 2, devn, vol_id);
	if (err)
		return -1;

	err = sysfs_read_ull_subst(desc->vol_bytes_path,
				   &req->bytes, 2, devn, vol_id);
	if (err)
		return -1;

	err = sysfs_read_uint_subst(desc->vol_ebs_path,
				    &req->eraseblocks, 2, devn, vol_id);
	if (err)
		return -1;

	len = sysfs_read_data_subst(desc->vol_type_path, &buf1[0],
				    10, 2,  devn, vol_id);
	if (len == -1)
		return -1;

	if (buf1[len - 1] != '\n') {
		ubi_err("bad volume type");
		return UBI_EBUG;
	}

	if (!strncmp(&buf1[0], "static", sizeof("static") - 1)) {
		req->type = UBI_STATIC_VOLUME;
	} else if (!strncmp(&buf1[0], "dynamic", sizeof("dynamic") - 1)) {
		req->type = UBI_DYNAMIC_VOLUME;
	} else {
		ubi_err("bad type %s", &buf1[0]);
		return -1;
	}

	len = sysfs_read_data_subst(desc->vol_name_path, &buf2[0],
				    UBI_MAX_VOLUME_NAME, 2,  devn, vol_id);
	if (len == -1)
		return -1;

	if (buf2[len - 1] != '\n') {
		ubi_err("bad volume name");
		return UBI_EBUG;
	}

	req->name = malloc(len);
	if (!req->name) {
		ubi_err("cannot allocate memory");
		return -1;
	}

	memcpy(req->name, &buf2[0], len - 1);
	req->name[len - 1] = '\0';

	return 0;
}

/**
 * ubi_cdev_open - open a UBI device
 *
 * @desc	UBI library descriptor
 * @devn	Number of UBI device to open
 * @flags	Flags to pass to open()
 *
 * This function opens a UBI device by number and returns a file
 * descriptor.  In case of an error %-1 is returned and errno is set
 * appropriately.
 */
static int
ubi_cdev_open(ubi_lib_t desc, int devn, int flags)
{
	char *buf;
	int fd;

	ubi_dbg("desc=%p, devn=%d, flags=%08x\n", desc, devn, flags);

	if (desc == NULL) {
		ubi_err("desc is NULL\n");
		return -1;
	}
	if (desc->vol_cdev_path_len == 0) {
		ubi_err("path_len == 0\n");
		return -1;
	}
	buf = malloc(desc->cdev_path_len);

	sprintf(buf, desc->cdev_path, devn);

	fd = open(buf, flags);
	if (fd == -1)
		ubi_dbg("cannot open %s", buf);

	free(buf);
	return fd;
}

/**
 * ubi_cdev_close - close a UBI device
 *
 * @dev_fd	file descriptor of UBI device to close
 *
 * This function closes the given UBI device.
 */
static int
ubi_cdev_close(int dev_fd)
{
	return close(dev_fd);
}

/**
 * @size is now in bytes.
 */
int
ubi_mkvol(ubi_lib_t desc, int devn, int vol_id, int vol_type,
	  long long bytes, int alignment, const char *name)
{
	int fd;
	int err;
	struct ubi_mkvol_req req;

	if ((fd = ubi_cdev_open(desc, devn, O_RDWR)) == -1)
		return -1;

	req.vol_id = vol_id;
	req.bytes = bytes;
	req.vol_type = vol_type;
	req.alignment = alignment;
	req.name_len = strlen(name);
	req.name = name;

	/* printf("DBG: %s(vol_id=%d, bytes=%lld, type=%d, alig=%d, nlen=%d, "
	       "name=%s)\n", __func__, vol_id, bytes, vol_type, alignment,
	       strlen(name), name);*/

	err = ioctl(fd, UBI_IOCMKVOL, &req);
	if (err < 0) {
		ubi_err("ioctl returned %d errno=%d\n", err, errno);
		goto out_close;
	}

	ubi_dbg("created volume %d, size %lld, name \"%s\" "
		"at UBI dev %d\n", vol_id, bytes, name, devn);

	close(fd);
	return err;
 out_close:
	ubi_cdev_close(fd);
	return err;
}

int
ubi_rmvol(ubi_lib_t desc, int devn, int vol_id)
{
	int fd;
	int err;

	if ((fd = ubi_cdev_open(desc, devn, O_RDWR)) == -1)
		return -1;

	err = ioctl(fd, UBI_IOCRMVOL, &vol_id);
	if (err < 0)
		goto out_close;

	ubi_dbg("removed volume %d", vol_id);

 out_close:
	ubi_cdev_close(fd);
	return err;
}

int
ubi_get_info(ubi_lib_t desc, struct ubi_info *ubi)
{
	memcpy(ubi, &desc->ubi, sizeof(struct ubi_info));
	return 0;
}


int
ubi_vol_open(ubi_lib_t desc, int devn, int vol_id, int flags)
{
	char *buf;
	int fd;

	ubi_dbg("desc=%p, devn=%d, vol_id=%d, flags=%08x\n",
		desc, devn, vol_id, flags);

	if (desc == NULL) {
		ubi_err("desc is NULL\n");
		return -1;
	}
	if (desc->vol_cdev_path_len == 0) {
		ubi_err("path_len == 0\n");
		return -1;
	}
	buf = malloc(desc->cdev_path_len);

	sprintf(buf, desc->vol_cdev_path, devn, vol_id);

	fd = open(buf, flags);
	if (fd == -1)
		ubi_dbg("cannot open %s", buf);

	free(buf);
	return fd;
}

int
ubi_vol_close(int vol_fd)
{
	return close(vol_fd);
}


int
ubi_vol_update(int vol_fd, unsigned long long bytes)
{
	int err;

	err = ioctl(vol_fd, UBI_IOCVOLUP, &bytes);
	if (err) {
		ubi_err("%s failure calling update ioctl\n"
			"    IOCTL(%08lx) err=%d errno=%d\n",
			__func__, (long unsigned int)UBI_IOCVOLUP, err, errno);
	}
	return err;
}

FILE *
ubi_vol_fopen_read(ubi_lib_t desc, int devn, uint32_t vol_id)
{
	FILE *fp;
	int fd;

	fd = ubi_vol_open(desc, devn, vol_id, O_RDONLY);
	if (fd == -1)
		return NULL;

	fp = fdopen(fd, "r");
	if (fp == NULL)
		ubi_vol_close(fd);

	return fp;
}

FILE *
ubi_vol_fopen_update(ubi_lib_t desc, int devn, uint32_t vol_id,
		     unsigned long long bytes)
{
	FILE *fp;
	int fd;
	int err;

	fd = ubi_vol_open(desc, devn, vol_id, O_RDWR);
	if (fd == -1)
		return NULL;

	fp = fdopen(fd, "r+");
	if (fp == NULL) {
		printf("DBG: %s(errno=%d)\n", __func__, errno);
		ubi_vol_close(fd);
		return NULL;
	}
	err = ubi_vol_update(fd, bytes);
	if (err < 0) {
		printf("DBG: %s() fd=%d err=%d\n", __func__, fd, err);
		fclose(fp);
		return NULL;
	}
	return fp;
}

int
ubi_vol_get_used_bytes(int vol_fd, unsigned long long *bytes)
{
	off_t res;

	res = lseek(vol_fd, 0, SEEK_END);
	if (res == (off_t)-1)
		return -1;
	*bytes = (unsigned long long) res;
	res = lseek(vol_fd, 0, SEEK_SET);
	return res == (off_t)-1 ? -1 : 0;
}
