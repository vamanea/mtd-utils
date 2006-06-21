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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Authors: Artem B. Bityutskiy
 *          Thomas Gleixner
 *          Frank Haverkamp
 *          Oliver Lohmann
 *          Andreas Arnez
 */

/*
 * This file defines the layout of UBI headers and all the other UBI on-flash
 * data structures.
 */

#ifndef __UBI_HEADER_H__
#define __UBI_HEADER_H__

#include <asm/byteorder.h>

/* The version of this UBI implementation */
#define UBI_VERSION 1

/* The highest erase counter value supported by this implementation of UBI */
#define UBI_MAX_ERASECOUNTER 0x7FFFFFFF

/* The initial CRC32 value used when calculating CRC checksums */
#define UBI_CRC32_INIT 0xFFFFFFFFU

/**
 * Magic numbers of the UBI headers.
 *
 * @UBI_EC_HDR_MAGIC: erase counter header magic number (ASCII "UBI#")
 * @UBI_VID_HDR_MAGIC: volume identifier header magic number (ASCII "UBI!")
 */
enum {
	UBI_EC_HDR_MAGIC  = 0x55424923,
	UBI_VID_HDR_MAGIC = 0x55424921
};

/**
 * Molume type constants used in volume identifier headers.
 *
 * @UBI_VID_DYNAMIC: dynamic volume
 * @UBI_VID_STATIC: static volume
 */
enum {
	UBI_VID_DYNAMIC = 1,
	UBI_VID_STATIC  = 2
};

/**
 * Compatibility constants used by internal volumes.
 *
 * @UBI_COMPAT_DELETE: delete this internal volume before anything is written
 * to the flash
 * @UBI_COMPAT_RO: attach this device in read-only mode
 * @UBI_COMPAT_IGNORE: ignore this internal volume, but the UBI wear-leveling
 * unit may still move these logical eraseblocks to ensure wear-leveling
 * @UBI_COMPAT_PRESERVE: preserve this internal volume - do not touch its
 * physical eraseblocks, don't even allow the wear-leveling unit to move
 * them
 * @UBI_COMPAT_REJECT: reject this UBI image
 */
enum {
	UBI_COMPAT_DELETE   = 1,
	UBI_COMPAT_RO       = 2,
	UBI_COMPAT_IGNORE   = 3,
	UBI_COMPAT_PRESERVE = 4,
	UBI_COMPAT_REJECT   = 5
};

/*
 * ubi16_t/ubi32_t/ubi64_t - 16, 32, and 64-bit integers used in UBI on-flash
 * data structures.
 */
typedef struct {
	uint16_t int16;
} __attribute__ ((packed)) ubi16_t;

typedef struct {
	uint32_t int32;
} __attribute__ ((packed)) ubi32_t;

typedef struct {
	uint64_t int64;
} __attribute__ ((packed)) ubi64_t;

/*
 * In this implementation UBI uses the big-endian format for on-flash integers.
 * The below are the corresponding endianess conversion macros.
 */
#define cpu_to_ubi16(x) ((ubi16_t){__cpu_to_be16(x)})
#define ubi16_to_cpu(x) ((uint16_t)__be16_to_cpu((x).int16))

#define cpu_to_ubi32(x) ((ubi32_t){__cpu_to_be32(x)})
#define ubi32_to_cpu(x) ((uint32_t)__be32_to_cpu((x).int32))

#define cpu_to_ubi64(x) ((ubi64_t){__cpu_to_be64(x)})
#define ubi64_to_cpu(x) ((uint64_t)__be64_to_cpu((x).int64))

/*
 * Sizes of UBI headers.
 */
#define UBI_EC_HDR_SIZE  sizeof(struct ubi_ec_hdr)
#define UBI_VID_HDR_SIZE sizeof(struct ubi_vid_hdr)

/*
 * Sizes of UBI headers without the ending CRC.
 */
