#ifndef __BOOTENV_H__
#define __BOOTENV_H__
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

#include <stdio.h> /* FILE */
#include <stdint.h>
#include <pfiflash.h>

/* DOXYGEN DOCUMENTATION */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bootenv.h
 * @author oliloh@de.ibm.com
 * @version 1.3
 *
 * 1.3 Some renaming
 */

/**
 * @mainpage Usage
 *
 * @section intro Introduction
 * This library provides all functionality to handle with the so-called
 * platform description data (PDD) and the bootparameters defined in
 * U-Boot. It is able to apply the defined PDD operations in PDD update
 * scenarios. For more information about the PDD and bootparameter
 * environment "bootenv" confer the PDD documentation.
 *
 * @section ret Return codes
 * This library defines some return codes which will be delivered classified
 * as warnings or errors. See the "Defines" section for details and numeric
 * values.
 *
 * @section benv Bootenv format description
 * There are two different input formats:
 *	- text files
 *	- binary files
 *
 * @subsection txt Text Files
 * Text files have to be specified like:
 * @verbatim key1=value1,value2,value7\n key2=value55,value1\n key4=value1\n@endverbatim
 *
 * @subsection bin Binary files
 * Binary files have to be specified like:
 * @verbatim<CRC32-bit>key1=value1,value2,value7\0key2=value55,value1\0... @endverbatim
 * You can confer the U-Boot documentation for more details.
 *
 * @section benvlists Bootenv lists format description.
 * Values referenced in the preceeding subsection can be
 * defined like lists:
 * @verbatim value1,value2,value3 @endverbatim
 * There are some situation where a conversion of a comma
 * seperated list can be useful, e.g. to get a list
 * of defined PDD entries.
 */

#define BOOTENV_MAXSIZE (1024 * 100) /* max 100kiB space for bootenv */

/**
 * @def BOOTENV_ECRC
 *	@brief Given binary file is to large.
 * @def BOOTENV_EFMT
 *	@brief Given bootenv section has an invalid format
 * @def BOOTENV_EBADENTRY
 *	@brief Bad entry in the bootenv section.
 * @def BOOTENV_EINVAL
 *	@brief Invalid bootenv defintion.
 * @def BOOTENV_ENOPDD
 *	@brief Given bootenv sectoin has no PDD defintion string (pdd=...).
 * @def BOOTENV_EPDDINVAL
 *	@brief Given bootenv section has an invalid PDD defintion.
 * @def BOOTENV_ENOTIMPL
 *	@brief Functionality not implemented.
 * @def BOOTENV_ECOPY
 *	@brief Bootenv memory copy error
 * @def BOOTENV_ENOTFOUND
 *	@brief Given key has has no value.
 * @def BOOTENV_EMAX
 *	@brief Highest error value.
 */
#define BOOTENV_ETOOBIG		1
#define BOOTENV_EFMT		2
#define BOOTENV_EBADENTRY	3
#define BOOTENV_EINVAL		4
#define BOOTENV_ENOPDD		5
#define BOOTENV_EPDDINVAL	6
#define BOOTENV_ENOTIMPL	7
#define BOOTENV_ECOPY		8
#define BOOTENV_ENOTFOUND	9
#define BOOTENV_EMAX		10

/**
 * @def BOOTENV_W
 *	@brief A warning which is handled internally as an error
 *	 but can be recovered by manual effort.
 * @def BOOTENV_WPDD_STRING_DIFFERS
 *	@brief The PDD strings of old and new PDD differ and
 *	can cause update problems, because new PDD values
 *	are removed from the bootenv section completely.
 */
#define BOOTENV_W		     20
#define BOOTENV_WPDD_STRING_DIFFERS  21
#define BOOTENV_WMAX 22 /* highest warning value */


typedef struct bootenv *bootenv_t;
	/**< A bootenv library handle. */

typedef struct bootenv_list *bootenv_list_t;
	/**< A handle for a value list. */

typedef int(*pdd_func_t)(bootenv_t, bootenv_t, bootenv_t*,
		int*, char*, size_t);


/**
 * @brief Get a new handle.
 * @return 0
 * @return or error
 * */
int bootenv_create(bootenv_t *env);

/**
 * @brief	Cleanup structure.
 * @param env	Bootenv structure which shall be destroyed.
 * @return 0
 * @return or error
 */
