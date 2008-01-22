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

#ifndef __UBIUTILS_LIST_H__
#define __UBIUTILS_LIST_H__

#include <stdint.h>

#define list_for_each(elem, ptr, list)                           \
	for ((elem) = (list) != NULL ? (typeof(elem)) head(list) \
				 : NULL, (ptr) = (list);         \
		ptr != NULL;                                     \
		ptr = tail(ptr),                                 \
		elem = (typeof(elem)) (ptr) ? head(ptr) : NULL)

static inline struct list_entry *list_empty(void)
{
	return NULL;
}

typedef void* info_t;
typedef int  (*free_func_t)(info_t*);
typedef int  (*cmp_func_t)(info_t, info_t);
typedef void (*process_func_t)(info_t);

struct list_entry {
	struct list_entry *next;
	info_t	info;
};

struct list_entry *list_empty(void);
int    is_empty(struct list_entry *l);
info_t is_in(cmp_func_t cmp, info_t e, struct list_entry *l);
info_t head(struct list_entry *l);
struct list_entry *tail(struct list_entry *l);
struct list_entry *remove_head(struct list_entry *l);
struct list_entry *cons(info_t e, struct list_entry *l);
struct list_entry *prepend_elem(info_t e, struct list_entry *);
struct list_entry *append_elem(info_t e, struct list_entry *);
struct list_entry *remove_all(free_func_t free_func, struct list_entry *l);
struct list_entry *insert_sorted(cmp_func_t cmp_func, info_t e, struct list_entry *l);
void   apply(process_func_t process_func, struct list_entry *l);

#endif /* !__UBIUTILS_LIST_H__ */
