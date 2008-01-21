#ifndef __LIST_H__
#define __LIST_H__
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

#include <stdint.h>

#define foreach(elem, ptr, list)				\
	for (elem = list != NULL ? (typeof(elem)) head(list)	\
				 : NULL, ptr = list;		\
		ptr != NULL;					\
		ptr = tail(ptr),				\
		elem = (typeof(elem)) ptr ? head(ptr) : NULL)

typedef struct node* list_t;
typedef void* info_t;
typedef int  (*free_func_t)(info_t*);
typedef int  (*cmp_func_t)(info_t, info_t);
typedef void (*process_func_t)(info_t);

struct node {
	list_t next;
	info_t	info;
};

list_t mk_empty(void);
int    is_empty(list_t l);
info_t is_in(cmp_func_t cmp, info_t e, list_t l);
info_t head(list_t l);
list_t tail(list_t l);
list_t remove_head(list_t l);
list_t cons(info_t e, list_t l);
list_t prepend_elem(info_t e, list_t);
list_t append_elem(info_t e, list_t);
list_t remove_all(free_func_t free_func, list_t l);
list_t insert_sorted(cmp_func_t cmp_func, info_t e, list_t l);
void   apply(process_func_t process_func, list_t l);

#endif /* __LIST_H__ */