#define UBI_EC_HDR_SIZE_CRC  (UBI_EC_HDR_SIZE  - sizeof(ubi32_t))
#define UBI_VID_HDR_SIZE_CRC (UBI_VID_HDR_SIZE - sizeof(ubi32_t))

/*
 * How much private data may internal volumes store in the VID header.
 */
#define UBI_VID_HDR_IVOL_DATA_SIZE 12

/**
 * struct ubi_ec_hdr - UBI erase counter header.
 *
 * @magic: the erase counter header magic number (%UBI_EC_HDR_MAGIC)
 * @version: the version of UBI implementation which is supposed to accept this
 * UBI image (%UBI_VERSION)
 * @padding1: reserved for future, zeroes
 * @ec: the erase counter
 * @vid_hdr_offset: where the VID header begins
 * @data_offset: where the user data begins
 * @padding2: reserved for future, zeroes
 * @hdr_crc: the erase counter header CRC checksum
 *
 * The erase counter header takes 64 bytes and has a plenty of unused space for
 * future usage. The unused fields are zeroed. The @version field is used to
 * indicate the version of UBI implementation which is supposed to be able to
 * work with this UBI image. If @version is greater then the current UBI
 * version, the image is rejecter. This may be useful in future if something
 * is changed radically. This field is duplicated in the volume identifier
 * header.
 *
 * The @vid_hdr_offset and @data_offset fields contain the offset of the the
 * volume identifier header and user data, relative to the beginning of the
 * eraseblock. These values have to be the same for all eraseblocks.
 */
struct ubi_ec_hdr {
	ubi32_t magic;
	uint8_t version;
	uint8_t padding1[3];
	ubi64_t ec; /* Warning: the current limit is 31-bit anyway! */
	ubi32_t vid_hdr_offset;
	ubi32_t data_offset;
	uint8_t padding2[36];
	ubi32_t hdr_crc;
} __attribute__ ((packed));

