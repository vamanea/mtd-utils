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
 *
 * Read in PFI (partial flash image) data and store it into internal
 * data structures for further processing. Take also care about
 * special handling if the data contains PDD (platform description
 * data/boot-parameters).
 */

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "bootenv.h"
#include "reader.h"

#define __unused __attribute__((unused))

/* @FIXME hard coded offsets right now - get them from Artem? */
#define NAND2048_DEFAULT_VID_HDR_OFF 1984
#define NAND512_DEFAULT_VID_HDR_OFF  448
#define NOR_DEFAULT_VID_HDR_OFF      64

#define EBUF_PFI(fmt...)						\
	do { int i = snprintf(err_buf, err_buf_size, "%s\n", label);	\
	     snprintf(err_buf + i, err_buf_size - i, fmt);		\
	} while (0)

#define EBUF(fmt...) \
	do { snprintf(err_buf, err_buf_size, fmt); } while (0)


int
read_pdd_data(FILE* fp_pdd, pdd_data_t* pdd_data,
	      char* err_buf, size_t err_buf_size)
{
	int rc = 0;
	bootenv_t pdd = NULL;
	pdd_data_t res = NULL;
	const char* value;

	res = (pdd_data_t) malloc(sizeof(struct pdd_data));
	if (!res) {
		rc = -ENOMEM;
		goto err;
	}
	rc = bootenv_create(&pdd);
	if (rc != 0) {
		goto err;
	}
	rc = bootenv_read_txt(fp_pdd, pdd);
	if (rc != 0) {
		goto err;
	}
	rc = bootenv_get(pdd, "flash_type", &value);
	if (rc != 0) {
		goto err;
	}

	if (strcmp(value, "NAND") == 0) {

		rc = bootenv_get_num(pdd, "flash_page_size",
			     &(res->flash_page_size));
		if (rc != 0) {
			EBUF("Cannot read 'flash_page_size' from pdd.");
			goto err;
		}
		res->flash_type = NAND_FLASH;

		switch (res->flash_page_size) {
		case 512:
			res->vid_hdr_offset = NAND512_DEFAULT_VID_HDR_OFF;
			break;
		case 2048:
			res->vid_hdr_offset = NAND2048_DEFAULT_VID_HDR_OFF;
			break;
		default:
			EBUF("Unsupported  'flash_page_size' %d.",
			     res->flash_page_size);
			goto err;
		}
	}
	else if (strcmp(value, "NOR") == 0){
		res->flash_type = NOR_FLASH;
		res->vid_hdr_offset = NOR_DEFAULT_VID_HDR_OFF;
	}
	else {
		snprintf(err_buf, err_buf_size,
			 "Unkown flash type: %s", value);
		goto err;
	}

	rc = bootenv_get_num(pdd, "flash_eraseblock_size",
			     &(res->eb_size));
	if (rc != 0) {
		EBUF("Cannot read 'flash_eraseblock_size' from pdd.");
		goto err;
	}

	rc = bootenv_get_num(pdd, "flash_size",
			     &(res->flash_size));
	if (rc != 0) {
		EBUF("Cannot read 'flash_size' from pdd.");
		goto err;
	}

	goto out;
 err:
	if (res) {
		free(res);
		res = NULL;
	}
 out:
	bootenv_destroy(&pdd);
	*pdd_data = res;
	return rc;
}

