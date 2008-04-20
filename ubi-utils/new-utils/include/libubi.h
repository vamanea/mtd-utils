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
 * Author: Artem Bityutskiy
 *
 * UBI (Unsorted Block Images) library.
 */

#ifndef __LIBUBI_H__
#define __LIBUBI_H__

#include <ctype.h>
#include <stdint.h>
#include <mtd/ubi-user.h>
#include <mtd/ubi-header.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UBI version libubi is made for */
#define LIBUBI_UBI_VERSION 1

/* UBI library descriptor */
typedef void * libubi_t;

/**
 * struct ubi_attach_request - MTD device attachement request.
 * @dev_num: number to assigne to the newly created UBI device
 *           (%UBI_DEV_NUM_AUTO should be used to automatically assign the
 *           number)
 * @mtd_num: MTD device number to attach
 * @vid_hdr_offset: VID header offset (%0 means default offset and this is what
 *                  most of the users want)
 */
struct ubi_attach_request
{
	int dev_num;
	int mtd_num;
	int vid_hdr_offset;
};

/**
 * struct ubi_mkvol_request - volume creation request.
 * @vol_id: ID to assign to the new volume (%UBI_VOL_NUM_AUTO should be used to
 *          automatically assign ID)
 * @alignment: volume alignment
 * @bytes: volume size in bytes
 * @vol_type: volume type (%UBI_DYNAMIC_VOLUME or %UBI_STATIC_VOLUME)
 * @name: volume name
 */
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
 * @dev_count: count of UBI devices in system
 * @lowest_dev_num: lowest UBI device number
 * @highest_dev_num: highest UBI device number
 * @version: UBI version
 * @ctrl_major: major number of the UBI control device
 * @ctrl_minor: minor number of the UBI control device
 */
struct ubi_info
{
	int dev_count;
	int lowest_dev_num;
	int highest_dev_num;
	int version;
	int ctrl_major;
	int ctrl_minor;
};

/**
 * struct ubi_dev_info - UBI device information.
 * @vol_count: count of volumes on this UBI device
 * @lowest_vol_num: lowest volume number
 * @highest_vol_num: highest volume number
 * @major: major number of corresponding character device
 * @minor: minor number of corresponding character device
 * @total_lebs: total number of logical eraseblocks on this UBI device
 * @avail_lebs: how many logical eraseblocks are not used and available for new
 *             volumes
 * @total_bytes: @total_lebs * @leb_size
 * @avail_bytes: @avail_lebs * @leb_size
 * @bad_count: count of bad physical eraseblocks
 * @leb_size: logical eraseblock size
 * @max_ec: current highest erase counter value
 * @bad_rsvd: how many physical eraseblocks of the underlying flash device are
 *            reserved for bad eraseblocks handling
 * @max_vol_count: maximum possible number of volumes on this UBI device
 * @min_io_size: minimum input/output unit size of the UBI device
 */
struct ubi_dev_info
{
	int dev_num;
	int vol_count;
	int lowest_vol_num;
	int highest_vol_num;
	int major;
	int minor;
	int total_lebs;
	int avail_lebs;
	long long total_bytes;
	long long avail_bytes;
	int bad_count;
	int leb_size;
	long long max_ec;
	int bad_rsvd;
	int max_vol_count;
	int min_io_size;
};

/**
 * struct ubi_vol_info - UBI volume information.
 * @dev_num: UBI device number the volume resides on
 * @vol_id: ID of this volume
 * @major: major number of corresponding volume character device
 * @minor: minor number of corresponding volume character device
 * @dev_major: major number of corresponding UBI device character device
 * @dev_minor: minor number of corresponding UBI device character device
 * @type: volume type (%UBI_DYNAMIC_VOLUME or %UBI_STATIC_VOLUME)
 * @alignment: alignemnt of this volume
 * @data_bytes: how many data bytes are stored on this volume (equivalent to
 *              @rsvd_bytes for dynamic volumes)
 * @rsvd_bytes: how many bytes are reserved for this volume
 * @rsvd_lebs: how many logical eraseblocks are reserved for this volume
 * @leb_size: logical eraseblock size of this volume (may be less then
 *            device's logical eraseblock size due to alignment)
 * @corrupted: non-zero if the volume is corrupted
 * @name: volume name (null-terminated)
 */
