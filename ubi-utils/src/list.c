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

int
is_empty(struct list_entry *l)
{
	return l == NULL;
}

info_t
head(struct list_entry *l)
{
	assert(!is_empty(l));
	return l->info;
}

struct list_entry *
tail(struct list_entry *l)
{
	assert(!is_empty(l));
	return l->next;
}

struct list_entry *
remove_head(struct list_entry *l)
{
	struct list_entry *res;
	assert(!is_empty(l));

	res = l->next;
	free(l);
	return res;
}

struct list_entry *
cons(info_t e, struct list_entry *l)
{
	struct list_entry *res = malloc(sizeof(*l));
	if (!res)
		return NULL;
	res->info = e;
	res->next = l;

	return res;
}

struct list_entry *
prepend_elem(info_t e, struct list_entry *l)
{
	return cons(e,l);
}

struct list_entry *
append_elem(info_t e, struct list_entry *l)
{
	if (is_empty(l)) {
		return cons(e,l);
	}
	l->next = append_elem(e, l->next);

	return l;
}

struct list_entry *
insert_sorted(cmp_func_t cmp, info_t e, struct list_entry *l)
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

struct list_entry *
remove_all(free_func_t free_func, struct list_entry *l)
{
	if (is_empty(l))
		return l;
	struct list_entry *lnext = l->next;

	if (free_func && l->info) {
		free_func(&(l->info));
	}
	free(l);

	return remove_all(free_func, lnext);
}


info_t
is_in(cmp_func_t cmp, info_t e, struct list_entry *l)
{
	return
	(is_empty(l))
	? NULL
	: (cmp(e, l->info)) == 0 ? l->info : is_in(cmp, e, l->next);
}


void
apply(process_func_t process_func, struct list_entry *l)
{
	struct list_entry *ptr;
	void *i;
	list_for_each(i, ptr, l) {
		process_func(i);
	}
}
