/*
 * Copyright International Business Machines Corp., 2006, 2007
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
 * Authors: Oliver Lohmann <oliloh@de.ibm.com>
 *	    Drake Dowsett <dowsett@de.ibm.com>
 * Contact: Andreas Arnez <anrez@de.ibm.com>
 */

/* TODO Compare data before writing it. This implies that the volume
 * parameters are compared first: size, alignment, name, type, ...,
 * this is the same, compare the data. Volume deletion is deffered
 * until the difference has been found out.
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#define __USE_GNU
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/ioctl.h>

#include <libubi.h>
#include <pfiflash.h>

#include <mtd/ubi-user.h>	/* FIXME Is this ok here? */
#include <mtd/mtd-user.h>

#include "pfiflash_error.h"
#include "ubimirror.h"
#include "error.h"
#include "reader.h"
#include "example_ubi.h"
#include "bootenv.h"

/* ubi-header.h and crc32.h needed for CRC checking */
#include <mtd/ubi-header.h>	/* FIXME Is this ok here? */
#include "crc32.h"

#define ubi_unused __attribute__((unused))

#define COMPARE_BUFFER_SIZE 2048

#define DEFAULT_DEV_PATTERN    "/dev/ubi%d"
#define DEFAULT_VOL_PATTERN    "/dev/ubi%d_%d"

static const char copyright [] ubi_unused =
	"Copyright International Business Machines Corp., 2006, 2007";

/* simply clear buffer, then write into front of it */
#define EBUF(fmt...)							\
		snprintf(err_buf, err_buf_size, fmt);

/* make a history of buffer and then prepend something in front */
#define EBUF_PREPEND(fmt)						\
	do {								\
		int EBUF_HISTORY_LENGTH = strlen(err_buf);		\
		char EBUF_HISTORY[EBUF_HISTORY_LENGTH + 1];		\
		strncpy(EBUF_HISTORY, err_buf, EBUF_HISTORY_LENGTH + 1);\
		EBUF(fmt ": %s", EBUF_HISTORY);				\
	} while (0)

/* An array of PDD function pointers indexed by the algorithm. */
static pdd_func_t pdd_funcs[PDD_HANDLING_NUM]  =
	{
		&bootenv_pdd_keep,
		&bootenv_pdd_merge,
		&bootenv_pdd_overwrite
	};

typedef enum ubi_update_process_t {
	UBI_REMOVE = 0,
	UBI_WRITE,
	UBI_COMPARE,
} ubi_update_process_t;


/**
 * skip_raw_volumes - reads data from pfi to advance fp past raw block
 * @pfi:	fp to pfi data
 * @pfi_raws:	header information
 *
 * Error handling):
 *	when early EOF in pfi data
 *	- returns -PFIFLASH_ERR_EOF, err_buf matches text to err
 *	when file I/O error
 *	- returns -PFIFLASH_ERR_FIO, err_buf matches text to err
 **/
static int
skip_raw_volumes(FILE* pfi, list_t pfi_raws,
		  char* err_buf, size_t err_buf_size)
{
	int rc;
	void *i;
	list_t ptr;

	if (is_empty(pfi_raws))
		return 0;

	rc = 0;
	foreach(i, ptr, pfi_raws) {
		size_t j;
		pfi_raw_t raw;

		raw = (pfi_raw_t)i;
		for(j = 0; j < raw->data_size; j++) {
			int c;

			c = fgetc(pfi);
			if (c == EOF)
				rc = -PFIFLASH_ERR_EOF;
			else if (ferror(pfi))
				rc = -PFIFLASH_ERR_FIO;

			if (rc != 0)
				goto err;
		}
	}

 err:
	EBUF(PFIFLASH_ERRSTR[-rc]);
	return rc;
}


/**
 * my_ubi_mkvol - wraps the ubi_mkvol functions and impl. bootenv update hook
 * @devno:	UBI device number.
 * @s:		Current seqnum.
 * @u:		Information about the UBI volume from the PFI.
 *
 * Error handling:
 *	when UBI system couldn't be opened
 *	- returns -PFIFLASH_ERR_UBI_OPEN, err_buf matches text to err
 *	when UBI system couldn't create a volume
 *	- returns -PFIFLASH_ERR_UBI_MKVOL, err_buf matches text to err
 **/
static int
my_ubi_mkvol(int devno, int s, pfi_ubi_t u,
	     char *err_buf, size_t err_buf_size)
{
	int rc, type;
	char path[PATH_MAX];
	libubi_t ulib;
	struct ubi_mkvol_request req;

	rc = 0;
	ulib = NULL;

	log_msg("[ ubimkvol id=%d, size=%d, data_size=%d, type=%d, "
		"alig=%d, nlen=%d, name=%s",
		u->ids[s], u->size, u->data_size, u->type, u->alignment,
		strnlen(u->names[s], PFI_UBI_VOL_NAME_LEN), u->names[s]);

	ulib = libubi_open();
	if (ulib == NULL) {
		rc = -PFIFLASH_ERR_UBI_OPEN;
		EBUF(PFIFLASH_ERRSTR[-rc]);
		goto err;
	}

	switch (u->type) {
	case pfi_ubi_static:
		type = UBI_STATIC_VOLUME; break;
	case pfi_ubi_dynamic:
	default:
		type = UBI_DYNAMIC_VOLUME;
	}

	snprintf(path, PATH_MAX, DEFAULT_DEV_PATTERN, devno);

	req.vol_id = u->ids[s];
	req.alignment = u->alignment;
	req.bytes = u->size;
	req.vol_type = type;
	req.name = u->names[s];

	rc = ubi_mkvol(ulib, path, &req);
	if (rc != 0) {
		rc = -PFIFLASH_ERR_UBI_MKVOL;
		EBUF(PFIFLASH_ERRSTR[-rc], u->ids[s]);
		goto err;
	}

 err:
	if (ulib != NULL)
		libubi_close(ulib);

	return rc;
}


