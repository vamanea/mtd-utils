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

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "hashmap.h"
#define DEFAULT_BUCKETS 4096

#if 0
#define INFO_MSG(fmt...) do {	\
	info_msg(fmt);		\
} while (0)
#else
#define INFO_MSG(fmt...)
#endif

struct hashentry {
	char* key;	/* key '0' term. str */
	char* value;    /* payload '0' term. str */

	hashentry_t next;
};

struct hashmap {
	size_t entries;     /* current #entries */
	size_t maxsize;     /* no. of hash buckets */
	hashentry_t* data;  /* array of buckets */
};

static int
is_empty(hashentry_t l)
{
	return l == NULL ? 1 : 0;
}

hashmap_t
hashmap_new(void)
{
	hashmap_t res;
	res = (hashmap_t) calloc(1, sizeof(struct hashmap));

	if (res == NULL)
		return NULL;

	res->maxsize = DEFAULT_BUCKETS;
	res->entries = 0;

	res->data   = (hashentry_t*)
		calloc(1, res->maxsize * sizeof(struct hashentry));

	if (res->data == NULL)
		return NULL;

	return res;
}

static hashentry_t
new_entry(const char* key, const char* value)
{
	hashentry_t res;

	res = (hashentry_t) calloc(1, sizeof(struct hashentry));

	if (res == NULL)
		return NULL;

	/* allocate key and value and copy them */
	res->key = strdup(key);
	if (res->key == NULL) {
		free(res);
		return NULL;
	}

	res->value = strdup(value);
	if (res->value == NULL) {
		free(res->key);
		free(res);
		return NULL;
	}

	res->next = NULL;

	return res;
}

static hashentry_t
free_entry(hashentry_t e)
{
	if (!is_empty(e)) {
		if(e->key != NULL) {
			free(e->key);
		}
		if(e->value != NULL)
			free(e->value);
		free(e);
	}

	return NULL;
}

static hashentry_t
remove_entry(hashentry_t l, const char* key, size_t* entries)
{
	hashentry_t lnext;
	if (is_empty(l))
		return NULL;

	if(strcmp(l->key,key) == 0) {
		lnext = l->next;
		l = free_entry(l);
		(*entries)--;
		return lnext;
	}

	l->next = remove_entry(l->next, key, entries);

	return l;
}

static hashentry_t
insert_entry(hashentry_t l, hashentry_t e, size_t* entries)
{
	if (is_empty(l)) {
		(*entries)++;
		return e;
	}

	/* check for update */
	if (strcmp(l->key, e->key) == 0) {
		e->next = l->next;
		l = free_entry(l);
		return e;
	}

	l->next = insert_entry(l->next, e, entries);
	return l;
}

static hashentry_t
remove_all(hashentry_t l, size_t* entries)
{
	hashentry_t lnext;
	if (is_empty(l))
		return NULL;

	lnext = l->next;
	free_entry(l);
	(*entries)--;

	return remove_all(lnext, entries);
}

static const char*
value_lookup(hashentry_t l, const char* key)
{
	if (is_empty(l))
		return NULL;

	if (strcmp(l->key, key) == 0)
		return l->value;

	return value_lookup(l->next, key);
}

static void
print_all(hashentry_t l)
{
	if (is_empty(l)) {
		printf("\n");
		return;
	}

	printf("%s=%s", l->key, l->value);
	if (!is_empty(l->next)) {
		printf(",");
	}

	print_all(l->next);
}

static void
keys_to_array(hashentry_t l, const char** a, size_t* i)
{
	if (is_empty(l))
		return;

	a[*i] = l->key;
	(*i)++;

	keys_to_array(l->next, a, i);
}

uint32_t
hash_str(const char* str, uint32_t mapsize)
{
	uint32_t hash = 0;
	uint32_t x    = 0;
	uint32_t i    = 0;
	size_t   len  = strlen(str);

	for(i = 0; i < len; str++, i++)	{
		hash = (hash << 4) + (*str);
		if((x = hash & 0xF0000000L) != 0) {
			hash ^= (x >> 24);
			hash &= ~x;
		}
	}

	return (hash & 0x7FFFFFFF) % mapsize;
}


