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
 * ECC algorithm for NAND FLASH. Detects and corrects 1 bit errors in
 * a 256 bytes of data.
 *
 * Reimplement by Thomas Gleixner after staring long enough at the
 * mess in drivers/mtd/nand/nandecc.c
 *
 */

#include "nandecc.h"

static int countbits(uint32_t byte)
{
	int res = 0;

	for (;byte; byte >>= 1)
		res += byte & 0x01;
	return res;
}

/**
 * @dat:       data which should be corrected
 * @read_ecc:  ecc information read from flash
 * @calc_ecc:  calculated ecc information from the data
 * @return:    number of corrected bytes
 *             or -1 when no correction is possible
 */
int nand_correct_data(uint8_t *dat, const uint8_t *read_ecc,
		      const uint8_t *calc_ecc)
{
	uint8_t s0, s1, s2;

	/*
	 * Do error detection
	 *
	 * Be careful, the index magic is due to a pointer to a
	 * uint32_t.
	 */
	s0 = calc_ecc[0] ^ read_ecc[0];
	s1 = calc_ecc[1] ^ read_ecc[1];
	s2 = calc_ecc[2] ^ read_ecc[2];

	if ((s0 | s1 | s2) == 0)
		return 0;

	/* Check for a single bit error */
	if( ((s0 ^ (s0 >> 1)) & 0x55) == 0x55 &&
	    ((s1 ^ (s1 >> 1)) & 0x55) == 0x55 &&
	    ((s2 ^ (s2 >> 1)) & 0x54) == 0x54) {

		uint32_t byteoffs, bitnum;

		byteoffs = (s1 << 0) & 0x80;
		byteoffs |= (s1 << 1) & 0x40;
		byteoffs |= (s1 << 2) & 0x20;
		byteoffs |= (s1 << 3) & 0x10;

		byteoffs |= (s0 >> 4) & 0x08;
		byteoffs |= (s0 >> 3) & 0x04;
		byteoffs |= (s0 >> 2) & 0x02;
		byteoffs |= (s0 >> 1) & 0x01;

		bitnum = (s2 >> 5) & 0x04;
		bitnum |= (s2 >> 4) & 0x02;
		bitnum |= (s2 >> 3) & 0x01;

		dat[byteoffs] ^= (1 << bitnum);

		return 1;
	}

	if(countbits(s0 | ((uint32_t)s1 << 8) | ((uint32_t)s2 <<16)) == 1)
		return 1;

	return -1;
}

