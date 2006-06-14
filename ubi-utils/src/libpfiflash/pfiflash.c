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

/**
 * @file pfiflash.c
 *
 * @author Oliver Lohmann <oliloh@de.ibm.com>
 *
 * @brief This library is provides an interface to the pfiflash utility.
 *
 * <oliloh@de.ibm.com> Wed Mar 15 11:39:19 CET 2006 Initial creation.
 *
 * @TODO Comare data before writing it. This implies that the volume
 * parameters are compared first: size, alignment, name, type, ...,
 * this is the same, compare the data. Volume deletion is deffered
 * until the difference has been found out.
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#define __USE_GNU
#include <string.h>

#include <libubi.h>
#include <pfiflash.h>

#include <mtd/ubi-user.h>	/* FIXME Is this ok here!!?? */

#include "ubimirror.h"
#include "error.h"
#include "reader.h"
#include "example_ubi.h"
#include "bootenv.h"

static const char copyright [] __attribute__((unused)) =
	"Copyright (c) International Business Machines Corp., 2006";

#define EBUF(fmt...) do {			\
		snprintf(err_buf, err_buf_size, fmt);	\
	} while (0)

static pdd_func_t pdd_funcs[PDD_HANDLING_NUM]  =
	{
		&bootenv_pdd_keep,
		&bootenv_pdd_merge,
		&bootenv_pdd_overwrite
	};
/**< An array of PDD function pointers indexed by the algorithm. */


typedef enum ubi_update_process_t {
	UBI_REMOVE = 0,
	UBI_WRITE,
} ubi_update_process_t;

static int
skip_raw_sections(FILE* pfi, list_t pfi_raws,
		  char* err_buf, size_t err_buf_size)
{
	int rc = 0;

	void *i;
	list_t ptr;
	size_t j, skip_size;

	if (is_empty(pfi_raws))
		return 0;

	foreach(i, ptr, pfi_raws) {
		skip_size = ((pfi_raw_t)i)->data_size;
		for(j = 0;  j < skip_size; j++) {
			fgetc(pfi);
			if (ferror(pfi)) {
				EBUF("Cannot skip raw section in PFI.");
				rc = -EIO;
				goto err;
			}
		}
	}
 err:
	return rc;
}

/**
 * @brief Wraps the ubi_mkvol functions and implements a hook for the bootenv
 *	  update.
 * @param devno UBI device number.
 * @param s Current seqnum.
 * @param u Information about the UBI volume from the PFI.
 * @param err_buf An error buffer.
 * @param err_buf_size The size of the error buffer.
 * @return 0 On Sucess.
 * @return else Error.
 */
static int
my_ubi_mkvol(int devno, int s, pfi_ubi_t u, char *err_buf, size_t err_buf_size)
{
	int rc = 0;
	int type;
	ubi_lib_t ulib = NULL;

	log_msg("%s(vol_id=%d, size=%d, data_size=%d, type=%d, "
		"alig=%d, nlen=%d, name=%s)", __func__,
		u->ids[s], u->size, u->data_size, u->type, u->alignment,
		strnlen(u->names[s], PFI_UBI_VOL_NAME_LEN), u->names[s]);

	rc = ubi_open(&ulib);
	if (rc != 0) {
		goto err;
	}

	switch (u->type) {
	case pfi_ubi_static:
		type = UBI_STATIC_VOLUME; break;
	case pfi_ubi_dynamic:
		type = UBI_DYNAMIC_VOLUME; break;
	default:
		type = UBI_DYNAMIC_VOLUME;
	}

	rc = ubi_mkvol(ulib, devno, u->ids[s], type, u->size, u->alignment,
		       u->names[s]);
	if (rc != 0) {
		EBUF("Cannot create volume: %d", u->ids[s]);
		goto err;
	}

 err:
	if (ulib != NULL)
		ubi_close(&ulib);
	return rc;
}

/**
 * @brief A wrapper around the UBI library function ubi_rmvol.
 * @param devno UBI device number.
 * @param s Current seqnum.
 * @param u Information about the UBI volume from the PFI.
 * @param err_buf An error buffer.
 * @param err_buf_size The size of the error buffer.
 *
 * If the volume does not exist, the function will return success.
 */
