#ifndef __EB_CHAIN_H__
#define __EB_CHAIN_H__

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

typedef struct eb_info *eb_info_t;
struct eb_info {
	struct ubi_ec_hdr ec;
	struct ubi_vid_hdr vid;
	fpos_t eb_top;
	uint32_t linear;

	eb_info_t next;
	eb_info_t older;
};

int eb_chain_insert(eb_info_t *head, eb_info_t item);

int eb_chain_position(eb_info_t *head, uint32_t vol_id, uint32_t *lnum,
		      eb_info_t *pos);

int eb_chain_print(FILE *stream, eb_info_t *head);

int eb_chain_destroy(eb_info_t *head);

#endif /* __EB_CHAIN_H__ */
