#ifndef __ECCLAYOUTS_H__
#define __ECCLAYOUTS_H__
/*
 * Copyright (c) International Business Machines Corp., 2007
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

#include <stdint.h>
#include <mtd/mtd-abi.h>

/* Define default oob placement schemes for large and small page devices */
static struct nand_ecclayout mtd_nand_oob_16 = {
	.eccbytes = 6,
	.eccpos = { 0, 1, 2, 3, 6, 7 },
	.oobfree = {{ .offset = 8, .length = 8 }}
};

static struct nand_ecclayout mtd_nand_oob_64 = {
	.eccbytes = 24,
	.eccpos = { 40, 41, 42, 43, 44, 45, 46, 47,
		    48, 49, 50, 51, 52, 53, 54, 55,
		    56, 57, 58, 59, 60, 61, 62, 63 },
	.oobfree = {{ .offset = 2, .length = 38 }}
};

/* Define IBM oob placement schemes */
static struct nand_ecclayout ibm_nand_oob_16 = {
	.eccbytes = 6,
	.eccpos = { 9, 10, 11, 13, 14, 15 },
	.oobfree = {{ .offset = 8, .length = 8 }}
};

static struct nand_ecclayout ibm_nand_oob_64 = {
	.eccbytes = 24,
	.eccpos = { 33, 34, 35, 37, 38, 39, 41, 42,
		    43, 45, 46, 47, 49, 50, 51, 53,
		    54, 55, 57, 58, 59, 61, 62, 63 },
	.oobfree = {{ .offset = 2, .length = 30 }}
};

struct oob_placement {
	const char *name;
	struct nand_ecclayout *nand_oob[2];
};

static struct oob_placement oob_placement[] = {
	{ .name = "IBM",
	  .nand_oob = { &ibm_nand_oob_16, &ibm_nand_oob_64 }},
	{ .name = "MTD",
	  .nand_oob = { &mtd_nand_oob_16, &mtd_nand_oob_64 }},
};

#endif
