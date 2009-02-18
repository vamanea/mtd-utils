/*
 * Copyright (c) International Business Machines Corp., 2008
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <bootenv.h>

#include "hashmap.h"
#include "error.h"

#include <mtd/ubi-media.h>
#include "crc32.h"

#define ubi_unused __attribute__((unused))

#define BOOTENV_MAXLINE 512 /* max line size of a bootenv.txt file */

/* Structures */
struct bootenv {
	hashmap_t map;	 ///< Pointer to hashmap which holds data structure.
};

struct bootenv_list {
	hashmap_t head; ///< Pointer to list which holds the data structure.
};

/**
 * @brief Remove the '\n' from a given line.
 * @param line	Input/Output line.
 * @param size	Size of the line.
 * @param fp	File Pointer.
 * @return 0
 * @return or error
 */
static int
remove_lf(char *line, size_t size, FILE* fp)
{
	size_t i;

	for (i = 0; i < size; i++) {
		if (line[i] == '\n') {
			line[i] = '\0';
			return 0;
		}
	}

	if (!feof(fp)) {
		return BOOTENV_EINVAL;
	}

	return 0;
}

/**
 * @brief Determine if a line contains only WS.
 * @param line The line to process.
 * @param size Size of input line.
 * @return 1	Yes, only WS.
 * @return 0	No, contains data.
 */
static int
is_ws(const char *line, size_t size)
{
	size_t i = 0;

	while (i < size) {
		switch (line[i]) {
			case '\n':
				return 1;
			case '#':
				return 1;
			case ' ':
				i++;
				continue;
			case '\t':
				i++;
				continue;
			default: /* any other char -> no cmnt */
				return 0;
		}
	}

	return 0;
}


/* ------------------------------------------------------------------------- */

/**
 * @brief Build a list from a comma seperated value string.
 * @param list	Pointer to hashmap structure which shall store
 *		the list.
 * @param value	Comma seperated value string.
 * @return 0
 * @return or error.
 */
static int
build_list_definition(hashmap_t list, const char *value)
{
	int rc = 0;
	char *str = NULL;
	char *ptr = NULL;
	size_t len, i, j;

	/* str: val1,val2 , val4,...,valN     */
	len = strlen(value);
	str = (char*) malloc((len+1) * sizeof(char));

	/* 1. reformat string: remove spaces */
	for (i = 0, j = 0; i < len; i++) {
		if (value[i] == ' ')
			continue;

		str[j] = value[i];
		j++;
	}
	str[j] = '\0';

	/* str: val1,val2,val4,...,valN\0*/
	/* 2. replace ',' seperator with '\0' */
	len = strlen(str);
	for (i = 0; i < len; i++) {
		if (str[i] == ',') {
			str[i] = '\0';
		}
	}

	/* str: val1\0val2\0val4\0...\0valN\0*/
	/* 3. insert definitions into a hash map, using it like a list */
	i = j = 0;
	ptr = str;
	while (((i = strlen(ptr)) > 0) && (j < len)) {
		rc = hashmap_add(list, ptr, "");
		if (rc != 0) {
			free(str);
			return rc;
		}
		j += i+1;
		if (j < len)
			ptr += i+1;
	}

	free(str);
	return rc;
}

/**
 * @brief Extract a key value pair and add it to a hashmap
 * @param str	Input string which contains a key value pair.
 * @param env	The updated handle which contains the new pair.
 * @return 0
 * @return or error
 * @note The input string format is: "key=value"
 */
static int
extract_pair(const char *str, bootenv_t env)
{
	int rc = 0;
	char *key = NULL;
	char *val = NULL;

	key = strdup(str);
	if (key == NULL)
		return -ENOMEM;

	val = strstr(key, "=");
	if (val == NULL) {
		rc = BOOTENV_EBADENTRY;
		goto err;
	}

	*val = '\0'; /* split strings */
	val++;

	rc = bootenv_set(env, key, val);

 err:
	free(key);
	return rc;
}

