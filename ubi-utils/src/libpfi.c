/*
 * Copyright (c) International Business Machines Corp., 2006
 * Copyright (C) 2008 Nokia Corporation
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
 * A library to work with pfi files.
 *
 * Authors Oliver Lohmann
 *         Andreas Arnez
 *         Joern Engel
 *         Frank Haverkamp
 *         Artem Bityutskiy
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <mtd/ubi-header.h>
#include <libpfi.h>
#include "common.h"
#include "bootenv.h"

#define PROGRAM_NAME "libpfi"

#define PFI_MAGIC     "PFI!\n"
#define PFI_MAGIC_LEN (sizeof(PFI_MAGIC) - 1)
#define PFI_DATA      "DATA\n"
















#define PFI_MANDATORY     0x0001
#define PFI_STRING        0x0002
#define PFI_LISTVALUE     0x0004
#define PFI_MANDATORY_UBI 0x0008

enum key_id {
	/* version 1 */
	key_version,	      /* must be index position 0! */
	key_mode,
	key_size,
	key_crc,
	key_label,
	key_flags,
	key_ubi_ids,
	key_ubi_size,
	key_ubi_type,
	key_ubi_names,
	key_ubi_alignment,
	num_keys,
};

struct pfi_header {
	uint8_t defined[num_keys];	 /* reserve all possible keys even if
				    version does not require this. */
	int mode_no;		 /* current mode no. -> can only increase */
	union {
		char *str;
		uint32_t num;
	} value[num_keys];
};

struct key_descriptor {
	enum key_id id;
	const char *name;
	uint32_t flags;
};

static const struct key_descriptor key_desc_v1[] = {
	{ key_version, "version", PFI_MANDATORY },
	{ key_mode, "mode", PFI_MANDATORY | PFI_STRING },
	{ key_size, "size", PFI_MANDATORY },
	{ key_crc, "crc", PFI_MANDATORY },
	{ key_label, "label", PFI_MANDATORY | PFI_STRING },
	{ key_flags, "flags", PFI_MANDATORY },
	{ key_ubi_ids, "ubi_ids", PFI_MANDATORY_UBI | PFI_STRING },
	{ key_ubi_size, "ubi_size", PFI_MANDATORY_UBI },
	{ key_ubi_type, "ubi_type", PFI_MANDATORY_UBI | PFI_STRING },
	{ key_ubi_names, "ubi_names", PFI_MANDATORY_UBI | PFI_STRING },
	{ key_ubi_alignment, "ubi_alignment", PFI_MANDATORY_UBI },
};

static const struct key_descriptor *key_descriptors[] = {
	NULL,
	key_desc_v1,					   /* version 1 */
};

static const int key_descriptors_max[] = {
	0,						   /* version 0 */
	sizeof(key_desc_v1)/sizeof(struct key_descriptor), /* version 1 */
};

#define ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))

static const char* modes[] = {"ubi"};

/* latest version contains all possible keys */
static const struct key_descriptor *key_desc = key_desc_v1;

#define PFI_IS_UBI(mode) \
	(((mode) != NULL) && (strcmp("ubi", (mode)) == 0))

static int get_mode_no(const char *mode)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(modes); i++)
		if (strcmp(mode, modes[i]) == 0)
			return i;
	return -1;
}

static int
find_key_by_name (const char *name)
{
	int i;

	for (i = 0; i < num_keys; i++) {
		if (strcmp(name, key_desc[i].name) == 0)
			return i;
	}
	return -1;
}

static int
check_valid (struct pfi_header *head)
{
	int i;
	int max_keys;
	uint32_t version;
	const char *mode;
	const struct key_descriptor *desc;
	uint32_t to_check = PFI_MANDATORY;

	/*
	 * For the validity check the list of possible keys depends on
	 * the version of the PFI file used.
	 */
	version = head->value[key_version].num;
	if (version > PFI_HDRVERSION)
		return PFI_ENOHEADER;

	max_keys = key_descriptors_max[version];
	desc = key_descriptors[version];

	if (!desc)
		return PFI_ENOVERSION;

	mode = head->value[key_mode].str;
	if (PFI_IS_UBI(mode)) {
		to_check |= PFI_MANDATORY_UBI;
	}
	else { /* neither UBI nor RAW == ERR */
		return PFI_EINSUFF;
	}

	for (i = 0; i < max_keys; i++) {
		if ((desc[i].flags & to_check) && !head->defined[i]) {
			fprintf(stderr, "libpfi: %s missing\n", desc[i].name);
			return PFI_EINSUFF;
		}
	}

	return 0;
}

