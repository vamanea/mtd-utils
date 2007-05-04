#ifndef __UBIMIRROR_H__
#define __UBIMIRROR_H__
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
 * An utility to mirror UBI volumes.
 */

#include <stdint.h>

/**
 * @def EUBIMIRROR_SRC_EQ_DST
 *	@brief Given source volume is also in the set of destination volumes.
 */
#define EUBIMIRROR_SRC_EQ_DST	20

/**
 * @def EUBIMIRROR_NO_SRC
 *	@brief The given source volume does not exist.
 */
#define EUBIMIRROR_NO_SRC	21

/**
 * @def EUBIMIRROR_NO_DST
 *	@brief One of the given destination volumes does not exist.
 */
#define EUBIMIRROR_NO_DST	22

/**
 * @brief Mirrors UBI devices from a source device (specified by seqnum)
 *	  to n target devices.
 * @param devno Device number used by the UBI operations.
 * @param seqnum An index into ids (defines the src_id).
 * @param ids An array of ids.
 * @param ids_size The number of entries in the ids array.
 * @param err_buf A buffer to store verbose error messages.
 * @param err_buf_size The size of the error buffer.
 *
 * @note A seqnum of value < 0 defaults to a seqnum of 0.
 * @note A seqnum exceeding the range of ids_size defaults to 0.
 * @note An empty ids list results in a empty stmt.
 * @pre	The UBI volume which shall be used as source volume exists.
 * @pre	The UBI volumes which are defined as destination volumes exist.
 * @post The content of the UBI volume which was defined as source volume
 *	 equals the content of the volumes which were defined as destination.
 */
int ubimirror(uint32_t devno, int seqnum, uint32_t* ids, ssize_t ids_size,
	      char *err_buf, size_t err_buf_size);

#endif /* __UBIMIRROR_H__ */