int
bootenv_destroy(bootenv_t* env)
{
	int rc = 0;

	if (env == NULL || *env == NULL)
		return -EINVAL;

	bootenv_t tmp = *env;

	rc = hashmap_free(tmp->map);
	if (rc != 0)
		return rc;

	free(tmp);
	return rc;
}

int
bootenv_create(bootenv_t* env)
{
	bootenv_t res;
	res = (bootenv_t) calloc(1, sizeof(struct bootenv));

	if (res == NULL)
		return -ENOMEM;

	res->map = hashmap_new();

	if (res->map == NULL) {
		free(res);
		return -ENOMEM;
	}

	*env = res;

	return 0;
}


/**
 * @brief Read a formatted buffer and scan it for valid bootenv
 *	  key/value pairs. Add those pairs into a hashmap.
 * @param env	Hashmap which shall be used to hold the data.
 * @param buf	Formatted buffer.
 * @param size	Size of the buffer.
 * @return 0
 * @return or error
 */
static int
rd_buffer(bootenv_t env, const char *buf, size_t size)
{
	const char *curr = buf;		/* ptr to current key/value pair */
	uint32_t i, j;			/* current length, chars processed */

	if (buf[size - 1] != '\0')	/* must end in '\0' */
		return BOOTENV_EFMT;

	for (j = 0; j < size; j += i, curr += i) {
		/* strlen returns the size of the string upto
		   but not including the null terminator;
		   adding 1 to account for '\0' */
		i = strlen(curr) + 1;

		if (i == 1)
			return 0;	/* no string found */

		if (extract_pair(curr, env) != 0)
			return BOOTENV_EINVAL;
	}

	return 0;
}


int
bootenv_read_crc(FILE* fp, bootenv_t env, size_t size, uint32_t* ret_crc)
{
	int rc;
	char *buf = NULL;
	size_t i = 0;
	uint32_t crc32_table[256];

	if ((fp == NULL) || (env == NULL))
		return -EINVAL;

	/* allocate temp buffer */
	buf = (char*) calloc(1, size * sizeof(char));
	if (buf == NULL)
		return -ENOMEM;

	/* FIXME Andreas, please review this I removed size-1 and
	 * replaced it by just size, I saw the kernel image starting
	 * with a 0x0060.... and not with the 0x60.... what it should
	 * be. Is this a tools problem or is it a problem here where
	 * fp is moved not to the right place due to the former size-1
	 * here.
	 */
	while((i < size) && (!feof(fp))) {
		int c = fgetc(fp);
		if (c == EOF) {
			/* FIXME isn't this dangerous, to update
			   the boot envs with incomplete data? */
			buf[i++] = '\0';
			break;	/* we have enough */
		}
		if (ferror(fp)) {
			rc = -EIO;
			goto err;
		}

		buf[i++] = (char)c;
	}

	/* calculate crc to return */
	if (ret_crc != NULL) {
		init_crc32_table(crc32_table);
		*ret_crc = clc_crc32(crc32_table, UBI_CRC32_INIT, buf, size);
	}

	/* transfer to hashmap */
	rc = rd_buffer(env, buf, size);

err:
	free(buf);
	return rc;
}


/**
 * If we have a single file containing the boot-parameter size should
 * be specified either as the size of the file or as BOOTENV_MAXSIZE.
 * If the bootparameter are in the middle of a file we need the exact
 * length of the data.
 */
int
bootenv_read(FILE* fp, bootenv_t env, size_t size)
{
	return bootenv_read_crc(fp, env, size, NULL);
}