int pfi_header_init (struct pfi_header **head)
{
	int i;
	struct pfi_header *self = malloc(sizeof(*self));

	*head = self;
	if (self == NULL)
		return PFI_ENOMEM;

	/* initialize maximum number of possible keys */
	for (i = 0; i < num_keys; i++) {
		memset(self, 0, sizeof(*self));
		self->defined[i] = 0;
	}

	return 0;
}

int pfi_header_destroy (struct pfi_header **head)
{
	int i;
	struct pfi_header *self = *head;

	for (i = 0; i < num_keys; i++) {
		if (self->defined[i] && (key_desc[i].flags & PFI_STRING) &&
		    self->value[i].str) {
			free(self->value[i].str);
		}
	}
	free(*head);
	*head = NULL;
	return 0;
}

int pfi_header_setnumber (struct pfi_header *head,
			   const char *key, uint32_t value)
{
	int key_id = find_key_by_name(key);

	if (key_id < 0)
		return PFI_EUNDEF;

	if (key_desc[key_id].flags & PFI_STRING)
		return PFI_EBADTYPE;

	head->value[key_id].num = value;
	head->defined[key_id] = 1;
	return 0;
}

int pfi_header_setvalue (struct pfi_header *head,
			  const char *key, const char *value)
{
	int key_id = find_key_by_name(key);

	if (value == NULL)
		return PFI_EINSUFF;

	if ((key_id < 0) || (key_id >= num_keys))
		return PFI_EUNDEF;

	if (key_desc[key_id].flags & PFI_STRING) {
		/*
		 * The value is a string. Copy to a newly allocated
		 * buffer. Delete the old value, if already set.
		 */
		size_t len = strlen(value) + 1;
		char *old_str = NULL;
		char *str;

		old_str = head->value[key_id].str;
		if (old_str != NULL)
			free(old_str);

		str = head->value[key_id].str = (char *) malloc(len);
		if (str == NULL)
			return PFI_ENOMEM;

		strcpy(str, value);
	} else {
		int len;
		int ret;
		/* FIXME: here we assume that the value is always
		   given in hex and starts with '0x'. */
		ret = sscanf(value, "0x%x%n", &head->value[key_id].num, &len);
		if (ret < 1 || value[len] != '\0')
			return PFI_EBADTYPE;
	}
	head->defined[key_id] = 1;
	return 0;
}

int pfi_header_getnumber (struct pfi_header *head,
			   const char *key, uint32_t *value)
{
	int key_id = find_key_by_name(key);

	if (key_id < 0)
		return PFI_EUNDEF;

	if (key_desc[key_id].flags & PFI_STRING)
		return PFI_EBADTYPE;

	if (!head->defined[key_id])
		return PFI_EUNDEF;

	*value = head->value[key_id].num;
	return 0;
}

int pfi_header_getstring (struct pfi_header *head,
			   const char *key, char *value, size_t size)
{
	int key_id = find_key_by_name(key);

	if (key_id < 0)
		return PFI_EUNDEF;

	if (!(key_desc[key_id].flags & PFI_STRING))
		return PFI_EBADTYPE;

	if (!head->defined[key_id])
		return PFI_EUNDEF;

	strncpy(value, head->value[key_id].str, size-1);
	value[size-1] = '\0';
	return 0;
}

