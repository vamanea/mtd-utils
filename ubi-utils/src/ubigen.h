#ifndef __UBIGEN_H__
#define __UBIGEN_H__
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
 * Author: Frank Haverkamp
 *
 * An utility to update UBI volumes.
 */

#include <stdio.h> /* FILE */
#include <stdint.h>
#include <mtd/ubi-media.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_BLOCKSIZE	(128 * 1024)
#define DEFAULT_PAGESIZE	(2*1024)

#define EUBIGEN_INVALID_TYPE		1
#define EUBIGEN_INVALID_HDR_OFFSET	2
#define EUBIGEN_INVALID_ALIGNMENT	3
#define EUBIGEN_TOO_SMALL_EB		4
#define EUBIGEN_MAX_ERROR		5


typedef enum action {
	NO_ERROR	 = 0x00000000,
	BROKEN_HDR_CRC	 = 0x00000001,
	BROKEN_DATA_CRC	 = 0x00000002,
	BROKEN_DATA_SIZE = 0x00000004,
	BROKEN_OMIT_BLK	 = 0x00000008,
	MARK_AS_UPDATE	 = 0x00000010,
} ubigen_action_t;

typedef struct ubi_info *ubi_info_t;

/**
 * @brief	  Initialize the internal CRC32 table.
 * @note	  Necessary because of the used crc32 function in UBI.
 *		  A usage of CRC32, from e.g. zlib will fail.
 */
void ubigen_init(void);

/**
 * @brief	  Create an ubigen handle.
 * @param  ...
 * @return 0	  On sucess.
 *	   else	  Error.
 * @note	  This parameterlist is ugly. But we have to use
 *		  two big structs and meta information internally,
 *		  filling them would be even uglier.
 */
int ubigen_create(ubi_info_t *u, uint32_t vol_id, uint8_t vol_type,
		  uint32_t eb_size, uint64_t ec, uint32_t alignment,
		  uint8_t version, uint32_t vid_hdr_offset,
		  uint8_t compat_flag, size_t data_size,
		  FILE* fp_in, FILE* fp_out);

/**
 * @brief	  Destroy an ubigen handle.
 * @param  u	  Handle to free.
 * @return 0	  On success.
 *	   else	  Error.
 */
int ubigen_destroy(ubi_info_t *u);

/**
 * @brief	  Get number of total logical EBs, necessary for the
 *		  complete storage of data in the handle.
 * @param  u	  The handle.
 * @return 0	  On success.
 *	   else	  Error.
 */
int ubigen_get_leb_total(ubi_info_t u, size_t* total);

/**
 * @brief	  Get the size in bytes of one logical EB in the handle.
 * @param  u	  The handle.
 * @return 0	  On success.
 *	   else	  Error.
 */
int ubigen_get_leb_size(ubi_info_t u, size_t* size);


/**
 * @brief	  Write a logical EB (fits exactly into 1 physical EB).
 * @param  u	  Handle which holds all necessary data.
 * @param  action Additional operations which shall be applied on this
 *		  logical eraseblock. Mostly injecting artifical errors.
 * @return 0	  On success.
 *	   else	  Error.
 */
int ubigen_write_leb(ubi_info_t u, ubigen_action_t action);

/**
 * @brief	  Write a complete array of logical eraseblocks at once.
 * @param  u	  Handle which holds all necessary data.
 * @return 0	  On success.
 *	   else	  Error.
 */
int ubigen_write_complete(ubi_info_t u);

/**
 * @brief	  Write a single block which is extracted from the
 *		  binary input data.
 * @param  u	  Handle which holds all necessary data.
 * @param  blk	  Logical eraseblock which shall hold a inc. copy entry
 *		  and a bad data crc.
 * @return 0	  On success.
 *	   else	  Error.
 */
int ubigen_write_broken_update(ubi_info_t u, uint32_t blk);

/**
 * @brief	  Use the current ubi_info data and some additional data
 *		  to set an UBI volume table entry from it.
 * @param  u	  Handle which holds some of the necessary data.
 * @param  res_bytes Number of reserved bytes which is stored in the volume
 *		     table entry.
 * @param  name	   A string which shall be used as a volume label.
 * @param  lvol_r  A pointer to a volume table entry.
 * @return 0	   On success.
 *	   else	   Error.
 */
int ubigen_set_lvol_rec(ubi_info_t u, size_t reserved_bytes,
		const char* name, struct ubi_vtbl_record *lvol_rec);

#ifdef __cplusplus
}
#endif

#endif /* __UBIGEN_H__ */