int
bootenv_read_txt(FILE* fp, bootenv_t env)
{
	int rc = 0;
	char *buf = NULL;
	char *line = NULL;
	char *lstart = NULL;
	char *curr = NULL;
	size_t len;
	size_t size;

	if ((fp == NULL) || (env == NULL))
		return -EINVAL;

	size = BOOTENV_MAXSIZE;

	/* allocate temp buffers */
	buf = (char*) calloc(1, size * sizeof(char));
	lstart = line = (char*) calloc(1, size * sizeof(char));
	if ((buf == NULL)  || (line == NULL)) {
		rc = -ENOMEM;
		goto err;
	}

	curr = buf;
	while ((line = fgets(line, size, fp)) != NULL) {
		if (is_ws(line, size)) {
			continue;
		}
		rc = remove_lf(line, BOOTENV_MAXSIZE, fp);
		if (rc != 0) {
			goto err;
		}

		/* copy new line to binary buffer */
		len = strlen(line);
		if (len > size) {
			rc = -EFBIG;
			goto err;
		}
		size -= len; /* track remaining space */

		memcpy(curr, line, len);
		curr += len + 1; /* for \0 seperator */
	}

	rc = rd_buffer(env, buf, BOOTENV_MAXSIZE);
err:
	if (buf != NULL)
		free(buf);
	if (lstart != NULL)
		free(lstart);
	return rc;
}

static int
fill_output_buffer(bootenv_t env, char *buf, size_t buf_size_max ubi_unused,
		size_t *written)
{
	int rc = 0;
	size_t keys_size, i;
	size_t wr = 0;
	const char **keys = NULL;
	const char *val = NULL;

	rc = bootenv_get_key_vector(env, &keys_size, 1, &keys);
	if (rc != 0)
		goto err;

	for (i = 0; i < keys_size; i++) {
		if (wr > BOOTENV_MAXSIZE) {
			rc = -ENOSPC;
			goto err;
		}

		rc = bootenv_get(env, keys[i], &val);
		if (rc != 0)
			goto err;

		wr += snprintf(buf + wr, BOOTENV_MAXSIZE - wr,
				"%s=%s", keys[i], val);
		wr++; /* for \0 */
	}

	*written = wr;

err:
	if (keys != NULL)
		free(keys);

	return rc;
}

int
bootenv_write_crc(FILE* fp, bootenv_t env, uint32_t* ret_crc)
{
	int rc = 0;
	size_t size = 0;
	char *buf = NULL;
	uint32_t crc32_table[256];

	if ((fp == NULL) || (env == NULL))
		return -EINVAL;

	buf = (char*) calloc(1, BOOTENV_MAXSIZE * sizeof(char));
	if (buf == NULL)
		return -ENOMEM;


	rc = fill_output_buffer(env, buf, BOOTENV_MAXSIZE, &size);
	if (rc != 0)
		goto err;

	/* calculate crc to return */
	if (ret_crc != NULL) {
		init_crc32_table(crc32_table);
		*ret_crc = clc_crc32(crc32_table, UBI_CRC32_INIT, buf, size);
	}

	if (fwrite(buf, size, 1, fp) != 1) {
		rc = -EIO;
		goto err;
	}

err:
	if (buf != NULL)
		free(buf);
	return rc;
}

int
bootenv_write(FILE* fp, bootenv_t env)
{
	return bootenv_write_crc(fp, env, NULL);
}

int
bootenv_compare(bootenv_t first, bootenv_t second)
{
	int rc;
	size_t written_first, written_second;
	char *buf_first, *buf_second;

	if (first == NULL || second == NULL)
		return -EINVAL;

	buf_first = malloc(BOOTENV_MAXSIZE);
	if (!buf_first)
		return -ENOMEM;
	buf_second = malloc(BOOTENV_MAXSIZE);
	if (!buf_second) {
		rc = -ENOMEM;
		goto err;
	}

	rc = fill_output_buffer(first, buf_first, BOOTENV_MAXSIZE,
			&written_first);
	if (rc < 0)
		goto err;
	rc = fill_output_buffer(second, buf_second, BOOTENV_MAXSIZE,
			&written_second);
	if (rc < 0)
		goto err;

	if (written_first != written_second) {
		rc = 1;
		goto err;
	}

	rc = memcmp(buf_first, buf_second, written_first);
	if (rc != 0) {
		rc = 2;
		goto err;
	}

err:
	if (buf_first)
		free(buf_first);
	if (buf_second)
		free(buf_second);

	return rc;
}

