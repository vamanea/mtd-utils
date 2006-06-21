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
 * UBI (Unsorted Block Images) library.
 *
 * Author: Artem B. Bityutskiy
 */

/**
 * sysfs_read_data - read data from a sysfs file.
 *
 * @file  path to the file to read from
 * @buf	  furrer where to store read data
 * @len	  length of provided buffer @buf
 *
 * This function returns the number of read bytes or -1 in case of error.
 */
int sysfs_read_data(const char *file, void *buf, int len);

/**
 * sysfs_read_data_subst - form path to a sysfs file and read data from it.
 *
 * @patt  path to the file to read from
 * @buf	  furrer where to store read data
 * @len	  length of provided buffer @buf
 * @n	  number of parameters to substitute to @patt
 *
 * This function forms path to a sysfs file by means of substituting parameters
 * to @patt and then reads @len bytes from this file and stores the read data
 * to @buf. This function returns the number of read bytes or -1 in case of
 * error.
 */
int sysfs_read_data_subst(const char *patt, void *buf, int len, int n, ...);

/**
 * sysfs_read_dev - read major and minor number from a sysfs file.
 *
 * @file   path to the file to read from
 * @major  major number is returned here
 * @minor  minor number is returned here
 */
int sysfs_read_dev(const char *file, unsigned int *major,
		unsigned int *minor);
/**
 * sysfs_read_dev_subst - for path to a file and read major and minor number
 * from it.
 *
 * @patt   pattern of the path to the file to read from
 * @major  major number is returned here
 * @minor  minor number is returned here
 * @n	   number of arguments to substitute
 *
 * This function substitures arguments to the @patt file path pattern and reads
 * major and minor numbers from the resulting file.
 */
int sysfs_read_dev_subst(const char *patt, unsigned int *major,
		unsigned int *minor, int n, ...);

/**
 * sysfs_read_ull_subst - form path to a sysfs file and read an unsigned long
 * long value from there.
 *
 * @patt  pattern of file path
 * @num	  the read value is returned here
 * @n	  number of parameters to substitute
 *
 *
 * This function first forms the path to a sysfs file by means of substituting
 * passed parameters to the @patt string, and then read an 'unsigned long long'
 * value from this file.
 */
int sysfs_read_ull_subst(const char *patt, unsigned long long *num,
		int n, ...);

/**
 * sysfs_read_uint_subst - the same as 'sysfs_read_uint_subst()' but reads an
 * unsigned int value.
 */
int sysfs_read_uint_subst(const char *patt, unsigned int *num,
		int n, ...);

/**
 * sysfs_read_ll - read a long long integer from an UBI sysfs file.
 *
 * @file  file name from where to read
 * @num	  the result is returned here
 */
int sysfs_read_ll(const char *file, long long *num);

/**
 * sysfs_read_int - the same as 'sysfs_read_ll()' but reads an 'int' value.
 */
int sysfs_read_int(const char *file, int *num);