/**
 * my_ubi_rmvol - a wrapper around the UBI library function ubi_rmvol
 * @devno	UBI device number
 * @id		UBI volume id to remove
 *
 * If the volume does not exist, the function will return success.
 *
 * Error handling:
 *	when UBI system couldn't be opened
 *	- returns -PFIFLASH_ERR_UBI_OPEN, err_buf matches text to err
 *	when UBI system couldn't update (truncate) a volume
 *	- returns -PFIFLASH_ERR_UBI_VOL_UPDATE, err_buf matches text to err
 *	when UBI system couldn't remove a volume
 *	- returns -PFIFLASH_ERR_UBI_RMVOL, err_buf matches text to err
 **/
static int
my_ubi_rmvol(int devno, uint32_t id,
	     char *err_buf, size_t err_buf_size)
{
	int rc, fd;
	char path[PATH_MAX];
	libubi_t ulib;

	rc = 0;
	ulib = NULL;

	log_msg("[ ubirmvol id=%d", id);

	ulib = libubi_open();
	if (ulib == NULL) {
		rc = -PFIFLASH_ERR_UBI_OPEN;
		EBUF(PFIFLASH_ERRSTR[-rc]);
		goto err;
	}

	snprintf(path, PATH_MAX, DEFAULT_VOL_PATTERN, devno, id);

	/* truncate whether it exist or not */
	fd = open(path, O_RDWR);
	if (fd < 0) {
		libubi_close(ulib);
		return 0;	/* not existent, return 0 */
	}

	rc = ubi_update_start(ulib, fd, 0);
	close(fd);
	if (rc < 0) {
		rc = -PFIFLASH_ERR_UBI_VOL_UPDATE;
		EBUF(PFIFLASH_ERRSTR[-rc], id);
		goto err;	/* if EBUSY than empty device, continue */
	}

	snprintf(path, PATH_MAX, DEFAULT_DEV_PATTERN, devno);

	rc = ubi_rmvol(ulib, path, id);
	if (rc != 0) {
#ifdef DEBUG
		int rc_old = rc;
		dbg_msg("Remove UBI volume %d returned with error: %d "
			"errno=%d", id, rc_old, errno);
#endif

		rc = -PFIFLASH_ERR_UBI_RMVOL;
		EBUF(PFIFLASH_ERRSTR[-rc], id);

		/* TODO Define a ubi_rmvol return value which says
		 * sth like EUBI_NOSUCHDEV. In this case, a failed
		 * operation is acceptable. Everything else has to be
		 * classified as real error. But talk to Andreas Arnez
		 * before defining something odd...
		 */
		/* if ((errno == EINVAL) || (errno == ENODEV))
		   return 0; */ /* currently it is EINVAL or ENODEV */

		goto err;
	}

 err:
	if (ulib != NULL)
		libubi_close(ulib);

	return rc;
}


/**
 * read_bootenv_volume - reads the current bootenv data from id into be_old
 * @devno	UBI device number
 * @id		UBI volume id to remove
 * @bootenv_old	to hold old boot_env data
 *
 * Error handling:
 *	when UBI system couldn't be opened
 *	- returns -PFIFLASH_ERR_UBI_OPEN, err_buf matches text to err
 *	when UBI system couldn't open a volume to read
 *	- returns -PFIFLASH_ERR_UBI_VOL_FOPEN, err_buf matches text to err
 *	when couldn't read bootenv data
 *	- returns -PFIFLASH_ERR_BOOTENV_READ, err_buf matches text to err
 **/
static int
read_bootenv_volume(int devno, uint32_t id, bootenv_t bootenv_old,
		    char *err_buf, size_t err_buf_size)
{
	int rc;
	FILE* fp_in;
	char path[PATH_MAX];
	libubi_t ulib;

	rc = 0;
	fp_in = NULL;
	ulib = NULL;

	ulib = libubi_open();
	if (ulib == NULL) {
		rc = -PFIFLASH_ERR_UBI_OPEN;
		EBUF(PFIFLASH_ERRSTR[-rc]);
		goto err;
	}

	snprintf(path, PATH_MAX, DEFAULT_VOL_PATTERN, devno, id);

	fp_in = fopen(path, "r");
	if (!fp_in) {
		rc = -PFIFLASH_ERR_UBI_VOL_FOPEN;
		EBUF(PFIFLASH_ERRSTR[-rc], id);
		goto err;
	}

	log_msg("[ reading old bootenvs ...");

	/* Save old bootenvs for reference */
	rc = bootenv_read(fp_in, bootenv_old, BOOTENV_MAXSIZE);
	if (rc != 0) {
		rc = -PFIFLASH_ERR_BOOTENV_READ;
		EBUF(PFIFLASH_ERRSTR[-rc]);
		goto err;
	}

 err:
	if (fp_in)
		fclose(fp_in);
	if (ulib)
		libubi_close(ulib);

	return rc;
}


