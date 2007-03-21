#ifndef __UBI_H__
#define __UBI_H__
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
 * @file	libubi.h
 * @author	Artem B. Bityutskiy
 * @author      Additions: Oliver Lohmann
 * @version	1.0
 */

#include <stdint.h>
#include <mtd/ubi-user.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @section eh Error Handling
 * The following error indication policy is used: in case of success, all
 * library functions return 0, in case of failure they either return UBI error
 * codes, or -1 if a system error occured; in the latter case the exact error
 * code has to be in the errno variable.
 *
 * @def UBI_ENOTFOUND
 *	@brief UBI was not found in the system.
 * @def UBI_EBUG
 *	@brief An error due to bug in kernel part of UBI in UBI library.
 * @def UBI_EINVAL
 *	@brief Invalid argument.
 * @def UBI_EMACS
 *	@brief Highest error value.
 */
#define UBI_ENOTFOUND	1
#define UBI_EBUG	2
#define UBI_EINVAL	3
#define UBI_EMAX	4


/**
 * UBI library descriptor, vague for library users.
 */
typedef struct ubi_lib *ubi_lib_t;

/**
 * struct ubi_info - general information about UBI.
 *
 * @version    UBI version
 * @nlen_max   maximum length of names of volumes
 * @dev_count  count UBI devices in the system
 */
struct ubi_info
{
	unsigned int version;
	unsigned int nlen_max;
	unsigned int dev_count;
};

/**
 * struct ubi_dev_info - information about an UBI device
 *
 * @wear       average number of erasures of flash erasable blocks
 * @major      major number of the corresponding character device
 * @minor      minor number of the corresponding character device
 * @eb_size    size of eraseblocks
 * @total_ebs  total count of eraseblocks
 * @avail_ebs  count of unused eraseblock available for new volumes
 * @vol_count  total count of volumes in this UBI device
 */
struct ubi_dev_info
{
	unsigned long long wear;
	unsigned int major;
	unsigned int minor;
	unsigned int eb_size;
	unsigned int total_ebs;
	unsigned int avail_ebs;
	unsigned int vol_count;
};

/**
 * struct ubi_vol_info - information about an UBI volume
 *
 * @bytes        volume size in bytes
 * @eraseblocks  volume size in eraseblocks
 * @major        major number of the corresponding character device
 * @minor        minor number of the corresponding character device
 * @type         volume type (%UBI_DYNAMIC_VOLUME or %UBI_STATIC_VOLUME)
 * @dev_path	 device path to volume
 * @name         volume name
 */
struct ubi_vol_info
{
	unsigned long long bytes;
	unsigned int eraseblocks;
	unsigned int major;
	unsigned int minor;
	int type;
	char *dev_path;
	char *name;
};

/**
 * ubi_mkvol - create a dynamic UBI volume.
 *
 * @desc       UBI library descriptor
 * @devn       Number of UBI device to create new volume on
 * @vol_id     volume ID to assign to the new volume
 * @vol_type   volume type (%UBI_DYNAMIC_VOLUME or %UBI_STATIC_VOLUME)
 * @bytes      volume size in bytes
 * @alignment  volume alignment
 * @name       volume name
 *
 * This function creates new UBI volume. If @vol_id is %UBI_VOLN_AUTO, then
 * volume number is assigned automatically. This function returns positive
 * volume number of the new volume in case of success or %-1 in case of
 * failure.
 */
int ubi_mkvol(ubi_lib_t desc, int devn, int vol_id, int vol_type,
	      long long bytes, int alignment, const char *name);

/**
 * ubi_rmvol - remove a volume.
 *
 * @desc       UBI library descriptor
 * @devn       Number of UBI device to remove volume from
 * @vol_id     volume ID to remove
 *
 * This function returns zero in case of success or %-1 in case of failure.
 */
int ubi_rmvol(ubi_lib_t desc, int devn, int vol_id);

/**
 * ubi_get_info - get UBI information.
 *
 * @desc  UBI library descriptor
 * @ubi   UBI information is returned here
 *
 * This function retrieves information about UBI and puts it to @ubi. Returns
 * zero in case of success and %-1 in case of failure.
 */
int ubi_get_info(ubi_lib_t desc, struct ubi_info *ubi);

/**
 * ubi_vol_open - open a UBI volume
 *
 * @desc	UBI library descriptor
 * @devn	Number of UBI device on which to open the volume
 * @vol_id	Number of UBI device on which to open the volume
 * @flags	Flags to pass to open()
 *
 * This function opens a UBI volume on a given UBI device.  It returns
 * the file descriptor of the opened volume device.  In case of an
 * error %-1 is returned and errno is set appropriately.
 */