int
read_pfi_raw(pfi_header pfi_hd, FILE* fp_pfi __unused, pfi_raw_t* pfi_raw,
	     const char* label, char* err_buf, size_t err_buf_size)
{
	int rc = 0;
	char tmp_str[PFI_KEYWORD_LEN];
	bootenv_list_t raw_start_list = NULL;
	pfi_raw_t res;
	size_t size;

	res = (pfi_raw_t) malloc(sizeof(struct pfi_raw));
	if (!res)
		return -ENOMEM;

	rc = pfi_header_getnumber(pfi_hd, "size", &(res->data_size));
	if (rc != 0) {
		EBUF_PFI("Cannot read 'size' from PFI.");
		goto err;
	}

	rc = pfi_header_getnumber(pfi_hd, "crc", &(res->crc));
	if (rc != 0) {
		EBUF_PFI("Cannot read 'crc' from PFI.");
		goto err;
	}

	rc = pfi_header_getstring(pfi_hd, "raw_starts",
				  tmp_str, PFI_KEYWORD_LEN);
	if (rc != 0) {
		EBUF_PFI("Cannot read 'raw_starts' from PFI.");
		goto err;
	}

	rc = bootenv_list_create(&raw_start_list);
	if (rc != 0) {
		goto err;
	}

	rc = bootenv_list_import(raw_start_list, tmp_str);
	if (rc != 0) {
		EBUF_PFI("Cannot translate PFI value: %s", tmp_str);
		goto err;
	}

	rc = bootenv_list_to_num_vector(raw_start_list,
					&size, &(res->starts));
	res->starts_size = size;

	if (rc != 0) {
		EBUF_PFI("Cannot create numeric value array: %s", tmp_str);
		goto err;
	}

	goto out;

 err:
	if (res) {
		free(res);
		res = NULL;
	}
 out:
	bootenv_list_destroy(&raw_start_list);
	*pfi_raw = res;
	return rc;
}

int
read_pfi_ubi(pfi_header pfi_hd, FILE* fp_pfi __unused, pfi_ubi_t* pfi_ubi,
	     const char *label, char* err_buf, size_t err_buf_size)
{
	int rc = 0;
	const char** tmp_names = NULL;
	char tmp_str[PFI_KEYWORD_LEN];
	bootenv_list_t ubi_id_list = NULL;
	bootenv_list_t ubi_name_list = NULL;
	pfi_ubi_t res;
	uint32_t i;
	size_t size;

	res = (pfi_ubi_t) calloc(1, sizeof(struct pfi_ubi));
	if (!res)
		return -ENOMEM;

	rc = pfi_header_getnumber(pfi_hd, "size", &(res->data_size));
	if (rc != 0) {
		EBUF_PFI("Cannot read 'size' from PFI.");
		goto err;
	}

	rc = pfi_header_getnumber(pfi_hd, "crc", &(res->crc));
	if (rc != 0) {
		EBUF_PFI("Cannot read 'crc' from PFI.");
		goto err;
	}

	rc = pfi_header_getstring(pfi_hd, "ubi_ids", tmp_str, PFI_KEYWORD_LEN);
	if (rc != 0) {
		EBUF_PFI("Cannot read 'ubi_ids' from PFI.");
		goto err;
	}

	rc = bootenv_list_create(&ubi_id_list);
	if (rc != 0) {
		goto err;
	}
	rc = bootenv_list_create(&ubi_name_list);
	if (rc != 0) {
		goto err;
	}

	rc = bootenv_list_import(ubi_id_list, tmp_str);
	if (rc != 0) {
		EBUF_PFI("Cannot translate PFI value: %s", tmp_str);
		goto err;
	}

	rc = bootenv_list_to_num_vector(ubi_id_list, &size,
					&(res->ids));
	res->ids_size = size;
	if (rc != 0) {
		EBUF_PFI("Cannot create numeric value array: %s", tmp_str);
		goto err;
	}

	if (res->ids_size == 0) {
		rc = -1;
		EBUF_PFI("Sanity check failed: No ubi_ids specified.");
		goto err;
	}

	rc = pfi_header_getstring(pfi_hd, "ubi_type",
				  tmp_str, PFI_KEYWORD_LEN);
	if (rc != 0) {
		EBUF_PFI("Cannot read 'ubi_type' from PFI.");
		goto err;
	}
	if (strcmp(tmp_str, "static") == 0)
		res->type = pfi_ubi_static;
	else if (strcmp(tmp_str, "dynamic") == 0)
		res->type = pfi_ubi_dynamic;
	else {
		EBUF_PFI("Unknown ubi_type in PFI.");
		goto err;
	}

	rc = pfi_header_getnumber(pfi_hd, "ubi_alignment", &(res->alignment));
	if (rc != 0) {
		EBUF_PFI("Cannot read 'ubi_alignment' from PFI.");
		goto err;
	}

	rc = pfi_header_getnumber(pfi_hd, "ubi_size", &(res->size));
	if (rc != 0) {
		EBUF_PFI("Cannot read 'ubi_size' from PFI.");
		goto err;
	}

	rc = pfi_header_getstring(pfi_hd, "ubi_names",
				  tmp_str, PFI_KEYWORD_LEN);
	if (rc != 0) {
		EBUF_PFI("Cannot read 'ubi_names' from PFI.");
		goto err;
	}

	rc = bootenv_list_import(ubi_name_list, tmp_str);
	if (rc != 0) {
		EBUF_PFI("Cannot translate PFI value: %s", tmp_str);
		goto err;
	}
	rc = bootenv_list_to_vector(ubi_name_list, &size,
				    &(tmp_names));
	res->names_size = size;
	if (rc != 0) {
		EBUF_PFI("Cannot create string array: %s", tmp_str);
		goto err;
	}

	if (res->names_size != res->ids_size) {
		EBUF_PFI("Sanity check failed: ubi_ids list does not match "
			 "sizeof ubi_names list.");
		rc = -1;
	}

	/* copy tmp_names to own structure */
	res->names = (char**) calloc(1, res->names_size * sizeof (char*));
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
	return rc;
}