/**
 * write_bootenv_volume - writes data from PFI file int to bootenv UBI volume
 * @devno	UBI device number
 * @id		UBI volume id
 * @bootend_old	old PDD data from machine
 * @pdd_f	function to handle PDD with
 * @fp_in	new pdd data contained in PFI
 * @fp_in_size	data size of new pdd data in PFI
 * @pfi_crc	crc value from PFI header
 *
 * Error handling:
 *	when UBI system couldn't be opened
 *	- returns -PFIFLASH_ERR_UBI_OPEN, err_buf matches text to err
 *	when bootenv can't be created
 *	- returns -PFIFLASH_ERR_BOOTENV_CREATE, err_buf matches text to err
 *	when bootenv can't be read
 *	- returns -PFIFLASH_ERR_BOOTENV_READ, err_buf matches text to err
 *	when PDD handling function returns and error
 *	- passes rc and err_buf data
 *	when CRC check fails
 *	- returns -PFIFLASH_ERR_CRC_CHECK, err_buf matches text to err
 *	when bootenv can't be resized
 *	- returns -PFIFLASH_ERR_BOOTENV_SIZE, err_buf matches text to err
 *	when UBI system couldn't open a volume
 *	- returns -PFIFLASH_ERR_UBI_VOL_FOPEN, err_buf matches text to err
 *	when couldn't write bootenv data
 *	- returns -PFIFLASH_ERR_BOOTENV_WRITE, err_buf matches text to err
 **/
static int
write_bootenv_volume(int devno, uint32_t id, bootenv_t bootenv_old,
		     pdd_func_t pdd_f, FILE* fp_in, size_t fp_in_size,
		     uint32_t pfi_crc,
		     char *err_buf, size_t err_buf_size)
{
	int rc, warnings, fd_out;
	uint32_t crc;
	char path[PATH_MAX];
	size_t update_size;
	FILE *fp_out;
	bootenv_t bootenv_new, bootenv_res;
	libubi_t ulib;

	rc = 0;
	warnings = 0;
	crc = 0;
	update_size = 0;
	fp_out = NULL;
	bootenv_new = NULL;
	bootenv_res = NULL;
	ulib = NULL;

	log_msg("[ ubiupdatevol bootenv id=%d, fp_in=%p", id, fp_in);

	/* Workflow:
	 * 1. Apply PDD operation and get the size of the returning
	 *    bootenv_res section. Without the correct size it wouldn't
	 *    be possible to call UBI update vol.
	 * 2. Call UBI update vol
	 * 3. Get FILE* to vol dev
	 * 4. Write to FILE*
	 */

	ulib = libubi_open();
	if (ulib == NULL) {
		rc = -PFIFLASH_ERR_UBI_OPEN;
		EBUF(PFIFLASH_ERRSTR[-rc]);
		goto err;
	}

	rc = bootenv_create(&bootenv_new);
	if (rc != 0) {
		rc = -PFIFLASH_ERR_BOOTENV_CREATE;
		EBUF(PFIFLASH_ERRSTR[-rc], " 'new'");
		goto err;
	}

	rc = bootenv_create(&bootenv_res);
	if (rc != 0) {
		rc = -PFIFLASH_ERR_BOOTENV_CREATE;
		EBUF(PFIFLASH_ERRSTR[-rc], " 'res'");
		goto err;
	}

	rc = bootenv_read_crc(fp_in, bootenv_new, fp_in_size, &crc);
	if (rc != 0) {
		rc = -PFIFLASH_ERR_BOOTENV_READ;
		EBUF(PFIFLASH_ERRSTR[-rc]);
		goto err;
	} else if (crc != pfi_crc) {
		rc = -PFIFLASH_ERR_CRC_CHECK;
		EBUF(PFIFLASH_ERRSTR[-rc], pfi_crc, crc);
		goto err;
	}

	rc = pdd_f(bootenv_old, bootenv_new, &bootenv_res, &warnings,
		   err_buf, err_buf_size);
	if (rc != 0) {
		EBUF_PREPEND("handling PDD");
		goto err;
	}
	else if (warnings)
		/* TODO do something with warnings */
		dbg_msg("A warning in the PDD operation occured: %d",
			warnings);

	rc = bootenv_size(bootenv_res, &update_size);
	if (rc != 0) {
		rc = -PFIFLASH_ERR_BOOTENV_SIZE;
		EBUF(PFIFLASH_ERRSTR[-rc]);
		goto err;
	}

	snprintf(path, PATH_MAX, DEFAULT_VOL_PATTERN, devno, id);

	fd_out = open(path, O_RDWR);
	if (fd_out < 0) {
		rc = -PFIFLASH_ERR_UBI_VOL_FOPEN;
		EBUF(PFIFLASH_ERRSTR[-rc], id);
		goto err;
	}
	fp_out = fdopen(fd_out, "r+");
	if (!fp_out) {
		rc = -PFIFLASH_ERR_UBI_VOL_FOPEN;
		EBUF(PFIFLASH_ERRSTR[-rc], id);
		goto err;
	}
	rc = ubi_update_start(ulib, fd_out, update_size);
	if (rc < 0) {
		rc = -PFIFLASH_ERR_UBI_VOL_UPDATE;
		EBUF(PFIFLASH_ERRSTR[-rc], id);
		goto err;
	}

	rc = bootenv_write(fp_out, bootenv_res);
	if (rc != 0) {
		rc = -PFIFLASH_ERR_BOOTENV_WRITE;
		EBUF(PFIFLASH_ERRSTR[-rc], devno, id);
		goto err;
	}

 err:
	if (ulib != NULL)
		libubi_close(ulib);
	if (bootenv_new != NULL)
		bootenv_destroy(&bootenv_new);
	if (bootenv_res != NULL)
		bootenv_destroy(&bootenv_res);
	if (fp_out)
		fclose(fp_out);

	return rc;
}


