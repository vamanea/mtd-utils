#ifndef _UBI_FLASH_H
#define _UBI_FLASH_H
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
 * FLASH related data structures and constants for UBI.
 * UBI scan analysis.
 *
 * IPL Initial Program Loader
 * SPL Secondary Program Loader
 */

#include <stdint.h>
#include <asm/byteorder.h>
#include <mtd/ubi-header.h>

#define UBI_BLOCK_IDENT_MAX  16

/* Block status information constants */
enum blockstat {
	/* IO Error */
	STAT_IO_FAILED	= 1,	/* 0xffffffff */
	/* Block is bad */
	STAT_BLOCK_BAD	= 2,	/* 0xfffffffe */
	/* ECC unrecoverable error */
	STAT_ECC_ERROR	= 3,	/* 0xfffffffd */
	/* CRC checksum failed */
	STAT_CRC_ERROR	= 4,	/* 0xfffffffc */
	/* Magic number not available */
	STAT_NO_MAGIC	= 5,	/* 0xfffffffb */
	/* No image available */
	STAT_NO_IMAGE	= 6,
	/* Image is invalid */
	STAT_INVALID_IMAGE	= 7,
	/* Image is defect */
	STAT_DEFECT_IMAGE	= 8,
};

/*
 * Flash types
 */
enum flashtypes {
	FLASH_TYPE_NAND = 1,
	FLASH_TYPE_NOR,
};

/* Nand read buffer size: 2KiB + 64byte spare */
#define NAND_READ_BUF_SIZE	(2048 + 64)

/* Size of the CRC table */
#define CRC32_TABLE_SIZE	256

/* Image is not available marker for image offs */
#define UBI_IMAGE_NOT_AVAILABLE	0xFFFFFFFF

/* Increment this number, whenever you change the structure */
#define UBI_SCAN_INFO_VERSION	2

/* Time measurement as far as the code size allows us to do this */
#define UBI_TIMESTAMPS		16

/**
 * struct ubi_scan_info - RAM table filled by IPL scan
 *
 * @version:		Version of the structure
 * @bootstatus:		Boot status of the current boot
 * @flashtype:		Flash type (NAND/NOR)
 * @flashid:		ID of the flash chip
 * @flashmfr:		Manufacturer ID of the flash chip
 * @flashsize:		Size of the FLASH
 * @blocksize:		Eraseblock size
 * @blockshift:		Shift count to calc block number from offset
 * @nrblocks:		Number of erase blocks on flash
 * @pagesize:		Pagesize (NAND)
 * @blockinfo:		Pointer to an array of block status information
 *			filled by FLASH scan
 * @images:		Pointer to FLASH block translation table sorted
 *			by image type and load order
 * @imageblocks:	Number of blocks found per image
 * @imageoffs:		Offset per imagetype to the first
 *			block in the translation table
 * @imagedups		duplicate blocks (max. one per volume)
 * @imagelen:		Length of the loaded image
 * @crc32_table:	CRC32 table buffer
 * @page_buf:		Page buffer for NAND FLASH
 */
struct ubi_scan_info {
	int			version;
	unsigned int		bootstatus;
	unsigned int		flashtype;
	unsigned int		flashid;
	unsigned int		flashmfr;
	unsigned int		flashsize;
	unsigned int		blocksize;
	unsigned int		blockshift;
	unsigned int		nrblocks;
	unsigned int		pagesize;

	struct ubi_vid_hdr	*blockinfo;
	struct ubi_vid_hdr	**images;
	unsigned int		imageblocks[UBI_BLOCK_IDENT_MAX];
	unsigned int		imageoffs[UBI_BLOCK_IDENT_MAX];
	struct ubi_vid_hdr	*imagedups[UBI_BLOCK_IDENT_MAX];
	unsigned int		imagelen;
	uint32_t		crc32_table[CRC32_TABLE_SIZE];
	uint8_t			page_buf[NAND_READ_BUF_SIZE];
	unsigned int		times[UBI_TIMESTAMPS];
};

/* External function definition */
extern int flash_read(void *buf, unsigned int offs, int len);
extern int flash_read_slice(struct ubi_scan_info *fi, void *buf,
			    unsigned int offs, int len);
extern void ipl_main(struct ubi_scan_info *fi);

#ifndef CFG_EXAMPLE_IPL
extern int ipl_scan(struct ubi_scan_info *fi);
extern int ipl_load(struct ubi_scan_info *fi, int nr, uint8_t *loadaddr);

#define IPL_STATIC

#else
#define IPL_STATIC static
#endif

/**
 * get_boot_status - get the boot status register
 *
 * Shift the lower 16 bit into the upper 16 bit and return
 * the result.
 */
uint32_t get_boot_status(void);

/**
 * set_boot_status - Set the boot status register
 *
 * @status:	The status value to set
 *
 */
void set_boot_status(uint32_t status);

static inline unsigned int ubi_vid_offset(struct ubi_scan_info *fi)
{
	if (fi->flashtype == FLASH_TYPE_NOR)
		return UBI_EC_HDR_SIZE;
	else
		return fi->pagesize - UBI_VID_HDR_SIZE;
}

static inline unsigned int ubi_data_offset(struct ubi_scan_info *fi)
{
	if (fi->flashtype == FLASH_TYPE_NOR)
		return UBI_EC_HDR_SIZE + UBI_VID_HDR_SIZE;
	else
		return fi->pagesize;
}

/**
 * IPL checkpoints
 */
#define CHKP_HWINIT		0x3030
#define CHKP_IPLSCAN_FAILED	0x3034
#define CHKP_SPL_START		0x3037
#define CHKP_SPLLOAD_STATUS	0x3130

extern void checkpoint(uint32_t cpoint);
extern void switch_watchdog(void);

#endif