/**
 * struct ubi_vid_hdr - on-flash UBI volume identifier header.
 *
 * @magic: volume identifier header magic number (%UBI_VID_HDR_MAGIC)
 * @version: UBI implementation version which is supposed to accept this UBI
 * image (%UBI_VERSION)
 * @vol_type: volume type (%UBI_VID_DYNAMIC or %UBI_VID_STATIC)
 * @copy_flag: a flag indicating if this physical eraseblock was created by
 * means of copying an original physical eraseblock to ensure wear-leveling.
 * @compat: compatibility of this volume (%UBI_COMPAT_DELETE,
 * %UBI_COMPAT_IGNORE, %UBI_COMPAT_PRESERVE, or %UBI_COMPAT_REJECT)
 * @vol_id: volume ID
 * @lnum: logical eraseblock number
 * @leb_ver: eraseblock copy number
 * @data_size: how many bytes of data this eraseblock contains.
 * @used_ebs: total number of used logical eraseblocks in this volume
 * @data_pad: how many bytes at the end of this eraseblock are not used
 * @data_crc: CRC checksum of data containing in this eraseblock
 * @padding1: reserved for future, zeroes
 * @ivol_data: private data of internal volumes
 * @hdr_crc: volume identifier header CRC checksum
 *
 * The @leb_ver and the @copy_flag fields are used to distinguish between older
 * and newer copies of logical eraseblocks, as well as to guarantee robustness
 * to unclean reboots. As UBI erases logical eraseblocks asynchronously, it has
 * to distinguish between older and newer copies of eraseblocks. This is done
 * using the @version field. On the other hand, when UBI moves an eraseblock,
 * its version is also increased and the @copy_flag is set to 1. Additionally,
 * when moving eraseblocks, UBI calculates data CRC and stores it in the
 * @data_crc field, even for dynamic volumes.
 *
 * Thus, if there are 2 eraseblocks of the same volume and logical number, UBI
 * uses the following algorithm to pick one of them. It first picks the one
 * with larger version (say, A). If @copy_flag is not set, then A is picked. If
 * @copy_flag is set, UBI checks the CRC of the eraseblock (@data_crc). This is
 * needed to ensure that copying was finished. If the CRC is all right, A is
 * picked. If not, the older eraseblock is picked.
 *
 * Note, the @leb_ver field may overflow. Thus, if you have 2 versions A and B,
 * then A > B if abs(A-B) < 0x7FFFFFFF, and A < B otherwise.
 *
 * There are 2 sorts of volumes in UBI: user volumes and internal volumes.
 * Internal volumes are not seen from outside and are used for different
 * internal UBI purposes. In this implementation there are only two internal
 * volumes: the layout volume and the update volume. Internal volumes are the
 * main mechanism of UBI extensions. For example, in future one may introduce a
 * journal internal volume.
 *
 * The @compat field is only used for internal volumes and contains the degree
 * of their compatibility. This field is always zero for user volumes. This
 * field provides a mechanism to introduce UBI extensions and to be still
 * compatible with older UBI binaries. For example, if someone introduced an
 * journal internal volume in future, he would probably use %UBI_COMPAT_DELETE
 * compatibility.  And in this case, older UBI binaries, which know nothing
 * about the journal volume, would just delete this and work perfectly fine.
 * This is somewhat similar to what Ext2fs does when it is fed by an Ext3fs
 * image - it just ignores the Ext3fs journal.
 *
 * The @data_crc field contains the CRC checksum of the contents of the logical
 * eraseblock if this is a static volume.  In case of dynamic volumes, it does
 * not contain the CRC checksum as a rule. The only exception is when the
 * logical eraseblock was moved by the wear-leveling unit, then the
 * wear-leveling unit calculates the eraseblocks' CRC and stores it at
 * @data_crc.
 *
 * The @data_size field is always used for static volumes because we want to
 * know about how many bytes of data are stored in this eraseblock.  For
 * dynamic eraseblocks, this field usually contains zero. The only exception is
 * when the logical eraseblock is moved to another physical eraseblock due to
 * wear-leveling reasons. In this case, UBI calculates CRC checksum of the
 * contents and uses both @data_crc and @data_size fields. In this case, the
 * @data_size field contains the size of logical eraseblock of this volume
 * (which may vary owing to @alignment).
 *
 * The @used_ebs field is used only for static volumes and indicates how many
 * eraseblocks the data of the volume takes. For dynamic volumes this field is
 * not used and always contains zero.
 *
 * The @data_pad is calculated when volumes are created using the alignment
 * parameter. So, effectively, the @data_pad field reduces the size of logical
 * eraseblocks of this volume. This is very handy when one uses block-oriented
 * software (say, cramfs) on top of the UBI volume.
 *
 * The @ivol_data contains private data of internal volumes. This might be very
 * handy to store data in the VID header, not in the eraseblock's contents. For
 * example it may make life of simple boot-loaders easier. The @ivol_data field
 * contains zeroes for user volumes.
 */
struct ubi_vid_hdr {
	ubi32_t magic;
	uint8_t version;
	uint8_t vol_type;
	uint8_t copy_flag;
	uint8_t compat;
	ubi32_t vol_id;
	ubi32_t lnum;
	ubi32_t leb_ver;
	ubi32_t data_size;
	ubi32_t used_ebs;
	ubi32_t data_pad;
	ubi32_t data_crc;
	uint8_t padding1[12];
	uint8_t ivol_data[UBI_VID_HDR_IVOL_DATA_SIZE];
	ubi32_t hdr_crc;
} __attribute__ ((packed));

/**
 * struct ubi_vid_hdr_upd_vol - private data of the update internal volume
 * stored in volume identifier headers.
 *
 * @vol_id: volume ID of the volume under update
 * @padding: zeroes
 */
struct ubi_vid_hdr_upd_vol {
	ubi32_t vol_id;
	uint8_t padding[UBI_VID_HDR_IVOL_DATA_SIZE - 4];
} __attribute__ ((packed));