/**
 * write_normal_volume - writes data from PFI file int to regular UBI volume
 * @devno	UBI device number
 * @id		UBI volume id
 * @update_size	size of data stream
 * @fp_in	PFI data file pointer
 * @pfi_crc	CRC data from PFI header
 *
 * Error handling:
 *	when UBI system couldn't be opened
 *	- returns -PFIFLASH_ERR_UBI_OPEN, err_buf matches text to err
 *	when UBI system couldn't open a volume
 *	- returns -PFIFLASH_ERR_UBI_VOL_FOPEN, err_buf matches text to err
 *	when unexpected EOF is encountered
 *	- returns -PFIFLASH_ERR_EOF, err_buf matches text to err
 *	when file I/O error
 *	- returns -PFIFLASH_ERR_FIO, err_buf matches text to err
 *	when CRC check fails
 *	- retruns -PFIFLASH_ERR_CRC_CHECK, err_buf matches text to err
 **/
static int
write_normal_volume(int devno, uint32_t id, size_t update_size, FILE* fp_in,
		    uint32_t pfi_crc,
		    char *err_buf, size_t err_buf_size)
{
	int rc, fd_out;
	uint32_t crc, crc32_table[256];
	char path[PATH_MAX];
	size_t bytes_left;
	FILE* fp_out;
	libubi_t ulib;

	rc = 0;
	crc = UBI_CRC32_INIT;
	bytes_left = update_size;
	fp_out = NULL;
	ulib = NULL;

	log_msg("[ ubiupdatevol id=%d, update_size=%d fp_in=%p",
		id, update_size, fp_in);

	ulib = libubi_open();
	if (ulib == NULL) {
		rc = -PFIFLASH_ERR_UBI_OPEN;
		EBUF(PFIFLASH_ERRSTR[-rc]);
		goto err;
	}

	snprintf(path, PATH_MAX, DEFAULT_VOL_PATTERN, devno, id);

	fd_out = open(path, O_RDWR);
	if (fd_out < 0) {
		rc = -PFIFLASH_ERR_UBI_VOL_FOPEN;
		EBUF(PFIFLASH_ERRSTR[-rc], id);
		goto err;
	}
	fp_out = fdopen(fd_out, "r+");
	if (!fp_out) {
		rc = -PFIFLASH_ERR_UBI_VOL_FOPEN;
		EBUF(PFIFLASH_ERRSTR[-rc], id);
		goto err;
	}
	rc = ubi_update_start(ulib, fd_out, update_size);
	if (rc < 0) {
		rc = -PFIFLASH_ERR_UBI_VOL_UPDATE;
		EBUF(PFIFLASH_ERRSTR[-rc], id);
		goto err;
	}

	init_crc32_table(crc32_table);
	while (bytes_left) {
		char buf[1024];
		size_t to_rw = sizeof buf > bytes_left ?
			bytes_left : sizeof buf;
		if (fread(buf, 1, to_rw, fp_in) != to_rw) {
			rc = -PFIFLASH_ERR_EOF;
			EBUF(PFIFLASH_ERRSTR[-rc]);
			goto err;
		}
		crc = clc_crc32(crc32_table, crc, buf, to_rw);
		if (fwrite(buf, 1, to_rw, fp_out) != to_rw) {
			rc = -PFIFLASH_ERR_FIO;
			EBUF(PFIFLASH_ERRSTR[-rc]);
			goto err;
		}
		bytes_left -= to_rw;
	}

	if (crc != pfi_crc) {
		rc = -PFIFLASH_ERR_CRC_CHECK;
		EBUF(PFIFLASH_ERRSTR[-rc], pfi_crc, crc);
		goto err;
	}

 err:
	if (fp_out)
		fclose(fp_out);
	if (ulib)
		libubi_close(ulib);

	return rc;
}

static int compare_bootenv(FILE *fp_pfi, FILE **fp_flash, uint32_t ids_size,
		uint32_t data_size, pdd_func_t pdd_f, char *err_buf,
		size_t err_buf_size)
{
	int rc, warnings = 0;
	unsigned int i;
	bootenv_t bootenv_pfi, bootenv_res = NULL, bootenv_flash = NULL;

	rc = bootenv_create(&bootenv_pfi);
	if (rc != 0) {
		rc = -PFIFLASH_ERR_BOOTENV_CREATE;
		goto err;
	}

	rc = bootenv_create(&bootenv_res);
	if (rc != 0) {
		rc = -PFIFLASH_ERR_BOOTENV_CREATE;
		goto err;
	}

	rc = bootenv_read(fp_pfi, bootenv_pfi, data_size);
	if (rc != 0) {
		rc = -PFIFLASH_ERR_BOOTENV_READ;
		goto err;
	}

	for (i = 0; i < ids_size; i++) {
		rc = bootenv_create(&bootenv_flash);
		if (rc != 0) {
			rc = -PFIFLASH_ERR_BOOTENV_CREATE;
			goto err;
		}

		rc = bootenv_read(fp_flash[i], bootenv_flash, BOOTENV_MAXSIZE);
		if (rc != 0) {
			rc = -PFIFLASH_ERR_BOOTENV_READ;
			goto err;
		}

		rc = pdd_f(bootenv_flash, bootenv_pfi, &bootenv_res,
				&warnings, err_buf, err_buf_size);
		if (rc != 0) {
			rc = -PFIFLASH_ERR_PDD_UNKNOWN;
			goto err;
		}

		rc = bootenv_compare(bootenv_flash, bootenv_res);
		if (rc > 0) {
			rc = -PFIFLASH_CMP_DIFF;
			goto err;
		} else if (rc < 0) {
			rc = -PFIFLASH_ERR_COMPARE;
			goto err;
		}

		bootenv_destroy(&bootenv_flash);
		bootenv_flash = NULL;
	}

err:
	if (bootenv_pfi)
		bootenv_destroy(&bootenv_pfi);
	if (bootenv_res)
		bootenv_destroy(&bootenv_res);
	if (bootenv_flash)
		bootenv_destroy(&bootenv_flash);

	return rc;
}

