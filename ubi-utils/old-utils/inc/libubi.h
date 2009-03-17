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

#ifndef __LIBUBI_H__
#define __LIBUBI_H__

#include <stdint.h>
#include <mtd/ubi-user.h>
#include <ctype.h>
#include <mtd/ubi-media.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UBI version libubi is made for */
#define LIBUBI_UBI_VERSION 1

/* UBI library descriptor */
typedef void * libubi_t;

/**
 * struct ubi_mkvol_request - volume creation request.
 * */
struct ubi_mkvol_request
{
	int vol_id;
	int alignment;
	long long bytes;
	int vol_type;
	const char *name;
};

/**
 * struct ubi_info - general UBI information.
 *
 * @dev_count        count of UBI devices in system
 * @lowest_dev_num   lowest UBI device number
 * @highest_dev_num  highest UBI device number
 * @version          UBI version
 */
struct ubi_info
{
	int dev_count;
	int lowest_dev_num;
	int highest_dev_num;
	int version;
};

/**
 * struct ubi_dev_info - UBI device information.
 *
 * @vol_count        count of volumes on this UBI device
 * @lowest_vol_num   lowest volume number
 * @highest_vol_num  highest volume number
 * @total_ebs        total number of eraseblocks on this UBI device
 * @avail_ebs        how many eraseblocks are not used and available for new
 *                   volumes
 * @total_bytes      @total_ebs * @eb_size
 * @avail_bytes      @avail_ebs * @eb_size
 * @bad_count        count of bad eraseblocks
 * @eb_size          size of UBI eraseblock
 * @max_ec           current highest erase counter value
 * @bad_rsvd         how many physical eraseblocks of the underlying flash
 *                   device are reserved for bad eraseblocks handling
 * @max_vol_count    maximum count of volumes on this UBI device
 * @min_io_size      minimum input/output size of the UBI device
 */
struct ubi_dev_info
{
	int dev_num;
	int vol_count;
	int lowest_vol_num;
	int highest_vol_num;
	int total_ebs;
	int avail_ebs;
	long long total_bytes;
	long long avail_bytes;
	int bad_count;
	int eb_size;
	long long max_ec;
	int bad_rsvd;
	int max_vol_count;
	int min_io_size;
};

/**
 * struct ubi_vol_info - UBI volume information.
 *
 * @dev_num      UBI device number the volume resides on
 * @vol_id       ID of this volume
 * @type         volume type (%UBI_DYNAMIC_VOLUME or %UBI_STATIC_VOLUME)
 * @alignment    alignemnt of this volume
 * @data_bytes   how many data bytes are stored on this volume (equivalent to
 *               @rsvd_bytes for dynamic volumes)
 * @rsvd_bytes   how many bytes are reserved for this volume
 * @rsvd_ebs     how many eraseblocks are reserved for this volume
 * @eb_size      logical eraseblock size of this volume (may be less then
 *               device's logical eraseblock size due to alignment)
 * @corrupted    the volume is corrupted if this flag is not zero
 * @name         volume name (null-terminated)
 */
struct ubi_vol_info
{
	int dev_num;
	int vol_id;
	int type;
	int alignment;
	long long data_bytes;
	long long rsvd_bytes;
	int rsvd_ebs;
	int eb_size;
	int corrupted;
	char name[UBI_VOL_NAME_MAX + 1];
};

/**
 * libubi_open - open UBI library.
 *
 * This function initializes and opens the UBI library and returns UBI library
 * descriptor in case of success and %NULL in case of failure.
 */
libubi_t libubi_open(void);

/**
 * libubi_close - close UBI library
 *
 * @desc UBI library descriptor
 */
void libubi_close(libubi_t desc);

/**
 * ubi_get_info - get general UBI information.
 *
 * @info  pointer to the &struct ubi_info object to fill
 * @desc  UBI library descriptor
 *
 * This function fills the passed @info object with general UBI information and
 * returns %0 in case of success and %-1 in case of failure.
 */
