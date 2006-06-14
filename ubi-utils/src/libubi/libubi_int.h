#ifndef __UBI_INT_H__
#define __UBI_INT_H__
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
 */

/*
 * Enable/disable UBI library debugging messages.
 */
#undef UBILIB_DEBUG

/*
 * UBI library error message.
 */
#define ubi_err(fmt, ...) do {						\
		fprintf(stderr, "UBI Library Error at %s: ", __func__); \
		fprintf(stderr, fmt, ##__VA_ARGS__);			\
		fprintf(stderr, "\n");					\
	} while(0)

#ifdef UBILIB_DEBUG
#define ubi_dbg(fmt, ...) do {						\
		fprintf(stderr, "UBI Debug: %s: ", __func__);		\
		fprintf(stderr, fmt, ##__VA_ARGS__);			\
		fprintf(stderr, "\n");					\
	} while(0)

#else
#define ubi_dbg(fmt, ...)
#endif

/**
 * SYSFS Entries.
 *
 * @def UBI_ROOT
 *	@brief Name of the root UBI directory in sysfs.
 *
 * @def UBI_NLEN_MAX
 *	@brief Name of syfs file containing the maximum UBI volume name length.
 *
 * @def UBI_VERSION
 *      @brief Name of sysfs file containing UBI version.
 *
 * @def UBI_WEAR
 *	@brief Name of sysfs file containing wear level of an UBI device.
 *
 * @def UBI_VOL_COUNT
 *	@brief Name of sysfs file contaning the of volume on an UBI device
 *
 * @def UBI_TOT_EBS
 *	@brief Name of sysfs file contaning the total number of
 *	eraseblocks on an UBI device.
 *
 * @def UBI_AVAIL_EBS
 *	@brief Name of sysfs file contaning the number of unused eraseblocks on
 *	an UBI device.
 *
 * @def UBI_EB_SIZE
 *	@brief Name of sysfs file containing size of UBI eraseblocks.
 *
 * @def UBI_NUMS
 *      @brief Name of sysfs file containing major and minor numbers
 *      of an UBI device or an UBI volume device.
 *
 * @def UBI_VBYTES
 *	@brief Name of sysfs file containing size of an UBI volume device in
 *	bytes.
 *
 * @def UBI_VEBS
 *	@brief Name of sysfs file containing size of an UBI volume device in
 *	eraseblocks.
 *
 * @def UBI_VTYPE
 *	@brief Name of sysfs file containing type of an UBI volume device.
 *
 * @def UBI_VNAME
 *	@brief Name of sysfs file containing name of an UBI volume device.
 **/
#define UBI_ROOT	"ubi"
#define UBI_NLEN_MAX	"volume_name_max"
#define UBI_VERSION	"version"
#define UBI_WEAR	"wear"
#define UBI_VOL_COUNT	"volumes_count"
#define UBI_TOT_EBS	"total_eraseblocks"
#define UBI_AVAIL_EBS	"avail_eraseblocks"
#define UBI_EB_SIZE	"eraseblock_size"
#define UBI_NUMS	"dev"
#define UBI_VBYTES	"bytes"
#define UBI_VEBS	"eraseblocks"
#define UBI_VTYPE	"type"
#define UBI_VNAME	"name"

#define UBI_CDEV_PATH	"/dev/ubi%d"
#define UBI_VOL_CDEV_PATH "/dev/ubi%d_%d"
#define UBI_SYSFS_ROOT	"/sys/class"

#define UBI_MAX_ID_SIZE	9

#endif /* !__UBI_INT_H__ */