static int compare_data(FILE *fp_pfi, FILE **fp_flash, uint32_t ids_size,
		uint32_t bytes_left)
{
	unsigned int i;
	size_t read_bytes, rc = 0;
	char buf_pfi[COMPARE_BUFFER_SIZE];
	char *buf_flash[ids_size];

	for (i = 0; i < ids_size; i++) {
		buf_flash[i] = malloc(COMPARE_BUFFER_SIZE);
		if (!buf_flash[i])
			return -PFIFLASH_ERR_COMPARE;
	}

	while (bytes_left) {
		if (bytes_left > COMPARE_BUFFER_SIZE)
			read_bytes = COMPARE_BUFFER_SIZE;
		else
			read_bytes = bytes_left;

		rc = fread(buf_pfi, 1, read_bytes, fp_pfi);
		if (rc != read_bytes) {
			rc = -PFIFLASH_ERR_COMPARE;
			goto err;
		}

		for (i = 0; i < ids_size; i++) {
			rc = fread(buf_flash[i], 1, read_bytes, fp_flash[i]);
			if (rc != read_bytes) {
				rc = -PFIFLASH_CMP_DIFF;
				goto err;
			}

			rc = memcmp(buf_pfi, buf_flash[i], read_bytes);
			if (rc != 0) {
				rc = -PFIFLASH_CMP_DIFF;
				goto err;
			}
		}

		bytes_left -= read_bytes;
	}

err:
	for (i = 0; i < ids_size; i++)
		free(buf_flash[i]);

	return rc;
}

static int compare_volumes(int devno, pfi_ubi_t u, FILE *fp_pfi,
		pdd_func_t pdd_f, char *err_buf, size_t err_buf_size)
{
	int rc, is_bootenv = 0;
	unsigned int i;
	char path[PATH_MAX];
	libubi_t ulib = NULL;
	FILE *fp_flash[u->ids_size];

	ulib = libubi_open();
	if (ulib == NULL) {
		rc = -PFIFLASH_ERR_UBI_OPEN;
		goto err;
	}

	for (i = 0; i < u->ids_size; i++) {
		if (u->ids[i] == EXAMPLE_BOOTENV_VOL_ID_1 ||
		    u->ids[i] == EXAMPLE_BOOTENV_VOL_ID_2)
			is_bootenv = 1;

		snprintf(path, PATH_MAX, DEFAULT_VOL_PATTERN, devno, u->ids[i]);

		fp_flash[i] = fopen(path, "r");
		if (fp_flash[i] == NULL) {
			rc = -PFIFLASH_ERR_UBI_OPEN;
			goto err;
		}
	}

	if (is_bootenv)
		rc = compare_bootenv(fp_pfi, fp_flash, u->ids_size,
				u->data_size, pdd_f, err_buf, err_buf_size);
	else
		rc = compare_data(fp_pfi, fp_flash, u->ids_size, u->data_size);

err:
	if (rc < 0)
		EBUF(PFIFLASH_ERRSTR[-rc]);

	for (i = 0; i < u->ids_size; i++)
		fclose(fp_flash[i]);
	if (ulib)
		libubi_close(ulib);

	return rc;
}

static int
erase_mtd_region(FILE* file_p, int start, int length)
{
	int rc, fd;
	erase_info_t erase;
	mtd_info_t mtdinfo;
	loff_t offset = start;
	loff_t end = offset + length;

	fd = fileno(file_p);
	if (fd < 0)
		return -PFIFLASH_ERR_MTD_ERASE;

	rc = ioctl(fd, MEMGETINFO, &mtdinfo);
	if (rc)
		return -PFIFLASH_ERR_MTD_ERASE;

	/* check for bad blocks in case of NAND flash */
	if (mtdinfo.type == MTD_NANDFLASH) {
		while (offset < end) {
			rc = ioctl(fd, MEMGETBADBLOCK, &offset);
			if (rc > 0) {
				return -PFIFLASH_ERR_MTD_ERASE;
			}

			offset += mtdinfo.erasesize;
		}
	}

	erase.start = start;
	erase.length = length;

	rc = ioctl(fd, MEMERASE, &erase);
	if (rc) {
		return -PFIFLASH_ERR_MTD_ERASE;
	}

	return rc;
}