int ubi_get_info(libubi_t desc, struct ubi_info *info);

/**
 * ubi_mkvol - create an UBI volume.
 *
 * @desc  UBI library descriptor
 * @node  name of the UBI character device to create a volume at
 * @req   UBI volume creation request (defined at <mtd/ubi-user.h>)
 *
 * This function creates a UBI volume as described at @req and returns %0 in
 * case of success and %-1 in case of failure. The assigned volume ID is
 * returned in @req->vol_id.
 */
int ubi_mkvol(libubi_t desc, const char *node, struct ubi_mkvol_request *req);

/**
 * ubi_rmvol - remove a UBI volume.
 *
 * @desc    UBI library descriptor
 * @node    name of the UBI character device to remove a volume from
 * @vol_id  ID of the volume to remove
 *
 * This function removes volume @vol_id from UBI device @node and returns %0 in
 * case of success and %-1 in case of failure.
 */
int ubi_rmvol(libubi_t desc, const char *node, int vol_id);

/**
 * ubi_rsvol - re-size UBI volume.
 *
 * @desc   UBI library descriptor
 * @node   name of the UBI character device owning the volume which should be
 *         re-sized
 * @vol_id volume ID to re-size
 * @bytes  new volume size in bytes
 *
 * This function returns %0 in case of success and %-1 in case of error.
 */
int ubi_rsvol(libubi_t desc, const char *node, int vol_id, long long bytes);

/**
 * ubi_get_dev_info - get UBI device information.
 *
 * @desc  UBI library descriptor
 * @node  name of the UBI character device to fetch information about
 * @info  pointer to the &struct ubi_dev_info object to fill
 *
 * This function fills the passed @info object with UBI device information and
 * returns %0 in case of success and %-1 in case of failure.
 */
int ubi_get_dev_info(libubi_t desc, const char *node,
		     struct ubi_dev_info *info);

/**
 * ubi_get_dev_info1 - get UBI device information.
 *
 * @desc     UBI library descriptor
 * @dev_num  UBI device number to fetch information about
 * @info     pointer to the &struct ubi_dev_info object to fill
 *
 * This function is identical to 'ubi_get_dev_info()' except that it accepts UBI
 * device number, not UBI character device.
 */
int ubi_get_dev_info1(libubi_t desc, int dev_num, struct ubi_dev_info *info);

/**
 * ubi_get_vol_info - get UBI volume information.
 *
 * @desc     UBI library descriptor
 * @node     name of the UBI volume character device to fetch information about
 * @info     pointer to the &struct ubi_vol_info object to fill
 *
 * This function fills the passed @info object with UBI volume information and
 * returns %0 in case of success and %-1 in case of failure.
 */
int ubi_get_vol_info(libubi_t desc, const char *node,
		     struct ubi_vol_info *info);

/**
 * ubi_get_vol_info1 - get UBI volume information.
 *
 * @desc     UBI library descriptor
 * @dev_num  UBI device number
 * @vol_id   ID of the UBI volume to fetch information about
 * @info     pointer to the &struct ubi_vol_info object to fill
 *
 * This function is identical to 'ubi_get_vol_info()' except that it accepts UBI
 * volume number, not UBI volume character device.
 */
int ubi_get_vol_info1(libubi_t desc, int dev_num, int vol_id,
		      struct ubi_vol_info *info);

/**
 * ubi_update_start - start UBI volume update.
 *
 * @desc   UBI library descriptor
 * @fd     volume character devie file descriptor
 * @bytes  how many bytes will be written to the volume
 *
 * This function initiates UBI volume update and returns %0 in case of success
 * and %-1 in case of error.
 */
int ubi_update_start(libubi_t desc, int fd, long long bytes);

#ifdef __cplusplus
}
#endif

#endif /* !__LIBUBI_H__ */
