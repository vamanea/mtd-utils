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

/*
 * This file contains various common stuff used by UBI utilities.
 */

#include <string.h>

/**
 * ubiutils_bytes_multiplier - convert size specifier to an integer
 *                             multiplier.
 *
 * @str: the size specifier string
 *
 * This function parses the @str size specifier, which may be one of
 * 'KiB', 'MiB', or 'GiB' into an integer multiplier. Returns positive
 * size multiplier in case of success and %-1 in case of failure.
 */
int ubiutils_get_multiplier(const char *str)
{
	if (!str)
		return 1;

	/* Remove spaces before the specifier */
	while (*str == ' ' || *str == '\t')
		str += 1;

	if (!strcmp(str, "KiB"))
		return 1024;
	if (!strcmp(str, "MiB"))
		return 1024 * 1024;
	if (!strcmp(str, "GiB"))
		return 1024 * 1024 * 1024;

	return -1;
}