int pfi_header_read(FILE *in, struct pfi_header *head)
{
	char mode[PFI_KEYWORD_LEN];
	char buf[256];

	if (fread(buf, 1, PFI_MAGIC_LEN, in) != PFI_MAGIC_LEN) {
		errmsg("cannot read %d bytes", PFI_MAGIC_LEN);
		perror("fread");
		return -1;
	}

	if (memcmp(buf, PFI_MAGIC, PFI_MAGIC_LEN) != 0)  {
		if (memcmp(buf, PFI_DATA, PFI_MAGIC_LEN) == 0)
			return 1;

		errmsg("PFI magic \"%s\" not found", PFI_MAGIC);
		return -1;
	}

	while (fgets(buf, sizeof(buf), in) != NULL && buf[0] != '\n') {
		char *value;
		char *end;
		value = strchr(buf, '=');
		if (value == NULL)
			return PFI_ENOHEADER;

		*value = '\0';
		value++;
		end = strchr(value, '\n');
		if (end)
		       *end = '\0';

		if (pfi_header_setvalue(head, buf, value))
			return PFI_ENOHEADER;
	}

	if (check_valid(head) != 0)
		return PFI_ENOHEADER;

	/* set current mode no. in head */
	pfi_header_getstring(head, "mode", mode, PFI_KEYWORD_LEN);
	if (head->mode_no > get_mode_no(mode)) {
		return PFI_EMODE;
	}
	head->mode_no = get_mode_no(mode);
	return 0;
}
int
read_pfi_ubi(struct pfi_header *pfi_hd, struct pfi_ubi **pfi_ubi,
	     const char *label)
{
	int err = 0;
	const char** tmp_names = NULL;
	char tmp_str[PFI_KEYWORD_LEN];
	bootenv_list_t ubi_id_list = NULL;
	bootenv_list_t ubi_name_list = NULL;
	struct pfi_ubi *res;
	uint32_t i;
	size_t size;

	res = (struct pfi_ubi *) calloc(1, sizeof(struct pfi_ubi));
	if (!res)
		return -ENOMEM;

	err = pfi_header_getnumber(pfi_hd, "size", &(res->data_size));
	if (err != 0) {
		errmsg("cannot read 'size' from PFI.");
		goto err;
	}

	err = pfi_header_getnumber(pfi_hd, "crc", &(res->crc));
	if (err != 0) {
		errmsg("cannot read 'crc' from PFI.");
		goto err;
	}

	err = pfi_header_getstring(pfi_hd, "ubi_ids", tmp_str, PFI_KEYWORD_LEN);
	if (err != 0) {
		errmsg("cannot read 'ubi_ids' from PFI.");
		goto err;
	}

	err = bootenv_list_create(&ubi_id_list);
	if (err != 0) {
		goto err;
	}
	err = bootenv_list_create(&ubi_name_list);
	if (err != 0) {
		goto err;
	}

	err = bootenv_list_import(ubi_id_list, tmp_str);
	if (err != 0) {
		errmsg("cannot translate PFI value: %s", tmp_str);
		goto err;
	}

	err = bootenv_list_to_num_vector(ubi_id_list, &size,
					&(res->ids));
	res->ids_size = size;
	if (err != 0) {
		errmsg("cannot create numeric value array: %s", tmp_str);
		goto err;
	}

	if (res->ids_size == 0) {
		err = -1;
		errmsg("sanity check failed: No ubi_ids specified.");
		goto err;
	}

	err = pfi_header_getstring(pfi_hd, "ubi_type",
				  tmp_str, PFI_KEYWORD_LEN);
	if (err != 0) {
		errmsg("cannot read 'ubi_type' from PFI.");
		goto err;
	}
	if (strcmp(tmp_str, "static") == 0)
		res->vol_type = UBI_VID_STATIC;
	else if (strcmp(tmp_str, "dynamic") == 0)
		res->vol_type = UBI_VID_DYNAMIC;
	else {
		errmsg("unknown ubi_type in PFI.");
		goto err;
	}

	err = pfi_header_getnumber(pfi_hd, "ubi_alignment", &(res->alignment));
	if (err != 0) {
		errmsg("cannot read 'ubi_alignment' from PFI.");
		goto err;
	}

	err = pfi_header_getnumber(pfi_hd, "ubi_size", &(res->size));
	if (err != 0) {
		errmsg("cannot read 'ubi_size' from PFI.");
		goto err;
	}

	err = pfi_header_getstring(pfi_hd, "ubi_names",
				  tmp_str, PFI_KEYWORD_LEN);
	if (err != 0) {
		errmsg("cannot read 'ubi_names' from PFI.");
		goto err;
	}

	err = bootenv_list_import(ubi_name_list, tmp_str);
	if (err != 0) {
		errmsg("cannot translate PFI value: %s", tmp_str);
		goto err;
	}
	err = bootenv_list_to_vector(ubi_name_list, &size,
				    &(tmp_names));
	res->names_size = size;
	if (err != 0) {
		errmsg("cannot create string array: %s", tmp_str);
		goto err;
	}

	if (res->names_size != res->ids_size) {
		errmsg("sanity check failed: ubi_ids list does not match "
			 "sizeof ubi_names list.");
		err = -1;
	}

	/* copy tmp_names to own structure */
	res->names = calloc(1, res->names_size * sizeof (char*));
	if (res->names == NULL)
		goto err;

	for (i = 0; i < res->names_size; i++) {
		res->names[i] = calloc(PFI_UBI_VOL_NAME_LEN + 1, sizeof(char));
		if (res->names[i] == NULL)
			goto err;
		strncpy(res->names[i], tmp_names[i], PFI_UBI_VOL_NAME_LEN + 1);
	}

	goto out;

 err:
	if (res) {
		if (res->names) {
			for (i = 0; i < res->names_size; i++) {
				if (res->names[i]) {
					free(res->names[i]);
				}
			}
			free(res->names);
		}
		if (res->ids) {
			free(res->ids);
		}
		free(res);
		res = NULL;
	}

 out:
	bootenv_list_destroy(&ubi_id_list);
	bootenv_list_destroy(&ubi_name_list);
	if (tmp_names != NULL)
		free(tmp_names);
	*pfi_ubi = res;
	return err;
}