int
bootenv_size(bootenv_t env, size_t *size)
{
	int rc = 0;
	char *buf = NULL;

	if (env == NULL)
		return -EINVAL;

	buf = (char*) calloc(1, BOOTENV_MAXSIZE * sizeof(char));
	if (buf == NULL)
		return -ENOMEM;

	rc = fill_output_buffer(env, buf, BOOTENV_MAXSIZE, size);
	if (rc != 0)
		goto err;

err:
	if (buf != NULL)
		free(buf);
	return rc;
}

int
bootenv_write_txt(FILE* fp, bootenv_t env)
{
	int rc = 0;
	size_t size, wr, i;
	const char **keys = NULL;
	const char *key = NULL;
	const char *val = NULL;

	if ((fp == NULL) || (env == NULL))
		return -EINVAL;

	rc = bootenv_get_key_vector(env, &size, 1, &keys);
	if (rc != 0)
		goto err;

	for (i = 0; i < size; i++) {
		key = keys[i];
		rc = bootenv_get(env, key, &val);
		if (rc != 0)
			goto err;

		wr = fprintf(fp, "%s=%s\n", key, val);
		if (wr != strlen(key) + strlen(val) + 2) {
			rc = -EIO;
			goto err;
		}
	}

err:
	if (keys != NULL)
		free(keys);
	return rc;
}

int
bootenv_valid(bootenv_t env ubi_unused)
{
	/* @FIXME No sanity check implemented. */
	return 0;
}

int
bootenv_copy_bootenv(bootenv_t in, bootenv_t *out)
{
	int rc = 0;
	const char *tmp = NULL;
	const char **keys = NULL;
	size_t vec_size, i;

	if ((in == NULL) || (out == NULL))
		return -EINVAL;

	/* purge output var for sure... */
	rc = bootenv_destroy(out);
	if (rc != 0)
		return rc;

	/* create the new map  */
	rc = bootenv_create(out);
	if (rc != 0)
		goto err;

	/* get the key list from the input map */
	rc = bootenv_get_key_vector(in, &vec_size, 0, &keys);
	if (rc != 0)
		goto err;

	if (vec_size != hashmap_size(in->map)) {
		rc = BOOTENV_ECOPY;
		goto err;
	}

	/* make a deep copy of the hashmap */
	for (i = 0; i < vec_size; i++) {
		rc = bootenv_get(in, keys[i], &tmp);
		if (rc != 0)
			goto err;

		rc = bootenv_set(*out, keys[i], tmp);
		if (rc != 0)
			goto err;
	}

err:
	if (keys != NULL)
		free(keys);

	return rc;
}

/* ------------------------------------------------------------------------- */


