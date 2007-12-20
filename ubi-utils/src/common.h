/*
 * Copyright (c) Artem Bityutskiy, 2007
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

#ifndef __UBI_UTILS_COMMON_H__
#define __UBI_UTILS_COMMON_H__

/* Error messages */
#define errmsg(fmt, ...) do {                               \
        fprintf(stderr, "Error: " fmt "\n", ##__VA_ARGS__); \
} while(0)

/* Warnings */
#define warnmsg(fmt, ...) do {                              \
        fprintf(stderr, "Warning: " fmt "\n", ##__VA_ARGS__); \
} while(0)

int ubiutils_get_multiplier(const char *str);

#endif /* !__UBI_UTILS_COMMON_H__ */
