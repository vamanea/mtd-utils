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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "peb.h"

int
peb_cmp(peb_t eb_1, peb_t eb_2)
{
	assert(eb_1);
	assert(eb_2);

	return eb_1->num == eb_2->num ? 0
		: eb_1->num > eb_2->num ? 1 : -1;
}

int
peb_new(uint32_t eb_num, uint32_t eb_size, peb_t *peb)
{
	int rc = 0;

	peb_t res = (peb_t) malloc(sizeof(struct peb));
	if (!res) {
		rc = -ENOMEM;
		goto err;
	}

	res->num  = eb_num;
	res->size = eb_size;
	res->data = (uint8_t*) malloc(res->size * sizeof(uint8_t));
	if (!res->data) {
		rc = -ENOMEM;
		goto err;
	}
	memset(res->data, 0xff, res->size);

	*peb = res;
	return 0;
err:
	if (res) {
		if (res->data)
			free(res->data);
		free(res);
	}
	*peb = NULL;
	return rc;
}

int
peb_fill(peb_t peb, uint8_t* buf, size_t buf_size)
{
	if (!peb)
		return -EINVAL;

	if (buf_size > peb->size)
		return -EINVAL;

	memcpy(peb->data, buf, buf_size);
	return 0;
}

int
peb_write(FILE* fp_out, peb_t peb)
{
	size_t written = 0;

	if (peb == NULL)
		return -EINVAL;

	written = fwrite(peb->data, 1, peb->size, fp_out);

	if (written != peb->size)
		return -EIO;

	return 0;
}

int
peb_free(peb_t* peb)
{
	peb_t tmp = *peb;
	if (tmp) {
		if (tmp->data)
			free(tmp->data);
		free(tmp);
	}
	*peb = NULL;

	return 0;
}

void peb_dump(FILE* fp_out, peb_t peb)
{
	fprintf(fp_out, "num: %08d\tsize: 0x%08x\n", peb->num, peb->size);
}