int
bootenv_pdd_keep(bootenv_t env_old, bootenv_t env_new, bootenv_t *env_res,
		 int *warnings, char *err_buf ubi_unused,
		 size_t err_buf_size ubi_unused)
{
	bootenv_list_t l_old = NULL;
	bootenv_list_t l_new = NULL;
	const char *pdd_old = NULL;
	const char *pdd_new = NULL;
	const char *tmp = NULL;
	const char **vec_old = NULL;
	const char **vec_new = NULL;
	const char **pdd_up_vec = NULL;
	size_t vec_old_size, vec_new_size, pdd_up_vec_size, i;
	int rc = 0;

	if ((env_old == NULL) || (env_new == NULL) || (env_res == NULL))
		return -EINVAL;

	/* get the pdd strings, e.g.:
	 * pdd_old=a,b,c
	 * pdd_new=a,c,d,e */
	rc = bootenv_get(env_old, "pdd", &pdd_old);
	if (rc != 0)
		goto err;
	rc = bootenv_get(env_new, "pdd", &pdd_new);
	if (rc != 0)
		goto err;

	/* put it into a list and then convert it to an vector */
	rc = bootenv_list_create(&l_old);
	if (rc != 0)
		goto err;
	rc  = bootenv_list_create(&l_new);
	if (rc != 0)
		goto err;

	rc = bootenv_list_import(l_old, pdd_old);
	if (rc != 0)
		goto err;

	rc = bootenv_list_import(l_new, pdd_new);
	if (rc != 0)
		goto err;

	rc = bootenv_list_to_vector(l_old, &vec_old_size, &vec_old);
	if (rc != 0)
		goto err;

	rc = bootenv_list_to_vector(l_new, &vec_new_size, &vec_new);
	if (rc != 0)
		goto err;

	rc = bootenv_copy_bootenv(env_new, env_res);
	if (rc != 0)
		goto err;

	/* calculate the update vector between the old and new pdd */
	pdd_up_vec = hashmap_get_update_key_vector(vec_old, vec_old_size,
			vec_new, vec_new_size, &pdd_up_vec_size);

	if (pdd_up_vec == NULL) {
		rc = -ENOMEM;
		goto err;
	}

	if (pdd_up_vec_size != 0) {
		/* need to warn the user about the unset of
		 * some pdd/bootenv values */
		*warnings = BOOTENV_WPDD_STRING_DIFFERS;

		/* remove all entries in the new bootenv load */
		for (i = 0; i < pdd_up_vec_size; i++) {
			bootenv_unset(*env_res, pdd_up_vec[i]);
		}
	}

	/* generate the keep array and copy old pdd values to new bootenv */
	for (i = 0; i < vec_old_size; i++) {
		rc = bootenv_get(env_old, vec_old[i], &tmp);
		if (rc != 0) {
			rc = BOOTENV_EPDDINVAL;
			goto err;
		}
		rc = bootenv_set(*env_res, vec_old[i], tmp);
		if (rc != 0) {
			goto err;
		}
	}
	/* put the old pdd string into the result map */
	rc = bootenv_set(*env_res, "pdd", pdd_old);
	if (rc != 0) {
		goto err;
	}


err:
	if (vec_old != NULL)
		free(vec_old);
	if (vec_new != NULL)
		free(vec_new);
	if (pdd_up_vec != NULL)
		free(pdd_up_vec);

	bootenv_list_destroy(&l_old);
	bootenv_list_destroy(&l_new);
	return rc;
}


int
bootenv_pdd_overwrite(bootenv_t env_old, bootenv_t env_new,
		      bootenv_t *env_res, int *warnings ubi_unused,
		      char *err_buf ubi_unused, size_t err_buf_size ubi_unused)
{
	if ((env_old == NULL) || (env_new == NULL) || (env_res == NULL))
		return -EINVAL;

	return bootenv_copy_bootenv(env_new, env_res);
}

int
bootenv_pdd_merge(bootenv_t env_old, bootenv_t env_new, bootenv_t *env_res,
		  int *warnings ubi_unused, char *err_buf, size_t err_buf_size)
{
	if ((env_old == NULL) || (env_new == NULL) || (env_res == NULL))
		return -EINVAL;

	snprintf(err_buf, err_buf_size, "The PDD merge operation is not "
			"implemented. Contact: <oliloh@de.ibm.com>");

	return BOOTENV_ENOTIMPL;
}

/* ------------------------------------------------------------------------- */

int
bootenv_get(bootenv_t env, const char *key, const char **value)
{
	if (env == NULL)
		return -EINVAL;

	*value = hashmap_lookup(env->map, key);
	if (*value == NULL)
		return BOOTENV_ENOTFOUND;

	return 0;
}

int
bootenv_get_num(bootenv_t env, const char *key, uint32_t *value)
{
	char *endptr = NULL;
	const char *str;

	if (env == NULL)
		return 0;

	str = hashmap_lookup(env->map, key);
	if (!str)
		return -EINVAL;

	*value = strtoul(str, &endptr, 0);

	if (*endptr == '\0') {
		return 0;
	}

	return -EINVAL;
}

int
bootenv_set(bootenv_t env, const char *key, const char *value)
{
	if (env == NULL)
		return -EINVAL;

	return hashmap_add(env->map, key, value);
}

int
bootenv_unset(bootenv_t env, const char *key)
{
	if (env == NULL)
		return -EINVAL;

	return hashmap_remove(env->map, key);
}

int
bootenv_get_key_vector(bootenv_t env, size_t* size, int sort,
		       const char ***vector)
{
	if ((env == NULL) || (size == NULL))
		return -EINVAL;

	*vector = hashmap_get_key_vector(env->map, size, sort);

	if (*vector == NULL)
		return -EINVAL;

	return 0;
}

