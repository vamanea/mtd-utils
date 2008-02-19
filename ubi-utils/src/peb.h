#ifndef __RAW_BLOCK_H__
#define __RAW_BLOCK_H__
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
 */

#include <stdint.h>
#include <stdio.h>

typedef struct peb *peb_t;
struct peb {
	uint32_t num;		/* Physical eraseblock number
				 * in the RAW file. */
	uint32_t size;		/* Data Size (equals physical
				 * erase block size) */
	uint8_t* data;		/* Data buffer */
};

int  peb_new(uint32_t peb_num, uint32_t peb_size, peb_t* peb);
int  peb_free(peb_t* peb);
int  peb_cmp(peb_t peb_1, peb_t peb_2);
int  peb_write(FILE* fp_out, peb_t peb);
void peb_dump(FILE* fp_out, peb_t peb);

#endif /* __RAW_BLOCK_H__ */