static int
my_ubi_rmvol(int devno, uint32_t id, char *err_buf, size_t err_buf_size)
{
	int rc = 0;
	ubi_lib_t ulib = NULL;
	int fd;

	log_msg("%s(id=%d)", __func__, id);

	rc = ubi_open(&ulib);
	if (rc != 0)
		goto err;

	/**
	 * Truncate if it exist or not.
	 */
	fd = ubi_vol_open(ulib, devno, id, O_RDWR);
	if (fd == -1)
		return 0;	/* not existent, return */

	rc = ubi_vol_update(fd, 0);
	if (rc < 0) {
		fprintf(stderr, "update failed rc=%d errno=%d\n", rc, errno);
		ubi_vol_close(fd);
		goto err;	/* if EBUSY than empty device, continue */
	}
	ubi_vol_close(fd);

	rc = ubi_rmvol(ulib, devno, id);
	if (rc != 0) {
		/* @TODO Define a ubi_rmvol return value which says
		 * sth like EUBI_NOSUCHDEV. In this case, a failed
		 * operation is acceptable. Everything else has to be
		 * classified as real error. But talk to Andreas Arnez
		 * before defining something odd...
		 */
		/* if ((errno == EINVAL) || (errno == ENODEV))
		   return 0; */ /* currently it is EINVAL or ENODEV */

		dbg_msg("Remove UBI volume %d returned with error: %d "
			"errno=%d", id, rc, errno);
		goto err;
	}
 err:
	if (ulib != NULL)
		ubi_close(&ulib);
	return rc;
}

static int
read_bootenv_volume(int devno, uint32_t id, bootenv_t bootenv_old,
		    char *err_buf, size_t err_buf_size)
{
	int rc = 0;
	ubi_lib_t ulib = NULL;
	FILE* fp_in = NULL;

	rc = ubi_open(&ulib);
	if (rc)
		return rc;

	fp_in = ubi_vol_fopen_read(ulib, devno, id);
	if (!fp_in) {
		EBUF("Cannot open bootenv volume");
		rc = -EIO;
		goto err;
	}

	log_msg("%s reading old bootenvs", __func__);

	/* Save old bootenvs for reference */
	rc = bootenv_read(fp_in, bootenv_old, BOOTENV_MAXSIZE);
	if (rc)
		EBUF("Cannot read bootenv_old");
 err:
	if (fp_in)
		fclose(fp_in);
	if (ulib)
		ubi_close(&ulib);
	return rc;
}

static int
write_bootenv_volume(int devno, uint32_t id, bootenv_t bootenv_old,
		     pdd_func_t pdd_f,
		     FILE* fp_in, /* new pdd data contained in pfi */
		     size_t fp_in_size,	/* data size of new pdd data in pfi */
		     char *err_buf, size_t err_buf_size)
{
	int rc = 0;
	int warnings = 0;
	ubi_lib_t ulib = NULL;
	bootenv_t bootenv_new = NULL;
	bootenv_t bootenv_res = NULL;
	size_t update_size = 0;
	FILE *fp_out = NULL;

	log_msg("%s(id=%d, fp_in=%p)", __func__, id, fp_in);

	/* Workflow:
	 * 1. Apply PDD operation and get the size of the returning
	 *    bootenv_res section. Without the correct size it wouldn't
	 *    be possible to call UBI update vol.
	 * 2. Call UBI update vol
	 * 3. Get FILE* to vol dev
	 * 4. Write to FILE*
	 */

	rc = ubi_open(&ulib);
	if (rc != 0) {
		goto err;
	}

	rc = bootenv_create(&bootenv_new);
	if (rc != 0)
		goto err;
	rc = bootenv_create(&bootenv_res);
	if (rc != 0)
		goto err;

	rc = bootenv_read(fp_in, bootenv_new, fp_in_size);
	if (rc != 0)
		goto err;

	rc = pdd_f(bootenv_old, bootenv_new, &bootenv_res, &warnings,
		   err_buf, err_buf_size);
	if (rc != 0)
		goto err;
	if (warnings) {
		/* @TODO Do sth with the warning */
		dbg_msg("A warning in the PDD operation occured: %d",
			warnings);
	}
	log_msg("... (2)");

	rc = bootenv_size(bootenv_res, &update_size);
	if (rc != 0)
		goto err;

	fp_out = ubi_vol_fopen_update(ulib, devno, id, update_size);
	if (fp_out == NULL)
		goto err;

	rc = bootenv_write(fp_out, bootenv_res);
	if (rc != 0) {
		EBUF("Write operation on ubi%d_%d failed.", devno, id);
		rc = -EIO;
		goto err;
	}

 err:
	if (ulib != NULL)
		ubi_close(&ulib);
	if (bootenv_new != NULL)
		bootenv_destroy(&bootenv_new);
	if (bootenv_res != NULL)
		bootenv_destroy(&bootenv_res);
	if (fp_out)
		fclose(fp_out);
	return rc;
}