int
bootenv_dump(bootenv_t env)
{
	if (env == NULL)
		return -EINVAL;

	return hashmap_dump(env->map);
}

int
bootenv_list_create(bootenv_list_t *list)
{
	bootenv_list_t res;
	res = (bootenv_list_t) calloc(1, sizeof(struct bootenv_list));

	if (res == NULL)
		return -ENOMEM;

	res->head = hashmap_new();

	if (res->head == NULL) {
		free(res);
		return -ENOMEM;
	}

	*list = res;
	return 0;
}

int
bootenv_list_destroy(bootenv_list_t *list)
{
	int rc = 0;

	if (list == NULL)
		return -EINVAL;

	bootenv_list_t tmp = *list;
	if (tmp == 0)
		return 0;

	rc = hashmap_free(tmp->head);
	if (rc != 0)
		return rc;

	free(tmp);
	*list = NULL;
	return 0;
}

int
bootenv_list_import(bootenv_list_t list, const char *str)
{
	if (list == NULL)
		return -EINVAL;

	return build_list_definition(list->head, str);
}

int
bootenv_list_export(bootenv_list_t list, char **string)
{
	size_t size, i, j, bufsize, tmp, rc = 0;
	const char **items;

	if (list == NULL)
		return -EINVAL;

	bufsize = BOOTENV_MAXLINE;
	char *res = (char*) malloc(bufsize * sizeof(char));
	if (res == NULL)
		return -ENOMEM;

	rc = bootenv_list_to_vector(list, &size, &items);
	if (rc != 0) {
		goto err;
	}

	j = 0;
	for (i = 0; i < size; i++) {
		tmp = strlen(items[i]);
		if (j >= bufsize) {
			bufsize += BOOTENV_MAXLINE;
			res = (char*) realloc(res, bufsize * sizeof(char));
			if (res == NULL)  {
				rc = -ENOMEM;
				goto err;
			}
		}
		memcpy(res + j, items[i], tmp);
		j += tmp;
		if (i < (size - 1)) {
			res[j] = ',';
			j++;
		}
	}
	j++;
	res[j] = '\0';
	free(items);
	*string = res;
	return 0;
err:
	free(items);
	return rc;
}

int
bootenv_list_add(bootenv_list_t list, const char *item)
{
	if ((list == NULL) || (item == NULL))
		return -EINVAL;

	return hashmap_add(list->head, item, "");
}

int
bootenv_list_remove(bootenv_list_t list, const char *item)
{
	if ((list == NULL) || (item == NULL))
		return -EINVAL;

	return hashmap_remove(list->head, item);
}

int
bootenv_list_is_in(bootenv_list_t list, const char *item)
{
	if ((list == NULL) || (item == NULL))
		return -EINVAL;

	return hashmap_lookup(list->head, item) != NULL ? 1 : 0;
}

int
bootenv_list_to_vector(bootenv_list_t list, size_t *size, const char ***vector)
{
	if ((list == NULL) || (size == NULL))
		return -EINVAL;

	*vector = hashmap_get_key_vector(list->head, size, 1);
	if (*vector == NULL)
		return -ENOMEM;

	return 0;
}

int
bootenv_list_to_num_vector(bootenv_list_t list, size_t *size,
		uint32_t **vector)
{
	int rc = 0;
	size_t i;
	uint32_t* res = NULL;
	char *endptr = NULL;
	const char **a = NULL;

	rc = bootenv_list_to_vector(list, size, &a);
	if (rc != 0)
		goto err;

	res = (uint32_t*) malloc (*size * sizeof(uint32_t));
	if (!res)
		goto err;

	for (i = 0; i < *size; i++) {
		res[i] = strtoul(a[i], &endptr, 0);
		if (*endptr != '\0')
			goto err;
	}

	if (a)
		free(a);
	*vector = res;
	return 0;

err:
	if (a)
		free(a);
	if (res)
		free(res);
	return rc;
}