int bootenv_destroy(bootenv_t *env);

/**
 * @brief Copy a bootenv handle.
 * @param in	The input bootenv.
 * @param out	The copied output bootenv. Discards old data.
 * @return 0
 * @return or error
 */
int bootenv_copy_bootenv(bootenv_t in, bootenv_t *out);

/**
 * @brief Looks for a value inside the bootenv data.
 * @param env Handle to a bootenv structure.
 * @param key The key.
 * @return NULL	 key not found
 * @return !NULL ptr to value
 */
int bootenv_get(bootenv_t env, const char *key, const char **value);


/**
 * @brief Looks for a value inside the bootenv data and converts it to num.
 * @param env Handle to a bootenv structure.
 * @param key The key.
 * @param value A pointer to the resulting numerical value
 * @return NULL	 key not found
 * @return !NULL ptr to value
 */
int bootenv_get_num(bootenv_t env, const char *key, uint32_t *value);

/**
 * @brief Set a bootenv value by key.
 * @param env   Handle to a bootenv structure.
 * @param key	Key.
 * @param value	Value to set.
 * @return 0
 * @return or error
 */
int bootenv_set(bootenv_t env, const char *key, const char *value);

/**
 * @brief Remove the given key (and its value) from a bootenv structure.
 * @param env	Handle to a bootenv structure.
 * @param key	Key.
 * @return 0
 * @return or error
 */
int bootenv_unset(bootenv_t env, const char *key);


/**
 * @brief Get a vector of all keys which are currently set
 *        within a bootenv handle.
 * @param env	Handle to a bootenv structure.
 * @param size	The size of the allocated array structure.
 * @param sort	Flag, if set the vector is sorted ascending.
 * @return NULL on error.
 * @return !NULL a pointer to the first element the allocated vector.
 * @warning Free the allocate memory yourself!
 */
int bootenv_get_key_vector(bootenv_t env, size_t *size, int sort,
				const char ***vector);

/**
 * @brief Calculate the size in bytes which are necessary to write the
 *        current bootenv section in a *binary file.
 * @param env	bootenv handle.
 * @param size  The size in bytes of the bootenv handle.
 * @return 0
 * @return or ERROR.
 */
int bootenv_size(bootenv_t env, size_t *size);

/**
 * @brief Read a binary bootenv file.
 * @param fp	File pointer to input stream.
 * @param env	bootenv handle.
 * @param size  maximum data size.
 * @return 0
 * @return or ERROR.
 */
int bootenv_read(FILE* fp, bootenv_t env, size_t size);

/**
 * @param ret_crc  return value of crc of read data
 */
int bootenv_read_crc(FILE* fp, bootenv_t env, size_t size, uint32_t *ret_crc);

/**
 * @brief Read bootenv data from an text/ascii file.
 * @param fp	File pointer to ascii PDD file.
 * @param env	bootenv handle
 * @return 0
 * @return or ERROR.
 */
int bootenv_read_txt(FILE* fp, bootenv_t env);

/**
 * @brief Write a bootenv structure to the given location (binary).
 * @param fp	Filepointer to binary file.
 * @param env	Bootenv structure which shall be written.
 * @return 0
 * @return or error
 */
int bootenv_write(FILE* fp, bootenv_t env);

/**
 * @param ret_crc  return value of crc of read data
 */
int bootenv_write_crc(FILE* fp, bootenv_t env, uint32_t* ret_crc);

/**
 * @brief Write a bootenv structure to the given location (text).
 * @param fp	Filepointer to text file.
 * @param env	Bootenv structure which shall be written.
 * @return 0
 * @return or error
 */
int bootenv_write_txt(FILE* fp, bootenv_t env);

/**
 * @brief Compare bootenvs using memcmp().
 * @param first	First bootenv.
 * @param second	Second bootenv.
 * @return 0 if bootenvs are equal
 * @return < 0 if error
 * @return > 0 if unequal
 */
int bootenv_compare(bootenv_t first, bootenv_t second);

/**
 * @brief Prototype for a PDD handling funtion
 */

/**
 * @brief The PDD keep operation.
 * @param env_old The old bootenv structure.
 * @param env_new The new bootenv structure.
 * @param env_res The result of PDD keep.
 * @param warnings A flag which marks any warnings.
 * @return 0
 * @return or error
 * @note For a complete documentation about the algorithm confer the
 *       PDD documentation.
 */
