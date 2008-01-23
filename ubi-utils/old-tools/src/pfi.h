#ifndef __pfi_h
#define __pfi_h
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
 * @file pfi.h
 *
 * @author Oliver Lohmann <oliloh@de.ibm.com>
 *         Andreas Arnez <arnez@de.ibm.com>
 *         Joern Engel <engeljoe@de.ibm.com>
 *         Frank Haverkamp <haverkam@de.ibm.com>
 *
 * @brief libpfi will hold all code to create and process pfi
 * images. Definitions made in this file are equaly usable for the
 * development host and the target system.
 *
 * @note This header additionally holds the official definitions for
 * the pfi headers.
 */

#include <stdio.h>		/* FILE */

#ifdef __cplusplus
extern "C" {
#endif

/* Definitions. */

#define PFI_HDRVERSION 1	/* current header version */

#define PFI_ENOVERSION 1	/* unknown version */
#define PFI_ENOHEADER  2	/* not a pfi header */
#define PFI_EINSUFF    3	/* insufficient information */
#define PFI_EUNDEF     4	/* key not defined */
#define PFI_ENOMEM     5	/* out of memory */
#define PFI_EBADTYPE   6	/* bad data type */
#define PFI_EFILE      7	/* file I/O error: see errno */
#define PFI_EFILEINVAL 8	/* file format not valid */
#define PFI_EINVAL     9	/* invalid parameter */
#define PFI_ERANGE     10	/* invalid range */
#define PFI_EMODE      11	/* expecting other mode in this header */
#define PFI_DATA_START 12	/* data section starts */
#define PFI_EMAX       13	/* should be always larger as the largest
				   error code */

#define PFI_LABEL_LEN  64	/* This is the maximum length for a
				   PFI header label */
#define PFI_KEYWORD_LEN 32	/* This is the maximum length for an
				   entry in the mode and type fields */

#define PFI_UBI_MAX_VOLUMES 128
#define PFI_UBI_VOL_NAME_LEN 127

/**
 * @brief The pfi header allows to set flags which influence the flashing
 * behaviour.
 */
#define PFI_FLAG_PROTECTED   0x00000001


/**
 * @brief Handle to pfi header. Used in most of the functions associated
 * with pfi file handling.
 */
typedef struct pfi_header *pfi_header;


/**
 * @brief Initialize a pfi header object.
 *
 * @param head	 Pointer to handle. This function allocates memory
 *		 for this data structure.
 * @return	 0 on success, otherwise:
 *		 PFI_ENOMEM : no memory available for the handle.
 */
int pfi_header_init (pfi_header *head);


/**
 * @brief Destroy a pfi header object.
 *
 * @param head	 handle. head is invalid after calling this function.
 * @return	 0 always.
 */
int pfi_header_destroy (pfi_header *head);


/**
 * @brief Add a key/value pair to a pfi header object.
 *
 * @param head	 handle.
 * @param key	 pointer to key string. Must be 0 terminated.
 * @param value	 pointer to value string. Must be 0 terminated.
 * @return	 0 on success, otherwise:
 *		 PFI_EUNDEF   : key was not found.
 *		 PFI_ENOMEM   : no memory available for the handle.
 *		 PFI_EBADTYPE : value is not an hex string. This happens
 *				 when the key stores an integer and the
 *				 new value is not convertable e.g. not in
 *				 0xXXXXXXXX format.
 */
int pfi_header_setvalue (pfi_header head,
			  const char *key, const char *value);


/**
 * @brief Add a key/value pair to a pfi header object. Provide the
 * value as a number.
 *
 * @param head	 handle.
 * @param key	 pointer to key string. Must be 0 terminated.
 * @param value	 value to set.
 * @return	 0 on success, otherwise:
 *		 PFI_EUNDEF   : key was not found.
 *		 PFI_EBADTYPE : value is not a string. This happens
 *				 when the key stores a string.
 */
int pfi_header_setnumber (pfi_header head,
			   const char *key, uint32_t value);


/**
 * @brief For a given key, return the numerical value stored in a
 * pfi header object.
 *
 * @param head	 handle.
 * @param key	 pointer to key string. Must be 0 terminated.
 * @param value	 pointer to value.
 * @return	 0 on success, otherwise:
 *		 PFI_EUNDEF   : key was not found.
 *		 PFI_EBADTYPE : stored value is not an integer but a string.
 */
int pfi_header_getnumber (pfi_header head,
			   const char *key, uint32_t *value);


static inline uint32_t
pfi_getnumber(pfi_header head, const char *key)
{
	uint32_t value;
	pfi_header_getnumber(head, key, &value);
	return value;
}

/**
 * @brief For a given key, return the string value stored in a pfi
 * header object.
 *
 * @param head	 handle.
 * @param key	 pointer to key string. Must be 0 terminated.
 * @param value	 pointer to value string. Memory must be allocated by the user.
 * @return	 0 on success, otherwise:
 *		 PFI_EUNDEF   : key was not found.
 *		 PFI_EBADTYPE : stored value is not a string but an integer.
 */
int pfi_header_getstring (pfi_header head,
			   const char *key, char *value, size_t size);


/**
 * @brief Write a pfi header object into a given file.
 *
 * @param out	 output stream.
 * @param head	 handle.
 * @return	 0 on success, error values otherwise:
 *		 PFI_EINSUFF   : not all mandatory fields are filled.
 *		 PFI_ENOHEADER : wrong header version or magic number.
 *		 -E*		: see <asm/errno.h>.
 */
int pfi_header_write (FILE *out, pfi_header head);


/**
 * @brief Read a pfi header object from a given file.
 *
 * @param in	 input stream.
 * @param head	 handle.
 * @return	 0 on success, error values otherwise:
 *		 PFI_ENOVERSION: unknown header version.
 *		 PFI_EFILE     : cannot read enough data.
 *		 PFI_ENOHEADER : wrong header version or magic number.
 *		 -E*		: see <asm/errno.h>.
 *
 * If the header verification returned success the user can assume that
 * all mandatory fields for a particular version are accessible. Checking
 * the return code when calling the get-function for those keys is not
 * required in those cases. For optional fields the checking must still be
 * done.
 */
int pfi_header_read (FILE *in, pfi_header head);


/**
 * @brief Display a pfi header in human-readable form.
 *
 * @param out	 output stream.
 * @param head	 handle.
 * @return	 always 0.
 *
 * @note Prints out that it is not implemented and whom you should
 * contact if you need it urgently!.
 */
int pfi_header_dump (FILE *out, pfi_header head);


/*
 * @brief	 Iterates over a stream of pfi files. The iterator function
 *		 must advance the file pointer in FILE *in to the next pfi
 *		 header. Function exists on feof(in).
 *
 * @param in	 input file descriptor, must be open and valid.
 * @param func	 iterator function called when pfi header could be
 *		 read and was validated. The function must return 0 on
 *		 success.
 * @return	 See pfi_header_init and pfi_header_read.
 *		 PFI_EINVAL	  : func is not valid
 *		 0 ok.
 */
typedef int (* pfi_read_func)(FILE *in, pfi_header hdr, void *priv_data);

int pfi_read (FILE *in, pfi_read_func func, void *priv_data);


#ifdef __cplusplus
}
#endif

#endif /* __pfi_h */
