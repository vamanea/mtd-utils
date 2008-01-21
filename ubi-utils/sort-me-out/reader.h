#ifndef __READER_H__
#define __READER_H__
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
 * Author: Oliver Lohmann
 *
 * Read Platform Description Data (PDD).
 */

#include <stdint.h>
#include <stdio.h>

#include "pfi.h"
#include "bootenv.h"
#include "list.h"

typedef enum flash_type_t {
	NAND_FLASH = 0,
	NOR_FLASH,
} flash_type_t;

typedef struct pdd_data *pdd_data_t;
typedef struct pfi_raw	*pfi_raw_t;
typedef struct pfi_ubi	*pfi_ubi_t;

struct pdd_data {
	uint32_t flash_size;
	uint32_t flash_page_size;
	uint32_t eb_size;
	uint32_t vid_hdr_offset;
	flash_type_t flash_type;
};

struct pfi_raw {
	uint32_t data_size;
	uint32_t *starts;
	uint32_t starts_size;
	uint32_t crc;
};

struct pfi_ubi {
	uint32_t data_size;
	uint32_t alignment;
	uint32_t *ids;
	uint32_t ids_size;
	char	 **names;
	uint32_t names_size;
	uint32_t size;
	enum { pfi_ubi_dynamic, pfi_ubi_static } type;
	int curr_seqnum; /* specifies the seqnum taken in an update,
			    default: 0 (used by pfiflash, ubimirror) */
	uint32_t crc;
};

int read_pdd_data(FILE* fp_pdd, pdd_data_t *pdd_data,
		char *err_buf, size_t err_buf_size);
int read_pfi_raw(pfi_header pfi_hd, FILE* fp_pfi, pfi_raw_t *pfi_raw,
		const char *label, char *err_buf, size_t err_buf_size);
int read_pfi_ubi(pfi_header pfi_hd, FILE* fp_pfi, pfi_ubi_t *pfi_ubi,
		const char *label, char *err_buf, size_t err_buf_size);

/**
 * @brief Reads all pfi headers into list structures, separated by
 *	  RAW and UBI sections.
 */
int read_pfi_headers(list_t *pfi_raws, list_t *pfi_ubis, FILE* fp_pfi,
		char* err_buf, size_t err_buf_size);
int free_pdd_data(pdd_data_t *pdd_data);
int free_pfi_raw(pfi_raw_t *raw_pfi);
int free_pfi_ubi(pfi_ubi_t *pfi_ubi);

#endif /* __READER_H__ */
