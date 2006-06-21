#ifndef __HASHMAP_H__
#define __HASHMAP_H__
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
#include <stdint.h>

typedef struct hashentry *hashentry_t;
typedef struct hashmap *hashmap_t;

hashmap_t hashmap_new(void);
int hashmap_free(hashmap_t map);

int hashmap_add(hashmap_t map, const char* key, const char* value);
int hashmap_update(hashmap_t map, const char* key, const char* value);
int hashmap_remove(hashmap_t map, const char* key);
const char* hashmap_lookup(hashmap_t map, const char* key);

const char** hashmap_get_key_vector(hashmap_t map, size_t* size, int sort);
int hashmap_key_is_in_vector(const char** vec, size_t size, const char* key);
const char** hashmap_get_update_key_vector(const char** vec1, size_t vec1_size,
		const char** vec2, size_t vec2_size, size_t* res_size);

int hashmap_dump(hashmap_t map);

int hashmap_is_empty(hashmap_t map);
size_t hashmap_size(hashmap_t map);

uint32_t hash_str(const char* str, uint32_t mapsize);

#endif /* __HASHMAP_H__ */
