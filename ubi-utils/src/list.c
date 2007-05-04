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

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "list.h"

list_t
mk_empty(void)
{
	return (list_t) NULL;
}

int
is_empty(list_t l)
{
	return l == NULL;
}

info_t
head(list_t l)
{
	assert(!is_empty(l));
	return l->info;
}

list_t
tail(list_t l)
{
	assert(!is_empty(l));
	return l->next;
}

list_t
remove_head(list_t l)
{
	list_t res;
	assert(!is_empty(l));

	res = l->next;
	free(l);
	return res;
}

list_t
cons(info_t e, list_t l)
{
	list_t res = malloc(sizeof(*l));
	if (!res)
		return NULL;
	res->info = e;
	res->next = l;

	return res;
}

list_t
prepend_elem(info_t e, list_t l)
{
	return cons(e,l);
}

list_t
append_elem(info_t e, list_t l)
{
	if (is_empty(l)) {
		return cons(e,l);
	}
	l->next = append_elem(e, l->next);

	return l;
}

list_t
insert_sorted(cmp_func_t cmp, info_t e, list_t l)
{
	if (is_empty(l))
		return cons(e, l);

	switch (cmp(e, l->info)) {
	case -1:
	case  0:
		return l;
		break;
	case  1:
		l->next = insert_sorted(cmp, e, l);
		break;
	default:
		break;
	}

	/* never reached */
	return NULL;
}

list_t
remove_all(free_func_t free_func, list_t l)
{
	if (is_empty(l))
		return l;
	list_t lnext = l->next;

	if (free_func && l->info) {
		free_func(&(l->info));
	}
	free(l);

	return remove_all(free_func, lnext);
}


info_t
is_in(cmp_func_t cmp, info_t e, list_t l)
{
	return
	(is_empty(l))
	? NULL
	: (cmp(e, l->info)) == 0 ? l->info : is_in(cmp, e, l->next);
}


void
apply(process_func_t process_func, list_t l)
{
	list_t ptr;
	void *i;
	foreach(i, ptr, l) {
		process_func(i);
	}
}