/**
 * process_raw_volumes - writes the raw sections of the PFI data
 * @pfi		PFI data file pointer
 * @pfi_raws	list of PFI raw headers
 * @rawdev	device to use to write raw data
 *
 * Error handling:
 *	when early EOF in PFI data
 *	- returns -PFIFLASH_ERR_EOF, err_buf matches text to err
 *	when file I/O error
 *	- returns -PFIFLASH_ERR_FIO, err_buf matches text to err
 *	when CRC check fails
 *	- returns -PFIFLASH_ERR_CRC_CHECK, err_buf matches text to err
 *	when opening MTD device fails
 *	- reutrns -PFIFLASH_ERR_MTD_OPEN, err_buf matches text to err
 *	when closing MTD device fails
 *	- returns -PFIFLASH_ERR_MTD_CLOSE, err_buf matches text to err
 **/
static int
process_raw_volumes(FILE* pfi, list_t pfi_raws, const char* rawdev,
		    char* err_buf, size_t err_buf_size)
{
	int rc;
	char *pfi_data;
	void *i;
	uint32_t crc, crc32_table[256];
	size_t j, k;
	FILE* mtd = NULL;
	list_t ptr;

	if (is_empty(pfi_raws))
		return 0;

	if (rawdev == NULL)
		return 0;

	rc = 0;

	pfi_data = NULL;

	log_msg("[ rawupdate dev=%s", rawdev);

	crc = UBI_CRC32_INIT;
	init_crc32_table(crc32_table);

	/* most likely only one element in list, but just in case */
	foreach(i, ptr, pfi_raws) {
		pfi_raw_t r = (pfi_raw_t)i;

		/* read in pfi data */
		if (pfi_data != NULL)
			free(pfi_data);
		pfi_data = malloc(r->data_size * sizeof(char));
		for (j = 0; j < r->data_size; j++) {
			int c = fgetc(pfi);
			if (c == EOF) {
				rc = -PFIFLASH_ERR_EOF;
				EBUF(PFIFLASH_ERRSTR[-rc]);
				goto err;
			} else if (ferror(pfi)) {
				rc = -PFIFLASH_ERR_FIO;
				EBUF(PFIFLASH_ERRSTR[-rc]);
				goto err;
			}
			pfi_data[j] = (char)c;
		}
		crc = clc_crc32(crc32_table, crc, pfi_data, r->data_size);

		/* check crc */
		if (crc != r->crc) {
			rc = -PFIFLASH_ERR_CRC_CHECK;
			EBUF(PFIFLASH_ERRSTR[-rc], r->crc, crc);
			goto err;
		}

		/* open device */
		mtd = fopen(rawdev, "r+");
		if (mtd == NULL) {
			rc = -PFIFLASH_ERR_MTD_OPEN;
			EBUF(PFIFLASH_ERRSTR[-rc], rawdev);
			goto err;
		}

		for (j = 0; j < r->starts_size; j++) {
			rc = erase_mtd_region(mtd, r->starts[j], r->data_size);
			if (rc) {
				EBUF(PFIFLASH_ERRSTR[-rc]);
				goto err;
			}

			fseek(mtd, r->starts[j], SEEK_SET);
			for (k = 0; k < r->data_size; k++) {
				int c = fputc((int)pfi_data[k], mtd);
				if (c == EOF) {
					fclose(mtd);
					rc = -PFIFLASH_ERR_EOF;
					EBUF(PFIFLASH_ERRSTR[-rc]);
					goto err;
				}
				if ((char)c != pfi_data[k]) {
					fclose(mtd);
					rc = -1;
					goto err;
				}
			}
		}
		rc = fclose(mtd);
		mtd = NULL;
		if (rc != 0) {
			rc = -PFIFLASH_ERR_MTD_CLOSE;
			EBUF(PFIFLASH_ERRSTR[-rc], rawdev);
			goto err;
		}
	}

 err:
	if (mtd != NULL)
		fclose(mtd);
	if (pfi_data != NULL)
		free(pfi_data);
	return rc;
}


/**
 * erase_unmapped_ubi_volumes - skip volumes provided by PFI file, clear rest
 * @devno	UBI device number
 * @pfi_ubis	list of UBI header data
 *
 * Error handling:
 *	when UBI id is out of bounds
 *	- returns -PFIFLASH_ERR_UBI_VID_OOB, err_buf matches text to err
 *	when UBI volume can't be removed
 *	- passes rc, prepends err_buf with contextual aid
 **/
static int
erase_unmapped_ubi_volumes(int devno, list_t pfi_ubis,
			   char *err_buf, size_t err_buf_size)
{
	int rc;
	uint8_t ubi_volumes[PFI_UBI_MAX_VOLUMES];
	size_t i;
	list_t ptr;
	pfi_ubi_t u;

	rc = 0;

	for (i = 0; i < PFI_UBI_MAX_VOLUMES; i++)
		ubi_volumes[i] = 1;

	foreach(u, ptr, pfi_ubis) {
		/* iterate over each vol_id */
		for(i = 0; i < u->ids_size; i++) {
			if (u->ids[i] >= PFI_UBI_MAX_VOLUMES) {
				rc = -PFIFLASH_ERR_UBI_VID_OOB;
				EBUF(PFIFLASH_ERRSTR[-rc], u->ids[i]);
				goto err;
			}
			/* remove from removal list */
			ubi_volumes[u->ids[i]] = 0;
		}
	}

	for (i = 0; i < PFI_UBI_MAX_VOLUMES; i++) {
		if (ubi_volumes[i]) {
			rc = my_ubi_rmvol(devno, i, err_buf, err_buf_size);
			if (rc != 0) {
				EBUF_PREPEND("remove volume failed");
				goto err;
			}
		}
	}

 err:
	return rc;
}


