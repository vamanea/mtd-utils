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
 * Author: Thomas Gleixner
 */

/*
 * CRC32 functions
 *
 * Can be compiled as seperate object, but is included into the ipl source
 * so gcc can inline the functions. We optimize for size so the omission of
 * the function frame is helpful.
 *
 */

#include <stdint.h>
#include <crc32.h>

/* CRC polynomial */
#define CRC_POLY	0xEDB88320

/**
 * init_crc32_table - Initialize crc table
 *
 * @table:	pointer to the CRC table which must be initialized
 *
 * Create CRC32 table for given polynomial. The table is created with
 * the lowest order term in the highest order bit. So the x^32 term
 * has to implied in the crc calculation function.
 */
void init_crc32_table(uint32_t *table)
{
	uint32_t crc;
	int i, j;

	for (i = 0; i < 256; i++) {
		crc = i;
		for (j = 8; j > 0; j--) {
			if (crc & 1)
				crc = (crc >> 1) ^ CRC_POLY;
			else
				crc >>= 1;
		}
		table[i] = crc;
	}
}

/**
 * clc_crc32 - Calculate CRC32 over a buffer
 *
 * @table:	pointer to the CRC table
 * @crc:	initial crc value
 * @buf:	pointer to the buffer
 * @len:	number of bytes to calc
 *
 * Returns the updated crc value.
 *
 * The algorithm resembles a hardware shift register, but calculates 8
 * bit at once.
 */
uint32_t clc_crc32(uint32_t *table, uint32_t crc, void *buf,
		   int len)
{
	const unsigned char *p = buf;

	while(--len >= 0)
		crc = table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
	return crc;
}
