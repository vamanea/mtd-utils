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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>

#include "config.h"
#include "libubiold_int.h"

int
sysfs_read_data(const char *file, void *buf, int len)
{
	int fd;
	ssize_t rd;

	fd = open(file, O_RDONLY);
	if (fd == -1) {
		ubi_err("cannot open file %s", file);
		return -1;
	}

	rd = read(fd, buf, len);
	if (rd == -1)
		ubi_err("cannot read file %s", file);

	close(fd);

	return rd;
}

int
sysfs_read_data_subst(const char *patt, void *buf, int len, int n, ...)
{
	va_list args;
	char buf1[strlen(patt) + 20 * n];

	va_start(args, n);
	vsprintf(&buf1[0], patt, args);
	va_end(args);

	return sysfs_read_data(&buf1[0], buf, len);
}

int
sysfs_read_dev(const char *file, unsigned int *major, unsigned int *minor)
{
	int fd;
	int ret;
	ssize_t rd;
	int err = -1;
	char buf[40];

	fd = open(file, O_RDONLY);
	if (fd == -1) {
		ubi_err("cannot open file %s", file);
		return -1;
	}

	rd = read(fd, &buf[0], 20);
	if (rd == -1) {
		ubi_err("cannot read file %s", file);
		goto error;
	}
	if (rd < 4) {
		ubi_err("bad contents of file %s:", file);
		goto error;
	}

	err = -1;
	if (buf[rd -1] != '\n') {
		ubi_err("bad contents of file %s", file);
		goto error;
	}

	ret = sscanf(&buf[0], "%d:%d\n", major, minor);
	if (ret != 2) {
		ubi_err("bad contents of file %s", file);
		goto error;
	}

	err = 0;

error:
	close(fd);

	return err;
}

int
sysfs_read_dev_subst(const char *patt, unsigned int *major,
		unsigned int *minor, int n, ...)
{
	va_list args;
	char buf[strlen(patt) + 20 * n];

	va_start(args, n);
	vsprintf(&buf[0], patt, args);
	va_end(args);

	return sysfs_read_dev(&buf[0], major, minor);
}

static int
sysfs_read_ull(const char *file ubi_unused, unsigned long long *num ubi_unused)
{
	return 0;
}

int
sysfs_read_ull_subst(const char *patt, unsigned long long *num, int n, ...)
{
	va_list args;
	char buf[strlen(patt) + 20 * n];

	va_start(args, n);
	vsprintf(&buf[0], patt, args);
	va_end(args);

	return	sysfs_read_ull(&buf[0], num);
}

static int
sysfs_read_uint(const char *file ubi_unused, unsigned int *num ubi_unused)
{
	return 0;
}

int
sysfs_read_uint_subst(const char *patt, unsigned int *num, int n, ...)
{
	va_list args;
	char buf[strlen(patt) + 20 * n];

	va_start(args, n);
	vsprintf(&buf[0], patt, args);
	va_end(args);

	return	sysfs_read_uint(&buf[0], num);
}

int
sysfs_read_ll(const char *file, long long *num)
{
	int fd;
	ssize_t rd;
	int err = -1;
	char buf[20];
	char *endptr;

	fd = open(file, O_RDONLY);
	if (fd == -1)
		return -1;

	rd = read(fd, &buf[0], 20);
	if (rd == -1)
		goto out;

	if (rd < 2) {
		ubi_err("bad contents in file %s: \"%c%c...\"",
			file, buf[0], buf[1]);
		goto out_errno;
	}

	*num = strtoll(&buf[0], &endptr, 10);
	if (endptr == &buf[0] || *endptr != '\n') {
		ubi_err("bad contents in file %s: \"%c%c...\"",
			file, buf[0], buf[1]);
		goto out_errno;
	}

	if  (*num < 0) {
		ubi_err("bad number in file %s: %lld", file, *num);
		goto out_errno;
	}

	err = 0;

out_errno:
	errno = EINVAL;

out:
	close(fd);
	return err;
}

int
sysfs_read_int(const char *file, int *num)
{
	int err;
	long long res = 0;

	err = sysfs_read_ll(file, &res);
	if (err)
		return err;

	if (res < 0 || res > INT_MAX) {
		ubi_err("bad number in file %s: %lld", file, res);
		errno = EINVAL;
		return -1;
	}

	*num = res;
	return 0;
}
