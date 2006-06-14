#ifndef _UBI_IPL_H
#define _UBI_IPL_H
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
 * Constants calculated from the CFG_XXX defines
 *
 * Declaration of the loader function which is invoked by the
 * assembler part of the IPL
 */

/* Size of IPL - is 4K for NAND and can also be 4K for NOR */
#define IPL_SIZE	   4096

/* Needed in asm code to upload the data, needed in C-code for CRC32 */
#define IPL_RAMADDR	   (CFG_MEMTOP - IPL_SIZE)

#if !defined(__ASSEMBLY__)

#include <stdint.h>
#include <mtd/ubi-header.h>

/* Address of the flash info structure */
#define FINFO_ADDR (struct ubi_scan_info *) (CFG_MEMTOP - CFG_IPLSIZE * 1024)

/* Size of the flash info structure */
#define FINFO_SIZE sizeof(struct ubi_scan_info)

/* Blockinfo array address */
#define BINFO_ADDR (struct ubi_vid_hdr *) ((void *)FINFO_ADDR + FINFO_SIZE)

/* Number of erase blocks */
#define NR_ERASE_BLOCKS	((CFG_FLASHSIZE * 1024) / CFG_BLOCKSIZE)

/* Blockinfo size */
#define BINFO_SIZE (NR_ERASE_BLOCKS * sizeof(struct ubi_vid_hdr))

/* Images array address */
#define IMAGES_ADDR (struct ubi_vid_hdr **) ((void *)BINFO_ADDR + BINFO_SIZE)

/* Images array size */
#define IMAGES_SIZE (NR_ERASE_BLOCKS * sizeof(unsigned int))

/* Total size of flash info + blockinfo + images */
#define INFO_SIZE ((FINFO_SIZE + BINFO_SIZE + IMAGES_SIZE) / sizeof(uint32_t))

/* Load address of the SPL */
#define SPL_ADDR (void *) ((void *)FINFO_ADDR - CFG_SPLCODE * 1024)

#define IPL_SIZE_CRC32	   (IPL_SIZE - sizeof(uint32_t))
#define IPL_RAMADDR_CRC32  ((void *)(IPL_RAMADDR + sizeof(uint32_t)))

/*
 * Linker script magic to ensure that load_spl() is linked to the
 * right place
 */
#define __crc32	 __attribute__((__section__(".crc32")))
#define __entry	 __attribute__((__section__(".entry.text")))
#define __unused __attribute__((unused))

#define MIN(x,y) ((x)<(y)?(x):(y))

#define stop_on_error(x) \
	{ while (1); }

void __entry load_spl(void);
void hardware_init(void);

#endif /* __ASSEMBLY__ */

#endif
