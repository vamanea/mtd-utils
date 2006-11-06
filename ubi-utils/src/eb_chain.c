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
 * Author:  Drake Dowsett, dowsett@de.ibm.com
 * Contact: Andreas Arnez, arnez@de.ibm.com
 */

/* see eb_chain.h */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "eb_chain.h"

#define COPY(dst, src)							\
	do								\
	{								\
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
eb_chain_insert(eb_info_t *head, eb_info_t new)
{
	uint32_t vol, num, ver;
	uint32_t new_vol, new_num, new_ver;
	eb_info_t prev, cur, hist, ins;
	eb_info_t *prev_ptr;

	if ((head == NULL) || (new == NULL))
		return 0;

	if (*head == NULL)
	{
		COPY(*head, new);
		(*head)->next = NULL;
		return 0;
	}

	new_vol = ubi32_to_cpu(new->inner.vol_id);
	new_num = ubi32_to_cpu(new->inner.lnum);
	new_ver = ubi32_to_cpu(new->inner.leb_ver);

	/** TRAVERSE HORIZONTALY **/

	cur = *head;
	prev = NULL;

	/* traverse until vol_id/lnum align */
	vol = ubi32_to_cpu(cur->inner.vol_id);
	num = ubi32_to_cpu(cur->inner.lnum);
	while ((new_vol > vol) || ((new_vol == vol) && (new_num > num)))
	{
		/* insert new at end of chain */
		if (cur->next == NULL)
		{
			COPY(ins, new);
			ins->next = NULL;
			cur->next = ins;
			return 0;
		}

		prev = cur;
		cur = cur->next;
		vol = ubi32_to_cpu(cur->inner.vol_id);
		num = ubi32_to_cpu(cur->inner.lnum);
	}

	if (prev == NULL)
		prev_ptr = head;
	else
		prev_ptr = &(prev->next);

	/* insert new into the middle of chain */
	if ((new_vol != vol) || (new_num != num))
	{
		COPY(ins, new);
		ins->next = cur;
		*prev_ptr = ins;
		return 0;
	}

	/** TRAVERSE VERTICALY **/

	hist = cur;
	prev = NULL;

	/* traverse until versions align */
	ver = ubi32_to_cpu(cur->inner.leb_ver);
	while (new_ver < ver)
	{
		/* insert new at bottom of history */
		if (hist->older == NULL)
		{
			COPY(ins, new);
			ins->next = NULL;
			ins->older = NULL;
			hist->older = ins;
			return 0;
		}

		prev = hist;
		hist = hist->older;
		ver = ubi32_to_cpu(hist->inner.leb_ver);
	}

	if (prev == NULL)
	{
		/* replace active version */
		COPY(ins, new);
		ins->next = hist->next;
		*prev_ptr = ins;

		/* place cur in vertical histroy */
		ins->older = hist;
		hist->next = NULL;
		return 0;
	}
	else
	{
		/* insert between versions, beneath active version */
		COPY(ins, new);
		ins->next = NULL;
		ins->older = prev->older;
		prev->older = ins;
		return 0;
	}

	/* logically impossible to reach this point... hopefully */
	return -EAGAIN;
}


/**
 * sets the pointer at pos to the position of the first entry in the chain
 * with of vol_id and, if given, with the same lnum as *lnum;
 * if there is no entry in the chain, then *pos is NULL on return;
 * always returns 0;
 **/
int
eb_chain_position(eb_info_t *head, uint32_t vol_id, uint32_t *lnum,
		  eb_info_t *pos)
{
	uint32_t vol, num;
	eb_info_t cur;

	if ((head == NULL) || (*head == NULL) || (pos == NULL))
		return 0;

	*pos = NULL;

	cur = *head;
	while (cur != NULL)
	{
		vol = ubi32_to_cpu(cur->inner.vol_id);
		num = ubi32_to_cpu(cur->inner.lnum);

		if (vol_id == vol)
			if ((lnum == NULL) || (*lnum == num))
			{
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
 **/
int
eb_chain_print(FILE* stream, eb_info_t *head)
{
	eb_info_t cur;

	if (head == NULL)
		return 0;

	if (stream == NULL)
		stream = stdout;

	if (*head == NULL)
	{
		fprintf(stream, "EMPTY\n");
		return 0;
	}

	cur = *head;
	while (cur != NULL)
	{
		eb_info_t hist;

		fprintf(stream, "  VOL %4u-%04u | VER 0x%8x\n",
			ubi32_to_cpu(cur->inner.vol_id),
			ubi32_to_cpu(cur->inner.lnum),
			ubi32_to_cpu(cur->inner.leb_ver));

		hist = cur->older;
		while (hist != NULL)
		{
			fprintf(stream, "+ VOL %4u-%04u | VER 0x%8x\n",
				ubi32_to_cpu(hist->inner.vol_id),
				ubi32_to_cpu(hist->inner.lnum),
				ubi32_to_cpu(hist->inner.leb_ver));

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
eb_chain_destroy(eb_info_t *head)
{
	if (head == NULL)
		return 0;

	while (*head != NULL)
	{
		eb_info_t cur;
		eb_info_t hist;

		cur = *head;
		*head = (*head)->next;

		hist = cur->older;
		while (hist != NULL)
		{
			eb_info_t temp;

			temp = hist;
			hist = hist->older;

			free(temp);
		}

		free(cur);
	}

	return 0;
}