/*
 * Count of internal UBI volumes.
 */
#define UBI_INT_VOL_COUNT 2

/*
 * Internal volume IDs start from this digit. There is a reserved room for 4096
 * internal volumes.
 */
#define UBI_INTERNAL_VOL_START (0x7FFFFFFF - 4096)

/**
 * enum ubi_internal_volume_numbers - volume IDs of internal UBI volumes.
 *
 * %UBI_LAYOUT_VOL_ID: volume ID of the layout volume
 * %UBI_UPDATE_VOL_ID: volume ID of the update volume
 */
enum ubi_internal_volume_ids {
	UBI_LAYOUT_VOL_ID = UBI_INTERNAL_VOL_START,
	UBI_UPDATE_VOL_ID = UBI_INTERNAL_VOL_START + 1
};

/*
 * Number of logical eraseblocks reserved for internal volumes.
 */
#define UBI_LAYOUT_VOLUME_EBS 2
#define UBI_UPDATE_VOLUME_EBS 1

/*
 * Names of internal volumes
 */
#define UBI_LAYOUT_VOLUME_NAME "The layout volume"
#define UBI_UPDATE_VOLUME_NAME "The update volume"

/*
 * Compatibility flags of internal volumes.
 */
#define UBI_LAYOUT_VOLUME_COMPAT UBI_COMPAT_REJECT
#define UBI_UPDATE_VOLUME_COMPAT UBI_COMPAT_REJECT

/*
 * The maximum number of volumes per one UBI device.
 */
#define UBI_MAX_VOLUMES 128

/*
 * The maximum volume name length.
 */
#define UBI_VOL_NAME_MAX 127

/*
 * Size of volume table records.
 */
#define UBI_VTBL_RECORD_SIZE sizeof(struct ubi_vol_tbl_record)

/*
 * Size of volume table records without the ending CRC.
 */
#define UBI_VTBL_RECORD_SIZE_CRC (UBI_VTBL_RECORD_SIZE - sizeof(ubi32_t))

/**
 * struct ubi_vol_tbl_record - a record in the volume table.
 *
 * @reserved_pebs: how many physical eraseblocks are reserved for this volume
 * @alignment: volume alignment
 * @data_pad: how many bytes are not used at the end of the eraseblocks to
 * satisfy the requested alignment
 * @padding1: reserved, zeroes
 * @name_len: the volume name length
 * @name: the volume name
 * @padding2: reserved, zeroes
 * @crc: a CRC32 checksum of the record
 *
 * The layout volume consists of 2 logical eraseblock, each of which contains
 * the volume table (i.e., the volume table is duplicated). The volume table is
 * an array of &struct ubi_vol_tbl_record objects indexed by the volume ID.
 *
 * If the size of the logical eraseblock is large enough to fit
 * %UBI_MAX_VOLUMES, the volume table contains %UBI_MAX_VOLUMES records.
 * Otherwise, it contains as much records as can be fit (i.e., size of logical
 * eraseblock divided by sizeof(struct ubi_vol_tbl_record)).
 *
 * The @alignment field is specified when the volume is created and cannot be
 * later changed. It may be useful, for example, when a block-oriented file
 * system works on top of UBI. The @data_pad field is calculated using the
 * logical eraseblock size and @alignment. The alignment must be multiple to the
 * minimal flash I/O unit. If @alignment is 1, all the available space of
 * eraseblocks is used.
 *
 * Empty records contain all zeroes and the CRC checksum of those zeroes.
 */
struct ubi_vol_tbl_record {
	ubi32_t reserved_pebs;
	ubi32_t alignment;
	ubi32_t data_pad;
	uint8_t vol_type;
	uint8_t padding1;
	ubi16_t name_len;
	uint8_t name[UBI_VOL_NAME_MAX + 1];
	uint8_t padding2[24];
	ubi32_t crc;
} __attribute__ ((packed));

#endif /* !__UBI_HEADER_H__ */
