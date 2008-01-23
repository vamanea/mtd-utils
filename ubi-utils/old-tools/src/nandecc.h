#ifndef _NAND_ECC_H
#define _NAND_ECC_H
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
 * NAND ecc functions
 */

#include <stdint.h>

int nand_calculate_ecc(const uint8_t *dat, uint8_t *ecc_code);
int nand_correct_data(uint8_t *dat, const uint8_t *read_ecc,
		      const uint8_t *calc_ecc);

#endif
