#ifndef __CRC32_H__
#define __CRC32_H__
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
 * Author: Thomas Gleixner
 *
 * CRC32 functions
 *
 * Can be compiled as seperate object, but is included into the ipl source
 * so gcc can inline the functions. We optimize for size so the omission of
 * the function frame is helpful.
 *
 */
#include <stdint.h>

void init_crc32_table(uint32_t *table);
uint32_t clc_crc32(uint32_t *table, uint32_t crc, void *buf, int len);

#endif /* __CRC32_H__ */