int bootenv_pdd_keep(bootenv_t env_old, bootenv_t env_new,
		bootenv_t *env_res, int *warnings,
		char *err_buf, size_t err_buf_size);


/**
 * @brief The PDD merge operation.
 * @param env_old The old bootenv structure.
 * @param env_new The new bootenv structure.
 * @param env_res The result of merge-pdd.
 * @param warnings A flag which marks any warnings.
 * @return 0
 * @return or error
 * @note For a complete documentation about the algorithm confer the
 *       PDD documentation.
 */
int bootenv_pdd_merge(bootenv_t env_old, bootenv_t env_new,
		bootenv_t *env_res, int *warnings,
		char *err_buf, size_t err_buf_size);

/**
 * @brief The PDD overwrite operation.
 * @param env_old The old bootenv structure.
 * @param env_new The new bootenv structure.
 * @param env_res The result of overwrite-pdd.
 * @param warnings A flag which marks any warnings.
 * @return 0
 * @return or error
 * @note For a complete documentation about the algorithm confer the
 *       PDD documentation.
 */
int bootenv_pdd_overwrite(bootenv_t env_new,
		bootenv_t env_old, bootenv_t *env_res, int *warnings,
		char *err_buf, size_t err_buf_size);

/**
 * @brief Dump a bootenv structure to stdout. (Debug)
 * @param env	Handle to a bootenv structure.
 * @return 0
 * @return or error
 */
int bootenv_dump(bootenv_t env);

/**
 * @brief Validate a bootenv structure.
 * @param env Handle to a bootenv structure.
 * @return 0
 * @return or error
 */
int bootenv_valid(bootenv_t env);

/**
 * @brief Create a new bootenv list structure.
 * @return NULL on error
 * @return or a new list handle.
 * @note This structure is used to store values in a list.
 *       A useful addition when handling PDD strings.
 */
int bootenv_list_create(bootenv_list_t *list);

/**
 * @brief Destroy a bootenv list structure
 * @param list	Handle to a bootenv list structure.
 * @return 0
 * @return or error
 */
int bootenv_list_destroy(bootenv_list_t *list);

/**
 * @brief Import a list from a comma seperated string
 * @param list	Handle to a bootenv list structure.
 * @param str		Comma seperated string list.
 * @return 0
 * @return or error
 */
int bootenv_list_import(bootenv_list_t list, const char *str);

/**
 * @brief Export a list to a string of comma seperated values.
 * @param list	Handle to a bootenv list structure.
 * @return NULL one error
 * @return or pointer to a newly allocated string.
 * @warning Free the allocated memory by yourself!
 */
int bootenv_list_export(bootenv_list_t list, char **string);

/**
 * @brief Add an item to the list.
 * @param list	A handle of a list structure.
 * @param item	An item.
 * @return 0
 * @return or error
 */
int bootenv_list_add(bootenv_list_t list, const char *item);

/**
 * @brief Remove an item from the list.
 * @param list	A handle of a list structure.
 * @param item	An item.
 * @return 0
 * @return or error
 */
int bootenv_list_remove(bootenv_list_t list, const char *item);

/**
 * @brief Check if a given item is in a given list.
 * @param list	A handle of a list structure.
 * @param item	An item.
 * @return 1 Item is in list.
 * @return 0 Item is not in list.
 */
int bootenv_list_is_in(bootenv_list_t list, const char *item);


/**
 * @brief Convert a list into a vector of all values inside the list.
 * @param list	Handle to a bootenv structure.
 * @param size	The size of the allocated vector structure.
 * @return 0
 * @return or error
 * @warning Free the allocate memory yourself!
 */
int bootenv_list_to_vector(bootenv_list_t list, size_t *size,
			   const char ***vector);

/**
 * @brief Convert a list into a vector of all values inside the list.
 * @param list	Handle to a bootenv structure.
 * @param size	The size of the allocated vector structure.
 * @return 0
 * @return or error
 * @warning Free the allocate memory yourself!
 */
int bootenv_list_to_num_vector(bootenv_list_t list, size_t *size,
					uint32_t **vector);

#ifdef __cplusplus
}
#endif
#endif /*__BOOTENV_H__ */