int ubi_vol_open(ubi_lib_t desc, int devn, int vol_id, int flags);

/**
 * ubi_vol_close - close a UBI volume
 *
 * @vol_fd	file descriptor of UBI volume to close
 *
 * This function closes the given UBI device.
 */
int ubi_vol_close(int vol_fd);

/**
 * ubi_vol_update - initiate volume update on a UBI volume
 * @vol_fd	File descriptor of UBI volume to update
 * @bytes	No. of bytes which shall be written.
 *
 * Initiates a volume update on a given volume.  The caller must then
 * actually write the appropriate number of bytes to the volume by
 * calling write().  Returns 0 on success, else error.
 */
int ubi_vol_update(int vol_fd, unsigned long long bytes);

/**
 * ubi_vol_fopen_read - open a volume for reading, returning a FILE *
 * @desc	UBI library descriptor
 * @devn	UBI device number
 * @vol_id	volume ID to read
 *
 * Opens a volume for reading.  Reading itself can then be performed
 * with fread().  The stream can be closed with fclose().  Returns a
 * stream on success, else NULL.
 */
FILE *
ubi_vol_fopen_read(ubi_lib_t desc, int devn, uint32_t vol_id);

/**
 * ubi_vol_fopen_update - open a volume for writing, returning a FILE *
 * @desc	UBI library descriptor
 * @devn	UBI device number
 * @vol_id	volume ID to update
 * @bytes	No. of bytes which shall be written.
 *
 * Initiates a volume update on a given volume.  The caller must then
 * actually write the appropriate number of bytes to the volume by
 * calling fwrite().  The file can be closed with fclose().  Returns a
 * stream on success, else NULL.
 */
FILE *
ubi_vol_fopen_update(ubi_lib_t desc, int devn, uint32_t vol_id,
		     unsigned long long bytes);

/**
 * ubi_vol_get_used_bytes - determine used bytes in a UBI volume
 * @vol_fd	File descriptor of UBI volume
 * @bytes	Pointer to result
 *
 * Returns 0 on success, else error.
 */
int ubi_vol_get_used_bytes(int vol_fd, unsigned long long *bytes);

/**
 * ubi_open - open UBI library.
 *
 * @desc  A pointer to an UBI library descriptor
 *
 * Returns zero in case of success.
 */
int ubi_open(ubi_lib_t *desc);

/**
 * ubi_close - close UBI library.
 *
 * @desc  A pointer to an UBI library descriptor
 */
int ubi_close(ubi_lib_t *desc);


/**
 * ubi_perror - print UBI error.
 *
 * @prefix  a prefix string to prepend to the error message
 * @code    error code
 *
 * If @code is %-1, this function calls 'perror()'
 */
void ubi_perror(const char *prefix, int code);

/**
 * ubi_set_cdev_pattern - set 'sprintf()'-like pattern of paths to UBI
 * character devices.
 *
 * @desc     UBI library descriptor
 * @pattern  the pattern to set
 *
 * The default UBI character device path is "/dev/ubi%u".
 */
int ubi_set_cdev_pattern(ubi_lib_t desc, const char *pattern);

/**
 * ubi_get_dev_info get information about an UBI device.
 *
 * @desc  UBI library descriptor
 * @devn  UBI device number
 * @di    the requested information is returned here
 */
int ubi_get_dev_info(ubi_lib_t desc, unsigned int devn,
		struct ubi_dev_info *di);

/**
 * ubi_set_vol_cdev_pattern - set 'sprintf()'-like pattµern ofpaths to UBI
 * volume character devices.
 *
 * @desc     UBI library descriptor
 * @pattern  the pattern to set
 *
 * The default UBI character device path is "/dev/ubi%u_%u".
 */
int ubi_set_vol_cdev_pattern(ubi_lib_t desc, const char *pattern);

/**
 * ubi_get_vol_info - get information about an UBI volume
 *
 * @desc  UBI library descriptor
 * @devn  UBI device number the volume belongs to
 * @vol_id  the requested volume number
 * @vi    volume information is returned here
 *
 * Users must free the volume name string @vi->name.
 */
int ubi_get_vol_info(ubi_lib_t desc, unsigned int devn, unsigned int vol_id,
		struct ubi_vol_info *vi);

#ifdef __cplusplus
}
#endif

#endif /* !__UBI_H__ */