int
free_pdd_data(pdd_data_t* pdd_data)
{
	if (*pdd_data) {
		free(*pdd_data);
	}
	*pdd_data = NULL;

	return 0;
}

int
free_pfi_raw(pfi_raw_t* pfi_raw)
{
	pfi_raw_t tmp = *pfi_raw;
	if (tmp) {
		if (tmp->starts)
			free(tmp->starts);
		free(tmp);
	}
	*pfi_raw = NULL;

	return 0;
}

int
free_pfi_ubi(pfi_ubi_t* pfi_ubi)
{
	size_t i;
	pfi_ubi_t tmp = *pfi_ubi;
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


int
read_pfi_headers(list_t *pfi_raws, list_t *pfi_ubis, FILE* fp_pfi,
		 char* err_buf, size_t err_buf_size)
{
	int rc = 0;
	char mode[PFI_KEYWORD_LEN];
	char label[PFI_LABEL_LEN];

	*pfi_raws = mk_empty(); pfi_raw_t raw = NULL;
	*pfi_ubis = mk_empty(); pfi_ubi_t ubi = NULL;
	pfi_header pfi_header = NULL;

	/* read all headers from PFI and store them in lists */
	rc = pfi_header_init(&pfi_header);
	if (rc != 0) {
		EBUF("Cannot initialize pfi header.");
		goto err;
	}
	while ((rc == 0) && !feof(fp_pfi)) {
		rc = pfi_header_read(fp_pfi, pfi_header);
		if (rc != 0) {
			if (rc == PFI_DATA_START) {
				rc = 0;
				break; /* data section starts,
					  all headers read */
			}
			else {
				goto err;
			}
		}
		rc = pfi_header_getstring(pfi_header, "label", label,
					  PFI_LABEL_LEN);
		if (rc != 0) {
			EBUF("Cannot read 'label' from PFI.");
			goto err;
		}
		rc = pfi_header_getstring(pfi_header, "mode", mode,
					  PFI_KEYWORD_LEN);
		if (rc != 0) {
			EBUF("Cannot read 'mode' from PFI.");
			goto err;
		}
		if (strcmp(mode, "ubi") == 0) {
			rc = read_pfi_ubi(pfi_header, fp_pfi, &ubi, label,
					  err_buf, err_buf_size);
			if (rc != 0) {
				goto err;
			}
			*pfi_ubis = append_elem(ubi, *pfi_ubis);
		}
		else if (strcmp(mode, "raw") == 0) {
			rc = read_pfi_raw(pfi_header, fp_pfi, &raw, label,
					  err_buf, err_buf_size);
			if (rc != 0) {
				goto err;
			}
			*pfi_raws = append_elem(raw, *pfi_raws);
		}
		else {
			EBUF("Recvieved unknown mode from PFI: %s", mode);
			goto err;
		}
	}
	goto out;

 err:
	*pfi_raws = remove_all((free_func_t)&free_pfi_raw, *pfi_raws);
	*pfi_ubis = remove_all((free_func_t)&free_pfi_ubi, *pfi_ubis);
 out:
	pfi_header_destroy(&pfi_header);
	return rc;

}