struct ubi_vol_info
{
	int dev_num;
	int vol_id;
	int major;
	int minor;
	int dev_major;
	int dev_minor;
	int type;
	int alignment;
	long long data_bytes;
	long long rsvd_bytes;
	int rsvd_lebs;
	int leb_size;
	int corrupted;
	char name[UBI_VOL_NAME_MAX + 1];
};

/**
 * libubi_open - open UBI library.
 * @required: if non-zero, libubi will print an error messages if this UBI is
 *            not present in the system
 *
 * This function initializes and opens the UBI library and returns UBI library
 * descriptor in case of success and %NULL in case of failure.
 */
libubi_t libubi_open(int required);

/**
 * libubi_close - close UBI library.
 * @desc UBI library descriptor
 */
void libubi_close(libubi_t desc);

/**
 * ubi_get_info - get general UBI information.
 * @desc: UBI library descriptor
 * @info: pointer to the &struct ubi_info object to fill
 *
 * This function fills the passed @info object with general UBI information and
 * returns %0 in case of success and %-1 in case of failure.
 */
int ubi_get_info(libubi_t desc, struct ubi_info *info);

/**
 * mtd_num2ubi_dev - find UBI device by attached MTD device.
 * @@desc: UBI library descriptor
 * @mtd_num: MTD device number
 * @dev_num: UBI device number is returned here
 *
 * This function finds UBI device to which MTD device @mtd_num is attached.
 * Returns %0 if the UBI device was found and %-1 if not.
 */
int mtd_num2ubi_dev(libubi_t desc, int mtd_num, int *dev_num);

/**
 * ubi_attach_mtd - attach MTD device to UBI.
 * @desc: UBI library descriptor
 * @node: name of the UBI control character device node
 * @req: MTD attach request.
 *
 * This function creates a new UBI device by attaching an MTD device as
 * described by @req. Returns %0 in case of success and %-1 in case of failure.
 * The newly created UBI device number is returned in @req->dev_num.
 */
int ubi_attach_mtd(libubi_t desc, const char *node,
		   struct ubi_attach_request *req);

/**
 * ubi_detach_mtd - detach an MTD device.
 * @desc: UBI library descriptor
 * @node: name of the UBI control character device node
 * @mtd_num: MTD device number to detach
 *
 * This function detaches MTD device number @mtd_num from UBI, which means the
 * corresponding UBI device is removed. Returns zero in case of success and %-1
 * in case of failure.
 */
int ubi_detach_mtd(libubi_t desc, const char *node, int mtd_num);

/**
 * ubi_remove_dev - remove an UBI device.
 * @desc: UBI library descriptor
 * @node: name of the UBI control character device node
 * @ubi_dev: UBI device number to remove
 *
 * This function removes UBI device number @ubi_dev and returns zero in case of
 * success and %-1 in case of failure.
 */
int ubi_remove_dev(libubi_t desc, const char *node, int ubi_dev);

/**
 * ubi_mkvol - create an UBI volume.
 * @desc: UBI library descriptor
 * @node: name of the UBI character device to create a volume at
 * @req: UBI volume creation request
 *
 * This function creates a UBI volume as described at @req and returns %0 in
 * case of success and %-1 in case of failure. The assigned volume ID is
 * returned in @req->vol_id.
 */
int ubi_mkvol(libubi_t desc, const char *node, struct ubi_mkvol_request *req);

/**
 * ubi_rmvol - remove a UBI volume.
 * @desc: UBI library descriptor
 * @node: name of the UBI character device to remove a volume from
 * @vol_id: ID of the volume to remove
 *
 * This function removes volume @vol_id from UBI device @node and returns %0 in
 * case of success and %-1 in case of failure.
 */