int
hashmap_is_empty(hashmap_t map)
{
	if (map == NULL)
		return -EINVAL;

	return map->entries > 0 ? 1 : 0;
}

const char*
hashmap_lookup(hashmap_t map, const char* key)
{
	uint32_t i;

	if ((map == NULL) || (key == NULL))
		return NULL;

	i = hash_str(key, map->maxsize);

	return value_lookup(map->data[i], key);
}

int
hashmap_add(hashmap_t map, const char* key, const char* value)
{
	uint32_t i;
	hashentry_t entry;

	if ((map == NULL) || (key == NULL) || (value == NULL))
		return -EINVAL;

	i = hash_str(key, map->maxsize);
	entry = new_entry(key, value);
	if (entry == NULL)
		return -ENOMEM;

	map->data[i] = insert_entry(map->data[i],
			entry, &map->entries);

	INFO_MSG("HASH_ADD: chain[%d] key:%s val:%s",i,  key, value);
	return 0;
}

int
hashmap_remove(hashmap_t map, const char* key)
{
	uint32_t i;

	if ((map == NULL) || (key == NULL))
		return -EINVAL;

	i = hash_str(key, map->maxsize);
	map->data[i] = remove_entry(map->data[i], key, &map->entries);

	return 0;
}

size_t
hashmap_size(hashmap_t map)
{
	if (map != NULL)
		return map->entries;
	else
		return 0;
}

int
hashmap_free(hashmap_t map)
{
	size_t i;

	if (map == NULL)
		return -EINVAL;

	/* "children" first */
	for(i = 0; i < map->maxsize; i++) {
		map->data[i] = remove_all(map->data[i], &map->entries);
	}
	free(map->data);
	free(map);

	return 0;
}

int
hashmap_dump(hashmap_t map)
{
	size_t i;
	if (map == NULL)
		return -EINVAL;

	for(i = 0; i < map->maxsize; i++) {
		if (map->data[i] != NULL) {
			printf("[%zd]: ", i);
			print_all(map->data[i]);
		}
	}

	return 0;
}

static const char**
sort_key_vector(const char** a, size_t size)
{
	/* uses bubblesort */
	size_t i, j;
	const char* tmp;

	if (size <= 0)
		return a;

	for (i = size - 1; i > 0; i--) {
		for (j = 0; j < i; j++) {
			if (strcmp(a[j], a[j+1]) > 0) {
				tmp  = a[j];
				a[j] = a[j+1];
				a[j+1] = tmp;
			}
		}
	}
	return a;
}

const char**
hashmap_get_key_vector(hashmap_t map, size_t* size, int sort)
{
	const char** res;
	size_t i, j;
	*size = map->entries;

	res = (const char**) malloc(*size * sizeof(char*));
	if (res == NULL)
		return NULL;

	j = 0;
	for(i=0; i < map->maxsize; i++) {
		keys_to_array(map->data[i], res, &j);
	}

	if (sort)
		res = sort_key_vector(res, *size);

	return res;
}

int
hashmap_key_is_in_vector(const char** vec, size_t size, const char* key)
{
	size_t i;
	for (i = 0; i < size; i++) {
		if (strcmp(vec[i], key) == 0) /* found */
			return 1;
	}

	return 0;
}

const char**
hashmap_get_update_key_vector(const char** vec1, size_t vec1_size,
		const char** vec2, size_t vec2_size, size_t* res_size)
{
	const char** res;
	size_t i, j;

	*res_size = vec2_size;

	res = (const char**) malloc(*res_size * sizeof(char*));
	if (res == NULL)
		return NULL;

	/* get all keys from vec2 which are not set in vec1 */
	j = 0;
	for (i = 0; i < vec2_size; i++) {
		if (!hashmap_key_is_in_vector(vec1, vec1_size, vec2[i]))
			res[j++] = vec2[i];
	}

	*res_size = j;
	return res;
}