int
free_pfi_ubi(struct pfi_ubi **pfi_ubi)
{
	size_t i;
	struct pfi_ubi *tmp = *pfi_ubi;
	if (tmp) {
		if (tmp->ids)
			free(tmp->ids);
		if (tmp->names) {
			for (i = 0; i < tmp->names_size; i++) {
				if (tmp->names[i]) {
					free(tmp->names[i]);
				}
			}
			free(tmp->names);
		}
		free(tmp);
	}
	*pfi_ubi = NULL;

	return 0;
}

int read_pfi_headers(struct list_entry **ubi_list, FILE *fp_pfi)
{
	int err = 0;
	long long data_offs = 0;
	long fpos;
	char mode[PFI_KEYWORD_LEN];
	char label[PFI_LABEL_LEN];
	struct list_entry *tmp;

	*ubi_list = list_empty(); struct pfi_ubi *ubi = NULL;
	struct pfi_header *pfi_header = NULL;

	/* read all headers from PFI and store them in lists */
	err = pfi_header_init(&pfi_header);
	if (err != 0) {
		errmsg("cannot initialize pfi header.");
		goto err;
	}
	while ((err == 0) && !feof(fp_pfi)) {
		err = pfi_header_read(fp_pfi, pfi_header);
		if (err != 0) {
			if (err == 1) {
				err = 0;
				break; /* data section starts,
					  all headers read */
			}
			else {
				goto err;
			}
		}
		err = pfi_header_getstring(pfi_header, "label", label,
					  PFI_LABEL_LEN);
		if (err != 0) {
			errmsg("cannot read 'label' from PFI.");
			goto err;
		}
		err = pfi_header_getstring(pfi_header, "mode", mode,
					  PFI_KEYWORD_LEN);
		if (err != 0) {
			errmsg("cannot read 'mode' from PFI.");
			goto err;
		}
		if (strcmp(mode, "ubi") == 0) {
			err = read_pfi_ubi(pfi_header, &ubi, label);
			if (err != 0) {
				goto err;
			}
			*ubi_list = append_elem(ubi, *ubi_list);
		}
		else {
			errmsg("recvieved unknown mode from PFI: %s", mode);
			goto err;
		}
		ubi->data_offs = data_offs;
		data_offs += ubi->data_size;
	}

	fpos = ftell(fp_pfi);
	if (fpos == -1) {
		errmsg("ftell returned error");
		perror("ftell");
		goto err;
	}

	list_for_each(ubi, tmp, *ubi_list)
		ubi->data_offs += fpos;

	goto out;

 err:
	*ubi_list = remove_all((free_func_t)&free_pfi_ubi, *ubi_list);
 out:
	pfi_header_destroy(&pfi_header);
	return err;

}