int ubi_rmvol(libubi_t desc, const char *node, int vol_id);

/**
 * ubi_rsvol - re-size UBI volume.
 * @desc: UBI library descriptor
 * @node: name of the UBI character device owning the volume which should be
 *        re-sized
 * @vol_id: volume ID to re-size
 * @bytes: new volume size in bytes
 *
 * This function returns %0 in case of success and %-1 in case of error.
 */
int ubi_rsvol(libubi_t desc, const char *node, int vol_id, long long bytes);

/**
 * ubi_node_type - test UBI node type.
 * @desc: UBI library descriptor
 * @node: the node to test
 *
 * This function tests whether @node is a UBI device or volume node and returns
 * %1 if this is an UBI device node, %2 if this is a volume node, and %-1 if
 * this is not an UBI node or if an error occurred (the latter is indicated by
 * a non-zero errno).
 */
int ubi_node_type(libubi_t desc, const char *node);

/**
 * ubi_get_dev_info - get UBI device information.
 * @desc: UBI library descriptor
 * @node: name of the UBI character device to fetch information about
 * @info: pointer to the &struct ubi_dev_info object to fill
 *
 * This function fills the passed @info object with UBI device information and
 * returns %0 in case of success and %-1 in case of failure.
 */
int ubi_get_dev_info(libubi_t desc, const char *node,
		     struct ubi_dev_info *info);

/**
 * ubi_get_dev_info1 - get UBI device information.
 * @desc: UBI library descriptor
 * @dev_num: UBI device number to fetch information about
 * @info: pointer to the &struct ubi_dev_info object to fill
 *
 * This function is identical to 'ubi_get_dev_info()' except that it accepts UBI
 * device number, not UBI character device.
 */
int ubi_get_dev_info1(libubi_t desc, int dev_num, struct ubi_dev_info *info);

/**
 * ubi_get_vol_info - get UBI volume information.
 * @desc: UBI library descriptor
 * @node: name of the UBI volume character device to fetch information about
 * @info: pointer to the &struct ubi_vol_info object to fill
 *
 * This function fills the passed @info object with UBI volume information and
 * returns %0 in case of success and %-1 in case of failure.
 */
int ubi_get_vol_info(libubi_t desc, const char *node,
		     struct ubi_vol_info *info);

/**
 * ubi_get_vol_info1 - get UBI volume information.
 * @desc: UBI library descriptor
 * @dev_num: UBI device number
 * @vol_id: ID of the UBI volume to fetch information about
 * @info: pointer to the &struct ubi_vol_info object to fill
 *
 * This function is identical to 'ubi_get_vol_info()' except that it accepts UBI
 * volume number, not UBI volume character device.
 */
int ubi_get_vol_info1(libubi_t desc, int dev_num, int vol_id,
		      struct ubi_vol_info *info);

/**
 * ubi_update_start - start UBI volume update.
 * @desc: UBI library descriptor
 * @fd: volume character devie file descriptor
 * @bytes: how many bytes will be written to the volume
 *
 * This function initiates UBI volume update and returns %0 in case of success
 * and %-1 in case of error. The caller is assumed to write @bytes data to the
 * volume @fd afterwards.
 */
int ubi_update_start(libubi_t desc, int fd, long long bytes);

/**
 * ubi_leb_change_start - start atomic LEB change.
 * @desc: UBI library descriptor
 * @fd: volume character devie file descriptor
 * @lnum: LEB number to change
 * @bytes: how many bytes of new data will be written to the LEB
 * @dtype: data type (%UBI_LONGTERM, %UBI_SHORTTERM, %UBI_UNKNOWN)
 *
 * This function initiates atomic LEB change operation and returns %0 in case
 * of success and %-1 in case of error. he caller is assumed to write @bytes
 * data to the volume @fd afterwards.
 */
int ubi_leb_change_start(libubi_t desc, int fd, int lnum, int bytes, int dtype);

#ifdef __cplusplus
}
#endif

#endif /* !__LIBUBI_H__ */
