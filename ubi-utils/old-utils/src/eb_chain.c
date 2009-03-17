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

/*
 * Author:  Drake Dowsett, dowsett@de.ibm.com
 * Contact: Andreas Arnez, arnez@de.ibm.com
 */

/* see eb_chain.h */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <mtd_swab.h>
#include "unubi_analyze.h"
#include "crc32.h"

#define COPY(dst, src)							\
	do {								\
		dst = malloc(sizeof(*dst));				\
		if (dst == NULL)					\
			return -ENOMEM;					\
		memcpy(dst, src, sizeof(*dst));				\
	} while (0)


/**
 * inserts an eb_info into the chain starting at head, then searching
 * linearly for the correct position;
 * new should contain valid vid and ec headers and the data_crc should
 * already have been checked before insertion, otherwise the chain
 * could be have un an undesired manner;
 * returns -ENOMEM if alloc fails, otherwise SHOULD always return 0,
 * if not, the code reached the last line and returned -EAGAIN,
 * meaning there is a bug or a case not being handled here;
 **/
int
eb_chain_insert(struct eb_info **head, struct eb_info *new)
{
	uint32_t vol, num, ver;
	uint32_t new_vol, new_num, new_ver;
	struct eb_info *prev, *cur, *hist, *ins;
	struct eb_info **prev_ptr;

	if ((head == NULL) || (new == NULL))
		return 0;

	if (*head == NULL) {
		COPY(*head, new);
		(*head)->next = NULL;
		return 0;
	}

	new_vol = be32_to_cpu(new->vid.vol_id);
	new_num = be32_to_cpu(new->vid.lnum);
	new_ver = be32_to_cpu(new->vid.leb_ver);

	/** TRAVERSE HORIZONTALY **/

	cur = *head;
	prev = NULL;

	/* traverse until vol_id/lnum align */
	vol = be32_to_cpu(cur->vid.vol_id);
	num = be32_to_cpu(cur->vid.lnum);
	while ((new_vol > vol) || ((new_vol == vol) && (new_num > num))) {
		/* insert new at end of chain */
		if (cur->next == NULL) {
			COPY(ins, new);
			ins->next = NULL;
			cur->next = ins;
			return 0;
		}

		prev = cur;
		cur = cur->next;
		vol = be32_to_cpu(cur->vid.vol_id);
		num = be32_to_cpu(cur->vid.lnum);
	}

	if (prev == NULL)
		prev_ptr = head;
	else
		prev_ptr = &(prev->next);

	/* insert new into the middle of chain */
	if ((new_vol != vol) || (new_num != num)) {
		COPY(ins, new);
		ins->next = cur;
		*prev_ptr = ins;
		return 0;
	}

	/** TRAVERSE VERTICALY **/

	hist = cur;
	prev = NULL;

	/* traverse until versions align */
	ver = be32_to_cpu(cur->vid.leb_ver);
	while (new_ver < ver) {
		/* insert new at bottom of history */
		if (hist->older == NULL) {
			COPY(ins, new);
			ins->next = NULL;
			ins->older = NULL;
			hist->older = ins;
			return 0;
		}

		prev = hist;
		hist = hist->older;
		ver = be32_to_cpu(hist->vid.leb_ver);
	}

	if (prev == NULL) {
		/* replace active version */
		COPY(ins, new);
		ins->next = hist->next;
		*prev_ptr = ins;

		/* place cur in vertical histroy */
		ins->older = hist;
		hist->next = NULL;
		return 0;
	}

	/* insert between versions, beneath active version */
	COPY(ins, new);
	ins->next = NULL;
	ins->older = prev->older;
	prev->older = ins;
	return 0;
}


/**
 * sets the pointer at pos to the position of the first entry in the chain
 * with of vol_id and, if given, with the same lnum as *lnum;
 * if there is no entry in the chain, then *pos is NULL on return;
 * always returns 0;
 **/
