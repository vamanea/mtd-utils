#ifndef __PFIFLASH_ERROR_H__
#define __PFIFLASH_ERROR_H__
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
 * Author: Drake Dowsett <dowsett@de.ibm.com>
 * Contact: Andreas Arnez <arnez@de.ibm.com>
 */

enum pfiflash_err {
	PFIFLASH_ERR_EOF = 1,
	PFIFLASH_ERR_FIO,
	PFIFLASH_ERR_UBI_OPEN,
	PFIFLASH_ERR_UBI_CLOSE,
	PFIFLASH_ERR_UBI_MKVOL,
	PFIFLASH_ERR_UBI_RMVOL,
	PFIFLASH_ERR_UBI_VOL_UPDATE,
	PFIFLASH_ERR_UBI_VOL_FOPEN,
	PFIFLASH_ERR_UBI_UNKNOWN,
	PFIFLASH_ERR_UBI_VID_OOB,
	PFIFLASH_ERR_BOOTENV_CREATE,
	PFIFLASH_ERR_BOOTENV_READ,
	PFIFLASH_ERR_BOOTENV_SIZE,
	PFIFLASH_ERR_BOOTENV_WRITE,
	PFIFLASH_ERR_PDD_UNKNOWN,
	PFIFLASH_ERR_MTD_OPEN,
	PFIFLASH_ERR_MTD_CLOSE,
	PFIFLASH_ERR_CRC_CHECK,
	PFIFLASH_ERR_MTD_ERASE,
	PFIFLASH_ERR_COMPARE,
	PFIFLASH_CMP_DIFF
};

const char *const PFIFLASH_ERRSTR[] = {
	"",
	"unexpected EOF",
	"file I/O error",
	"couldn't open UBI",
	"couldn't close UBI",
	"couldn't make UBI volume %d",
	"couldn't remove UBI volume %d",
	"couldn't update UBI volume %d",
	"couldn't open UBI volume %d",
	"unknown UBI operation",
	"PFI data contains out of bounds UBI id %d",
	"couldn't create bootenv%s",
	"couldn't read bootenv",
	"couldn't resize bootenv",
	"couldn't write bootenv on ubi%d_%d",
	"unknown PDD handling algorithm",
	"couldn't open MTD device %s",
	"couldn't close MTD device %s",
	"CRC check failed: given=0x%08x, calculated=0x%08x",
	"couldn't erase raw mtd region",
	"couldn't compare volumes",
	"on-flash data differ from pfi data, update is neccessary"
};

#endif /* __PFIFLASH_ERROR_H__ */
