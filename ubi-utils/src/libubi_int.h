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

#ifndef __LIBUBI_INT_H__
#define __LIBUBI_INT_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * UBI heavily makes use of the sysfs file system to interact with users-pace.
 * The below are pre-define UBI file and directory names.
 */

#define SYSFS_UBI         "class/ubi"
#define UBI_DEV_NAME_PATT "ubi%d"
#define UBI_VER           "version"
#define DEV_DEV           "dev"
#define UBI_VOL_NAME_PATT "ubi%d_%d"
#define DEV_AVAIL_EBS     "avail_eraseblocks"
#define DEV_TOTAL_EBS     "total_eraseblocks"
#define DEV_BAD_COUNT     "bad_peb_count"
#define DEV_EB_SIZE       "eraseblock_size"
#define DEV_MAX_EC        "max_ec"
#define DEV_MAX_RSVD      "reserved_for_bad"
#define DEV_MAX_VOLS      "max_vol_count"
#define DEV_MIN_IO_SIZE   "min_io_size"
#define VOL_TYPE          "type"
#define VOL_DEV           "dev"
#define VOL_ALIGNMENT     "alignment"
#define VOL_DATA_BYTES    "data_bytes"
#define VOL_RSVD_EBS      "reserved_ebs"
#define VOL_EB_SIZE       "usable_eb_size"
#define VOL_CORRUPTED     "corrupted"
#define VOL_NAME          "name"

/**
 * libubi - UBI library description data structure.
 *
 * @sysfs            sysfs file system path
 * @sysfs_ubi        UBI directory in sysfs
 * @ubi_dev          UBI device sysfs directory pattern
 * @ubi_version      UBI version file sysfs path
 * @dev_dev          UBI device's major/minor numbers file pattern
 * @dev_avail_ebs    count of available eraseblocks sysfs path pattern
 * @dev_total_ebs    total eraseblocks count sysfs path pattern
 * @dev_bad_count    count of bad eraseblocks sysfs path pattern
 * @dev_eb_size      size of UBI device's eraseblocks sysfs path pattern
 * @dev_max_ec       maximum erase counter sysfs path pattern
 * @dev_bad_rsvd     count of physical eraseblock reserved for bad eraseblocks
 *                   handling
 * @dev_max_vols     maximum volumes number count sysfs path pattern
 * @dev_min_io_size  minimum I/O unit size sysfs path pattern
 * @ubi_vol          UBI volume sysfs directory pattern
 * @vol_type         volume type sysfs path pattern
 * @vol_dev          volume's major/minor numbers file pattern
 * @vol_alignment    volume alignment sysfs path pattern
 * @vol_data_bytes   volume data size sysfs path pattern
 * @vol_rsvd_ebs     volume reserved size sysfs path pattern
 * @vol_eb_size      volume eraseblock size sysfs path pattern
 * @vol_corrupted    volume corruption flag sysfs path pattern
 * @vol_name         volume name sysfs path pattern
 */
struct libubi
{
	char *sysfs;
	char *sysfs_ubi;
	char *ubi_dev;
	char *ubi_version;
	char *dev_dev;
	char *dev_avail_ebs;
	char *dev_total_ebs;
	char *dev_bad_count;
	char *dev_eb_size;
	char *dev_max_ec;
	char *dev_bad_rsvd;
	char *dev_max_vols;
	char *dev_min_io_size;
	char *ubi_vol;
	char *vol_type;
	char *vol_dev;
	char *vol_alignment;
	char *vol_data_bytes;
	char *vol_rsvd_ebs;
	char *vol_eb_size;
	char *vol_corrupted;
	char *vol_name;
	char *vol_max_count;
};

static int read_int(const char *file, int *value);
static int dev_read_int(const char *patt, int dev_num, int *value);
static int dev_read_ll(const char *patt, int dev_num, long long *value);
static int dev_read_data(const char *patt, int dev_num, void *buf, int buf_len);
static int vol_read_int(const char *patt, int dev_num, int vol_id, int *value);
static int vol_read_ll(const char *patt, int dev_num, int vol_id,
		       long long *value);
static int vol_read_data(const char *patt, int dev_num, int vol_id, void *buf,
			 int buf_len);
static char *mkpath(const char *path, const char *name);
static int find_dev_num(struct libubi *lib, const char *node);
static int find_dev_num_vol(struct libubi *lib, const char *node);
static int find_vol_num(struct libubi *lib, int dev_num, const char *node);

#ifdef __cplusplus
}
#endif

#endif /* !__LIBUBI_INT_H__ */
