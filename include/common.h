/*
 * Copyright (c) Artem Bityutskiy, 2007, 2008
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

#ifndef __MTD_UTILS_COMMON_H__
#define __MTD_UTILS_COMMON_H__

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

/*
 * Note, the user is supposed to define its PROGRAM_NAME before including this
 * header.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define MIN(a ,b) ((a) < (b) ? (a) : (b))
#define min(a, b) MIN(a, b) /* glue for linux kernel source */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* Verbose messages */
#define bareverbose(verbose, fmt, ...) do {                        \
	if (verbose)                                               \
		printf(fmt, ##__VA_ARGS__);                        \
} while(0)
#define verbose(verbose, fmt, ...) \
	bareverbose(verbose, PROGRAM_NAME ": " fmt "\n", ##__VA_ARGS__)

/* Normal messages */
#define normsg(fmt, ...) do {                              \
	printf(PROGRAM_NAME ": " fmt "\n", ##__VA_ARGS__); \
} while(0)
#define normsg_cont(fmt, ...) do {                         \
	printf(PROGRAM_NAME ": " fmt, ##__VA_ARGS__);      \
} while(0)

/* Error messages */
#define errmsg(fmt, ...)  ({                                                \
	fprintf(stderr, PROGRAM_NAME ": error!: " fmt "\n", ##__VA_ARGS__); \
	-1;                                                                 \
})

/* System error messages */
#define sys_errmsg(fmt, ...)  ({                                            \
	int _err = errno;                                                   \
	size_t _i;                                                           \
	fprintf(stderr, PROGRAM_NAME ": error!: " fmt "\n", ##__VA_ARGS__); \
	for (_i = 0; _i < sizeof(PROGRAM_NAME) + 1; _i++)                   \
		fprintf(stderr, " ");                                       \
	fprintf(stderr, "error %d (%s)\n", _err, strerror(_err));           \
	-1;                                                                 \
})

/* Warnings */
#define warnmsg(fmt, ...) do {                                                \
	fprintf(stderr, PROGRAM_NAME ": warning!: " fmt "\n", ##__VA_ARGS__); \
} while(0)

static inline int is_power_of_2(unsigned long long n)
{
	        return (n != 0 && ((n & (n - 1)) == 0));
}

#ifdef __cplusplus
}
#endif

#endif /* !__MTD_UTILS_COMMON_H__ */