static int
write_normal_volume(int devno, uint32_t id, size_t update_size, FILE* fp_in,
		    char *err_buf, size_t err_buf_size)
{
	int rc = 0;
	ubi_lib_t ulib = NULL;
	FILE* fp_out = NULL;
	int c;
	size_t i;

	log_msg("%s(id=%d, update_size=%d fp_in=%p)",
		__func__, id, update_size, fp_in);

	rc = ubi_open(&ulib);
	if (rc)
		return rc;

	fp_out = ubi_vol_fopen_update(ulib, devno, id, update_size);
	if (fp_out == NULL) {
		rc = -1;
		goto err;
	}

	log_msg("starting the update ... "); /* FIXME DBG */
	for (i = 0; i < update_size; i++) {
		c = getc(fp_in);
		if (c == EOF && ferror(fp_in)) {
			rc = -EIO;
			goto err;
		}
		if (putc(c, fp_out) == EOF) {
			rc = -EIO;
			goto err;
		}
		/*  FIXME DBG */
		/* if ((i & 0xFFF) == 0xFFF) log_msg("."); */
	}
	/* log_msg("\n"); */		/*  FIXME DBG */
 err:
	if (fp_out)
		fclose(fp_out);
	if (ulib)
		ubi_close(&ulib);
	return rc;
}


/**
 * @brief ...
 * @precondition	The PFI file contains at least one ubi_id entry.
 *			This is assured by the PFI read process.
 * @postcondition	The used seqnum number is set in the UBI PFI
 *			header list.
 *			The UBI volumes specified by seqnum are processed.
 */
static int
process_ubi_volumes(FILE* pfi, int seqnum, list_t pfi_ubis,
		    bootenv_t bootenv_old, pdd_func_t pdd_f,
		    ubi_update_process_t ubi_update_process,
		    char *err_buf, size_t err_buf_size)
{
	int rc = 0;
	pfi_ubi_t u;
	list_t ptr;

	foreach(u, ptr, pfi_ubis) {
		int s = seqnum;
		if (seqnum > (u->ids_size - 1)) {
			s = 0; /* per default use the first */
		}
		u->curr_seqnum = s;

		switch (ubi_update_process) {
		case UBI_REMOVE:
			if ((u->ids[s] == EXAMPLE_BOOTENV_VOL_ID_1) ||
			    (u->ids[s] == EXAMPLE_BOOTENV_VOL_ID_2)) {
				rc =read_bootenv_volume(EXAMPLE_UBI_DEVICE,
							u->ids[s],
							bootenv_old, err_buf,
							err_buf_size);
				if (rc != 0)
					goto err;
				}
			rc = my_ubi_rmvol(EXAMPLE_UBI_DEVICE,  u->ids[s],
					  err_buf, err_buf_size);
			if (rc != 0)
				goto err;
			break;
		case UBI_WRITE:
			rc = my_ubi_mkvol(EXAMPLE_UBI_DEVICE, s, u,
					  err_buf, err_buf_size);
			if (rc != 0)
				goto err;
			if ((u->ids[s] == EXAMPLE_BOOTENV_VOL_ID_1) ||
			    (u->ids[s] == EXAMPLE_BOOTENV_VOL_ID_2)) {
				rc = write_bootenv_volume(EXAMPLE_UBI_DEVICE,
							  u->ids[s],
							  bootenv_old, pdd_f,
							  pfi,
							  u->data_size,
							  err_buf,
							  err_buf_size);
			}
			else {
				rc = write_normal_volume(EXAMPLE_UBI_DEVICE,
							 u->ids[s],
							 u->data_size, pfi,
							 err_buf,
							 err_buf_size);
			}
			if (rc != 0)
				goto err;
			break;
		default:
			EBUF("Invoked unknown UBI operation.");
			rc = -1;
			goto err;

		}
		if (rc != 0) {
			goto err;
		}
	}
 err:
	return rc;

}

static int
erase_unmapped_ubi_volumes(int devno, list_t pfi_ubis,
			   char *err_buf, size_t err_buf_size)
{
	int rc = 0;
	list_t ptr;
	pfi_ubi_t u;
	size_t i;
	uint8_t ubi_volumes[PFI_UBI_MAX_VOLUMES];

	for (i = 0; i < PFI_UBI_MAX_VOLUMES; i++) {
		ubi_volumes[i] = 1;
	}

	foreach(u, ptr, pfi_ubis) {
		/* iterate over each vol_id */
		for(i = 0; i < u->ids_size; i++) {
			if (u->ids[i] > PFI_UBI_MAX_VOLUMES) {
				EBUF("PFI file contains an invalid "
				     "volume id: %d", u->ids[i]);
				goto err;
			}
			/* remove from removal list */
			ubi_volumes[u->ids[i]] = 0;
		}
	}

	for (i = 0; i < PFI_UBI_MAX_VOLUMES; i++) {
		if (ubi_volumes[i]) {
			rc = my_ubi_rmvol(devno, i, err_buf, err_buf_size);
			if (rc != 0)
				goto err;
		}
	}
 err:
	return rc;
}