/**
 * process_ubi_volumes - delegate tasks regarding UBI volumes
 * @pfi			PFI data file pointer
 * @seqnum		sequence number
 * @pfi_ubis		list of UBI header data
 * @bootenv_old		storage for current system PDD
 * @pdd_f		function to handle PDD
 * @ubi_update_process	whether reading or writing
 *
 * Error handling:
 *	when and unknown ubi_update_process is given
 *	- returns -PFIFLASH_ERR_UBI_UNKNOWN, err_buf matches text to err
 *	otherwise
 *	- passes rc and err_buf
 **/
static int
process_ubi_volumes(FILE* pfi, int seqnum, list_t pfi_ubis,
		    bootenv_t bootenv_old, pdd_func_t pdd_f,
		    ubi_update_process_t ubi_update_process,
		    char *err_buf, size_t err_buf_size)
{
	int rc;
	pfi_ubi_t u;
	list_t ptr;

	rc = 0;

	foreach(u, ptr, pfi_ubis) {
		int s = seqnum;

		if (s > ((int)u->ids_size - 1))
			s = 0; /* per default use the first */
		u->curr_seqnum = s;

		switch (ubi_update_process) {
		case UBI_REMOVE:
			/* TODO are all these "EXAMPLE" vars okay? */
			if ((u->ids[s] == EXAMPLE_BOOTENV_VOL_ID_1) ||
			    (u->ids[s] == EXAMPLE_BOOTENV_VOL_ID_2)) {
				rc = read_bootenv_volume(EXAMPLE_UBI_DEVICE,
							 u->ids[s], bootenv_old,
							 err_buf, err_buf_size);
				/* it's okay if there is no bootenv
				 * we're going to write one */
				if ((rc == -PFIFLASH_ERR_UBI_VOL_FOPEN) ||
				    (rc == -PFIFLASH_ERR_BOOTENV_READ))
					rc = 0;
				if (rc != 0)
					goto err;
			}

			rc = my_ubi_rmvol(EXAMPLE_UBI_DEVICE, u->ids[s],
					  err_buf, err_buf_size);
			if (rc != 0)
				goto err;

			break;
		case UBI_WRITE:
			rc = my_ubi_mkvol(EXAMPLE_UBI_DEVICE, s, u,
					  err_buf, err_buf_size);
			if (rc != 0) {
				EBUF_PREPEND("creating volume");
				goto err;
			}

			if ((u->ids[s] == EXAMPLE_BOOTENV_VOL_ID_1) ||
			    (u->ids[s] == EXAMPLE_BOOTENV_VOL_ID_2)) {
				rc = write_bootenv_volume(EXAMPLE_UBI_DEVICE,
							  u->ids[s],
							  bootenv_old, pdd_f,
							  pfi,
							  u->data_size,
							  u->crc,
							  err_buf,
							  err_buf_size);
				if (rc != 0)
					EBUF_PREPEND("bootenv volume");
			} else {
				rc = write_normal_volume(EXAMPLE_UBI_DEVICE,
							 u->ids[s],
							 u->data_size, pfi,
							 u->crc,
							 err_buf,
							 err_buf_size);
				if (rc != 0)
					EBUF_PREPEND("normal volume");
			}
			if (rc != 0)
				goto err;

			break;
		case UBI_COMPARE:
			rc = compare_volumes(EXAMPLE_UBI_DEVICE, u, pfi, pdd_f,
					err_buf, err_buf_size);
			if (rc != 0) {
				EBUF_PREPEND("compare volume");
				goto err;
			}

			break;
		default:
			rc = -PFIFLASH_ERR_UBI_UNKNOWN;
			EBUF(PFIFLASH_ERRSTR[-rc]);
			goto err;
		}
	}

 err:
	return rc;
}


/**
 * mirror_ubi_volumes - mirror redundant pairs of volumes
 * @devno	UBI device number
 * @pfi_ubis	list of PFI header data
 *
 * Error handling:
 *	when UBI system couldn't be opened
 *	- returns -PFIFLASH_ERR_UBI_OPEN, err_buf matches text to err
 **/
static int
mirror_ubi_volumes(uint32_t devno, list_t pfi_ubis,
		   char *err_buf, size_t err_buf_size)
{
	int rc;
	uint32_t j;
	list_t ptr;
	pfi_ubi_t i;
	libubi_t ulib;

	rc = 0;
	ulib = NULL;

	log_msg("[ mirror ...");

	ulib = libubi_open();
	if (ulib == NULL) {
		rc = -PFIFLASH_ERR_UBI_OPEN;
		EBUF(PFIFLASH_ERRSTR[-rc]);
		goto err;
	}

	/**
	 * Execute all mirror operations on redundant groups.
	 * Create a volume within a redundant group if it does
	 * not exist already (this is a precondition of
	 * ubimirror).
	 */
	foreach(i, ptr, pfi_ubis) {
		for (j = 0; j < i->ids_size; j++) {
			/* skip self-match */
			if (i->ids[j] == i->ids[i->curr_seqnum])
				continue;

			rc = my_ubi_rmvol(devno, i->ids[j],
					  err_buf, err_buf_size);
			if (rc != 0)
				goto err;

			rc = my_ubi_mkvol(devno, j, i,
					  err_buf, err_buf_size);
			if (rc != 0)
				goto err;
		}
	}

	foreach(i, ptr, pfi_ubis) {
		rc = ubimirror(devno, i->curr_seqnum, i->ids, i->ids_size,
			       err_buf, err_buf_size);
		if (rc != 0)
			goto err;
	}


 err:
	if (ulib != NULL)
		libubi_close(ulib);

	return rc;
}


