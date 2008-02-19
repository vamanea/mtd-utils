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
 * @file pfi.c
 *
 * @author Oliver Lohmann
 *	   Andreas Arnez
 *	   Joern Engel
 *	   Frank Haverkamp
 *
 * @brief libpfi holds all code to create and process pfi files.
 *
 * <oliloh@de.ibm.com> Wed Feb	8 11:38:22 CET 2006: Initial creation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <errno.h>

#include "pfi.h"

#define PFI_MAGIC     "PFI!\n"
#define PFI_DATA      "DATA\n" /* The same size as PFI_MAGIC */
#define PFI_MAGIC_LEN 5

static const char copyright [] __attribute__((unused)) =
	"Copyright (c) International Business Machines Corp., 2006";

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
	key_raw_starts,
	key_raw_total_size,
	num_keys,
};

struct pfi_header {
	char defined[num_keys];	 /* reserve all possible keys even if
				    version does not require this. */
	int mode_no;		 /* current mode no. -> can only increase */
	union {
		char *str;
		uint32_t num;
	} value[num_keys];
};


#define PFI_MANDATORY	    0x0001
#define PFI_STRING	    0x0002
#define PFI_LISTVALUE	    0x0004	/* comma seperated list of nums */
#define PFI_MANDATORY_UBI   0x0008
#define PFI_MANDATORY_RAW   0x0010

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
	{ key_raw_starts, "raw_starts", PFI_MANDATORY_RAW | PFI_STRING },
	{ key_raw_total_size, "raw_total_size", PFI_MANDATORY_RAW },
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

static const char* modes[] = {"raw", "ubi"}; /* order isn't arbitrary! */

/* latest version contains all possible keys */
static const struct key_descriptor *key_desc = key_desc_v1;

#define PFI_IS_UBI(mode) \
	(((mode) != NULL) && (strcmp("ubi", (mode)) == 0))

#define PFI_IS_RAW(mode) \
	(((mode) != NULL) && (strcmp("raw", (mode)) == 0))

/**
 * @return	 <0	On Error.
 *		>=0	Mode no.
 */
static int
get_mode_no(const char* mode)
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
check_valid (pfi_header head)
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
	else if (PFI_IS_RAW(mode)) {
		to_check |= PFI_MANDATORY_RAW;
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

int pfi_header_init (pfi_header *head)
{
	int i;
	pfi_header self = (pfi_header) malloc(sizeof(*self));

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

int pfi_header_destroy (pfi_header *head)
{
	int i;
	pfi_header self = *head;

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

int pfi_header_setnumber (pfi_header head,
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

int pfi_header_setvalue (pfi_header head,
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

int pfi_header_getnumber (pfi_header head,
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

int pfi_header_getstring (pfi_header head,
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

int pfi_header_write (FILE *out, pfi_header head)
{
	int i;
	int ret;

	pfi_header_setnumber(head, "version", PFI_HDRVERSION);

	if ((ret = check_valid(head)) != 0)
		return ret;

	/* OK.	Now write the header. */

	ret = fwrite(PFI_MAGIC, 1, PFI_MAGIC_LEN, out);
	if (ret < PFI_MAGIC_LEN)
		return ret;


	for (i = 0; i < num_keys; i++) {
		if (!head->defined[i])
			continue;

		ret = fprintf(out, "%s=", key_desc[i].name);
		if (ret < 0)
			return PFI_EFILE;

		if (key_desc[i].flags & PFI_STRING) {
			ret = fprintf(out, "%s", head->value[i].str);
			if (ret < 0)
				return PFI_EFILE;
		} else {
			ret = fprintf(out, "0x%8x", head->value[i].num);
			if (ret < 0)
				return PFI_EFILE;

		}
		ret = fprintf(out, "\n");
		if (ret < 0)
			return PFI_EFILE;
	}
	ret = fprintf(out, "\n");
	if (ret < 0)
		return PFI_EFILE;

	ret = fflush(out);
	if (ret != 0)
		return PFI_EFILE;

	return 0;
}

int pfi_header_read (FILE *in, pfi_header head)
{
	char magic[PFI_MAGIC_LEN];
	char mode[PFI_KEYWORD_LEN];
	char buf[256];

	if (PFI_MAGIC_LEN != fread(magic, 1, PFI_MAGIC_LEN, in))
		return PFI_EFILE;
	if (memcmp(magic, PFI_MAGIC, PFI_MAGIC_LEN) != 0)  {
		if (memcmp(magic, PFI_DATA, PFI_MAGIC_LEN) == 0) {
			return PFI_DATA_START;
		}
		return PFI_ENOHEADER;
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

int pfi_header_dump (FILE *out, pfi_header head __attribute__((__unused__)))
{
	fprintf(out, "Sorry not implemented yet. Write mail to "
		"Andreas Arnez and complain!\n");
	return 0;
}

int pfi_read (FILE *in, pfi_read_func func, void *priv_data)
{
	int rc;
	pfi_header header;

	rc = pfi_header_init (&header);
	if (0 != rc)
		return rc;
	if (!func)
		return PFI_EINVAL;

	while ((0 == rc) && !feof(in)) {
		/*
		 * Read header and check consistency of the fields.
		 */
		rc = pfi_header_read( in, header );
		if (0 != rc)
			break;
		if (func) {
			rc = func(in, header, priv_data);
			if (rc != 0)
				break;
		}
	}

	pfi_header_destroy(&header);
	return rc;
}