static int
mirror_ubi_volumes(uint32_t devno, list_t pfi_ubis,
		   char *err_buf, size_t err_buf_size)
{
	int rc = 0;
	list_t ptr;
	uint32_t j;
	pfi_ubi_t i;
	ubi_lib_t  ulib = NULL;

	log_msg("%s(...)", __func__);

	rc = ubi_open(&ulib);
	if (rc != 0)
		goto err;

	/**
	 * Execute all mirror operations on redundant groups.
	 * Create a volume within a redundant group if it does
	 * not exist already (this is a precondition of
	 * ubimirror).
	 */
	foreach(i, ptr, pfi_ubis) {
		for(j = 0; j < i->ids_size; j++) {
			/* skip self-match */
			if (i->ids[j] == i->ids[i->curr_seqnum])
				continue;

			rc = my_ubi_rmvol(devno, i->ids[j], err_buf,
					  err_buf_size);
			if (rc != 0)
				goto err;

			rc = my_ubi_mkvol(devno, j, i, err_buf, err_buf_size);
			if (rc != 0)
				goto err;
		}
	}

	foreach(i, ptr, pfi_ubis) {
		rc = ubimirror(devno, i->curr_seqnum, i->ids,
			       i->ids_size, err_buf, err_buf_size);
		if (rc != 0)
			goto err;
	}


 err:
	if (ulib != NULL)
		ubi_close(&ulib);
	return rc;
}

int
pfiflash(FILE* pfi, int complete, int seqnum, pdd_handling_t pdd_handling,
	 char *err_buf, size_t err_buf_size)
{
	int rc = 0;
	pdd_func_t pdd_f = NULL;

	if (pfi == NULL)
		return -EINVAL;

	/**
	 * If the user didnt specify a seqnum we start per default
	 * with the index 0
	 */
	int curr_seqnum = seqnum < 0 ? 0 : seqnum;

	list_t pfi_raws   = mk_empty(); /* list of raw sections from a pfi */
	list_t pfi_ubis   = mk_empty(); /* list of ubi sections from a pfi */

	bootenv_t bootenv;
	rc = bootenv_create(&bootenv);
	if (rc != 0) {
		EBUF("Cannot create bootenv variable");
	}

	rc = read_pfi_headers(&pfi_raws, &pfi_ubis, pfi,
			      err_buf, err_buf_size);
	if (rc != 0) {
		EBUF("Cannot read PFI headers.");
		goto err;
	}

	/* @TODO: If you want to implement an IPL update - start here. */
	rc = skip_raw_sections(pfi, pfi_raws, err_buf, err_buf_size);
	if (rc != 0) {
		goto err;
	}

	if (complete) {
		rc = erase_unmapped_ubi_volumes(EXAMPLE_UBI_DEVICE, pfi_ubis,
						err_buf, err_buf_size);
		if (rc != 0) {
			EBUF("Cannot delete unmapped UBI volumes.");
			goto err;
		}
	}

	if ((pdd_handling >= 0) && (pdd_handling < PDD_HANDLING_NUM)) {
		pdd_f = pdd_funcs[pdd_handling];
	}
	else {
		EBUF("Used unknown PDD handling algorithm (pdd_handling)");
	}

	rc = process_ubi_volumes(pfi, curr_seqnum, pfi_ubis, bootenv, pdd_f,
				 UBI_REMOVE, err_buf, err_buf_size);
	if  (rc != 0) {
		goto err;
	}
	rc = process_ubi_volumes(pfi, curr_seqnum, pfi_ubis, bootenv, pdd_f,
				 UBI_WRITE, err_buf, err_buf_size);
	if  (rc != 0) {
		goto err;
	}
	if (seqnum < 0) { /* mirror redundant pairs */
		rc = mirror_ubi_volumes(EXAMPLE_UBI_DEVICE, pfi_ubis,
					err_buf, err_buf_size);
		if (rc != 0)
			goto err;
	}

 err:
	pfi_raws = remove_all((free_func_t)&free_pfi_raw, pfi_raws);
	pfi_ubis = remove_all((free_func_t)&free_pfi_ubi, pfi_ubis);
	bootenv_destroy(&bootenv);
	return rc;
}
