/*
 * Copyright (c) International Business Machines Corp., 2006, 2007
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

#ifndef __UNUBI_ANALYZE_H__
#define __UNUBI_ANALYZE_H__

/*
 * Author:  Drake Dowsett
 * Contact: Andreas Arnez (arnez@de.ibm.com)
 *
 * Eraseblock Chain
 *
 * A linked list structure to order eraseblocks by volume and logical number
 * and to update by version number. Doesn't contain actual eraseblock data
 * but rather the erasecounter and volume id headers as well as a position
 * indicator.
 *
 * Diagram Example:
 *
 * [V1.0v0]->[V1.1v2]->[V1.2v1]->[V2.0v2]->[V2.1v0]->[V2.2v1]->NULL
 *     |         |         |         |         |         |
 *   NULL    [V1.1v1]  [V1.2v0]  [V2.0v1]    NULL    [V2.2v0]
 *               |         |         |                   |
 *           [V1.1v0]    NULL    [V2.0v0]              NULL
 *               |                   |
 *             NULL                NULL
 *
 * [VA.BvC] represents the eb_info for the eraseblock with the vol_id A,
 * lnum B and leb_ver C
 * -> represents the `next' pointer
 * | represents the `older' pointer
 */

#include <stdio.h>
#include <stdint.h>
#include <mtd/ubi-header.h>

#define FN_EH_STAT      "analysis_blocks.txt"
#define FN_EH_DATA	"analysis_ec_hdr.data"
#define FN_EH_PLOT	"analysis_ec_hdr.plot"
#define FN_VH_DATA	"analysis_vid_hdr.data"
#define FN_VH_PLOT	"analysis_vid_hdr.plot"

struct eb_info {
	struct ubi_ec_hdr ec;
	struct ubi_vid_hdr vid;

	fpos_t eb_top;
	uint32_t linear;
	int ec_crc_ok;
	int vid_crc_ok;
	int data_crc_ok;
	uint32_t phys_addr;
	int phys_block;

	struct eb_info *next;
	struct eb_info *older;
};

int eb_chain_insert(struct eb_info **head, struct eb_info *item);

int eb_chain_position(struct eb_info **head, uint32_t vol_id, uint32_t *lnum,
		      struct eb_info **pos);

int eb_chain_print(FILE *stream, struct eb_info *head);

int eb_chain_destroy(struct eb_info **head);

int unubi_analyze(struct eb_info **head, struct eb_info *first,
		  const char *path);

#endif /* __UNUBI_ANALYZE_H__ */