/**
 * pfiflash_with_options - exposed func to flash memory with a PFI file
 * @pfi			PFI data file pointer
 * @complete		flag to erase unmapped volumes
 * @seqnum		sequence number
 * @compare		flag to compare
 * @pdd_handling	method to handle pdd (keep, merge, overwrite...)
 *
 * Error handling:
 *	when bootenv can't be created
 *	- returns -PFIFLASH_ERR_BOOTENV_CREATE, err_buf matches text to err
 *	when PFI headers can't be read, or
 *	when fail to skip raw sections, or
 *	when error occurs while processing raw volumes, or
 *	when fail to erase unmapped UBI vols, or
 *	when error occurs while processing UBI volumes, or
 *	when error occurs while mirroring UBI volumes
 *	- passes rc, prepends err_buf with contextual aid
 **/
int
pfiflash_with_options(FILE* pfi, int complete, int seqnum, int compare,
		  pdd_handling_t pdd_handling, const char* rawdev,
		  char *err_buf, size_t err_buf_size)
{
	int rc;
	bootenv_t bootenv;
	pdd_func_t pdd_f;

	if (pfi == NULL)
		return -EINVAL;

	rc = 0;
	pdd_f = NULL;

	/* If the user didnt specify a seqnum we start per default
	 * with the index 0 */
	int curr_seqnum = seqnum < 0 ? 0 : seqnum;

	list_t pfi_raws   = mk_empty(); /* list of raw sections from a pfi */
	list_t pfi_ubis   = mk_empty(); /* list of ubi sections from a pfi */

	rc = bootenv_create(&bootenv);
	if (rc != 0) {
		rc = -PFIFLASH_ERR_BOOTENV_CREATE;
		EBUF(PFIFLASH_ERRSTR[-rc], "");
		goto err;
	}

	rc = read_pfi_headers(&pfi_raws, &pfi_ubis, pfi, err_buf, err_buf_size);
	if (rc != 0) {
		EBUF_PREPEND("reading PFI header");
		goto err;
	}

	if (rawdev == NULL || compare)
		rc = skip_raw_volumes(pfi, pfi_raws, err_buf, err_buf_size);
	else
		rc = process_raw_volumes(pfi, pfi_raws, rawdev, err_buf,
					 err_buf_size);
	if (rc != 0) {
		EBUF_PREPEND("handling raw section");
		goto err;
	}

	if (complete && !compare) {
		rc = erase_unmapped_ubi_volumes(EXAMPLE_UBI_DEVICE, pfi_ubis,
						err_buf, err_buf_size);
		if (rc != 0) {
			EBUF_PREPEND("deleting unmapped UBI volumes");
			goto err;
		}
	}

	if (((int)pdd_handling >= 0) &&
	    (pdd_handling < PDD_HANDLING_NUM))
		pdd_f = pdd_funcs[pdd_handling];
	else {
		rc = -PFIFLASH_ERR_PDD_UNKNOWN;
		EBUF(PFIFLASH_ERRSTR[-rc]);
		goto err;
	}

	if (!compare) {
		rc = process_ubi_volumes(pfi, curr_seqnum, pfi_ubis, bootenv,
				pdd_f, UBI_REMOVE, err_buf, err_buf_size);
		if (rc != 0) {
			EBUF_PREPEND("removing UBI volumes");
			goto err;
		}

		rc = process_ubi_volumes(pfi, curr_seqnum, pfi_ubis, bootenv,
				pdd_f, UBI_WRITE, err_buf, err_buf_size);
		if  (rc != 0) {
			EBUF_PREPEND("writing UBI volumes");
			goto err;
		}

		if (seqnum < 0) { /* mirror redundant pairs */
			rc = mirror_ubi_volumes(EXAMPLE_UBI_DEVICE, pfi_ubis,
					err_buf, err_buf_size);
			if (rc != 0) {
				EBUF_PREPEND("mirroring UBI volumes");
				goto err;
			}
		}
	} else {
		/* only compare volumes, don't alter the content */
		rc = process_ubi_volumes(pfi, curr_seqnum, pfi_ubis, bootenv,
				pdd_f, UBI_COMPARE, err_buf, err_buf_size);

		if (rc == -PFIFLASH_CMP_DIFF)
			/* update is necessary, return positive value */
			rc = 1;

		if (rc < 0) {
			EBUF_PREPEND("comparing UBI volumes");
			goto err;
		}
	}

 err:
	pfi_raws = remove_all((free_func_t)&free_pfi_raw, pfi_raws);
	pfi_ubis = remove_all((free_func_t)&free_pfi_ubi, pfi_ubis);
	bootenv_destroy(&bootenv);
	return rc;
}


/**
 * pfiflash - passes to pfiflash_with_options
 * @pfi			PFI data file pointer
 * @complete		flag to erase unmapped volumes
 * @seqnum		sequence number
 * @pdd_handling	method to handle pdd (keep, merge, overwrite...)
 **/
int
pfiflash(FILE* pfi, int complete, int seqnum, pdd_handling_t pdd_handling,
		char *err_buf, size_t err_buf_size)
{
	return pfiflash_with_options(pfi, complete, seqnum, 0, pdd_handling,
				 NULL, err_buf, err_buf_size);
}
