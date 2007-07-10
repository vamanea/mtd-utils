#ifndef __PFIFLASH_H__
#define __PFIFLASH_H__
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

/**
 *
 * @file pfi.h
 *
 * @author Oliver Lohmann <oliloh@de.ibm.com>
 *
 * @brief The pfiflash library offers an interface for using the
 * pfiflash * utility.
 */

#include <stdio.h>		/* FILE */

#define PFIFLASH_MAX_ERR_BUF_SIZE 1024

#ifdef __cplusplus
extern "C" {
#endif

typedef enum pdd_handling_t
{
	PDD_KEEP = 0,
	PDD_MERGE,
	PDD_OVERWRITE,
	PDD_HANDLING_NUM, /* always the last item */
} pdd_handling_t; /**< Possible PDD handle algorithms. */

/**
 * @brief Flashes a PFI file to UBI Device 0.
 * @param complete	[0|1] Do a complete system update.
 * @param seqnum	Index in a redundant group.
 * @param compare	[0|1] Compare contents.
 * @param pdd_handling	The PDD handling algorithm.
 * @param rawdev	Device to use for raw flashing
 * @param err_buf	An error buffer.
 * @param err_buf_size	Size of the error buffer.
 */
int pfiflash_with_options(FILE* pfi, int complete, int seqnum, int compare,
		pdd_handling_t pdd_handling, const char* rawdev,
		char *err_buf, size_t err_buf_size);

/**
 * @brief Flashes a PFI file to UBI Device 0.
 * @param complete	[0|1] Do a complete system update.
 * @param seqnum	Index in a redundant group.
 * @param pdd_handling	The PDD handling algorithm.
 * @param err_buf	An error buffer.
 * @param err_buf_size	Size of the error buffer.
 */
int pfiflash(FILE* pfi, int complete, int seqnum, pdd_handling_t pdd_handling,
		char *err_buf, size_t err_buf_size);

#ifdef __cplusplus
}
#endif

#endif /* __PFIFLASH_H__ */