int
eb_chain_position(struct eb_info **head, uint32_t vol_id, uint32_t *lnum,
		  struct eb_info **pos)
{
	uint32_t vol, num;
	struct eb_info *cur;

	if ((head == NULL) || (*head == NULL) || (pos == NULL))
		return 0;

	*pos = NULL;

	cur = *head;
	while (cur != NULL) {
		vol = be32_to_cpu(cur->vid.vol_id);
		num = be32_to_cpu(cur->vid.lnum);

		if ((vol_id == vol) && ((lnum == NULL) || (*lnum == num))) {
			*pos = cur;
			return 0;
		}

		cur = cur->next;
	}

	return 0;
}


/**
 * prints to stream, the vol_id, lnum and leb_ver for each entry in the
 * chain, starting at head;
 * this is intended for debuging purposes;
 * always returns 0;
 *
 * FIXME I do not like the double list traversion ...
 **/
int
eb_chain_print(FILE* stream, struct eb_info *head)
{
	struct eb_info *cur;

	if (stream == NULL)
		stream = stdout;

	if (head == NULL) {
		fprintf(stream, "EMPTY\n");
		return 0;
	}
	/*               012345678012345678012345678012301230123 0123 01234567 0123457 01234567*/
	fprintf(stream, "VOL_ID   LNUM     LEB_VER  EC  VID DAT  PBLK PADDR    DSIZE   EC\n");
	cur = head;
	while (cur != NULL) {
		struct eb_info *hist;

		fprintf(stream, "%08x %-8u %08x %-4s%-4s",
			be32_to_cpu(cur->vid.vol_id),
			be32_to_cpu(cur->vid.lnum),
			be32_to_cpu(cur->vid.leb_ver),
			cur->ec_crc_ok   ? "ok":"bad",
			cur->vid_crc_ok  ? "ok":"bad");
		if (cur->vid.vol_type == UBI_VID_STATIC)
			fprintf(stream, "%-4s", cur->data_crc_ok ? "ok":"bad");
		else	fprintf(stream, "%-4s", cur->data_crc_ok ? "ok":"ign");
		fprintf(stream, " %-4d %08x %-8u %-8llu\n", cur->phys_block,
			cur->phys_addr, be32_to_cpu(cur->vid.data_size),
			(unsigned long long)be64_to_cpu(cur->ec.ec));

		hist = cur->older;
		while (hist != NULL) {
			fprintf(stream, "%08x %-8u %08x %-4s%-4s",
				be32_to_cpu(hist->vid.vol_id),
				be32_to_cpu(hist->vid.lnum),
				be32_to_cpu(hist->vid.leb_ver),
				hist->ec_crc_ok   ? "ok":"bad",
				hist->vid_crc_ok  ? "ok":"bad");
			if (hist->vid.vol_type == UBI_VID_STATIC)
				fprintf(stream, "%-4s", hist->data_crc_ok ? "ok":"bad");
			else	fprintf(stream, "%-4s", hist->data_crc_ok ? "ok":"ign");
			fprintf(stream, " %-4d %08x %-8u %-8llu (*)\n",
				hist->phys_block, hist->phys_addr,
				be32_to_cpu(hist->vid.data_size),
				(unsigned long long)be64_to_cpu(hist->ec.ec));

			hist = hist->older;
		}
		cur = cur->next;
	}

	return 0;
}


/**
 * frees the memory of the entire chain, starting at head;
 * head will be NULL on return;
 * always returns 0;
 **/
int
eb_chain_destroy(struct eb_info **head)
{
	if (head == NULL)
		return 0;

	while (*head != NULL) {
		struct eb_info *cur;
		struct eb_info *hist;

		cur = *head;
		*head = (*head)->next;

		hist = cur->older;
		while (hist != NULL) {
			struct eb_info *temp;

			temp = hist;
			hist = hist->older;
			free(temp);
		}
		free(cur);
	}
	return 0;
}

