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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <memory.h>
#include <limits.h>
#include <fcntl.h>

#include <libubi.h>
#include "ubimirror.h"

#define COMPARE_BUF_SIZE    (128 * 1024)

#define DEFAULT_DEV_PATTERN    "/dev/ubi%d"
#define DEFAULT_VOL_PATTERN    "/dev/ubi%d_%d"

#define EBUF(fmt...) do {			\
	snprintf(err_buf, err_buf_size, fmt);	\
} while (0)

enum {
	compare_error = -1,
	seek_error = -2,
	write_error = -3,
	read_error = -4,
	update_error = -5,
	ubi_error = -6,
	open_error = -7,
	close_error = -8,
	compare_equal = 0,
	compare_different = 1
};

/*
 * Read len number of bytes from fd.
 * Return 0 on EOF, -1 on error.
 */
static ssize_t fill_buffer(int fd, unsigned char *buf, ssize_t len)
{
	ssize_t got, have = 0;

	do {
		got = read(fd, buf + have, len - have);
		if (got == -1 && errno != EINTR)
			return -1;
		have += got;
	} while (got > 0 && have < len);
	return have;
}

/*
 * Write len number of bytes to fd.
 * Return bytes written (>= 0), -1 on error.
 */
static ssize_t flush_buffer(int fd, unsigned char *buf, ssize_t len)
{
	ssize_t done, have = 0;

	do {
		done = write(fd, buf + have, len - have);
		if (done == -1 && errno != EINTR)
			return -1;
		have += done;
	} while (done > 0 && have < len);
	return have;
}

/*
 *  Compare two files.  Return 0, 1, or -1, depending on whether the
 *  files are equal, different, or an error occured.
 *  Return compare-different when target volume can not be read. Might be
 *  an interrupted volume update and then the target device returns -EIO but
 *  can be updated.
 *
 *  fd_a is source
 *  fd_b is destination
 */
static int compare_files(int fd_a, int fd_b)
{
	unsigned char buf_a[COMPARE_BUF_SIZE], buf_b[COMPARE_BUF_SIZE];
	ssize_t len_a, len_b;
	int rc;

	for (;;) {
		len_a = fill_buffer(fd_a, buf_a, sizeof(buf_a));
		if (len_a == -1) {
			rc = compare_error;
			break;
		}
		len_b = fill_buffer(fd_b, buf_b, sizeof(buf_b));
		if (len_b == -1) {
			rc = compare_different;
			break;
		}
		if (len_a != len_b) {
			rc = compare_different;
			break;
		}
		if (len_a == 0) {	/* Size on both files equal and EOF */
			rc = compare_equal;
			break;
		}
		if (memcmp(buf_a, buf_b, len_a) != 0 ) {
			rc = compare_different;
			break;
		}
	}
	/* Position both files at the beginning */
	if (lseek(fd_a, 0, SEEK_SET) == -1 ||
	   lseek(fd_b, 0, SEEK_SET) == -1)
		rc = seek_error;
	return rc;
}

int vol_get_used_bytes(int vol_fd, unsigned long long *bytes)
{
	off_t res;

	res = lseek(vol_fd, 0, SEEK_END);
	if (res == (off_t)-1)
		return -1;
	*bytes = (unsigned long long) res;
	res = lseek(vol_fd, 0, SEEK_SET);
	return res == (off_t)-1 ? -1 : 0;
}

static int copy_files(libubi_t ulib, int fd_in, int fd_out)
{
	unsigned char buf_a[COMPARE_BUF_SIZE];
	ssize_t len_a, len_b;
	unsigned long long update_size, copied;

	if (vol_get_used_bytes(fd_in, &update_size) == -1 ||
	    ubi_update_start(ulib, fd_out, update_size) == -1)
		return update_error;
	for (copied = 0; copied < update_size; copied += len_b ) {
		len_a = fill_buffer(fd_in, buf_a, sizeof(buf_a));
		if (len_a == -1)
			return read_error;
		if (len_a == 0)		/* Reach EOF */
			return 0;
		len_b = flush_buffer(fd_out, buf_a, len_a);
		if (len_b != len_a)
			return write_error;
	}
	return 0;
}

int ubimirror(uint32_t devno, int seqnum, uint32_t *ids, ssize_t ids_size,
		char *err_buf, size_t err_buf_size)
{
	int rc = 0;
	uint32_t src_id;
	char path[PATH_MAX];
	libubi_t ulib;
	int fd_in = -1, i = 0, fd_out = -1;

	if (ids_size == 0)
		return 0;
	else {
		if ((seqnum < 0) || (seqnum > (ids_size - 1))) {
			EBUF("volume id %d out of range", seqnum);
			return EUBIMIRROR_NO_SRC;
		}
		src_id = ids[seqnum];
	}

	ulib = libubi_open();
	if (ulib == NULL)
		return ubi_error;

	snprintf(path, PATH_MAX, DEFAULT_VOL_PATTERN, devno, src_id);

	fd_in = open(path, O_RDONLY);
	if (fd_in == -1) {
		EBUF("open error source volume %d", ids[i]);
		rc = open_error;
		goto err;
	}

	for (i = 0; i < ids_size; i++) {
		if (ids[i] == src_id)		/* skip self-mirror */
			continue;

		snprintf(path, PATH_MAX, DEFAULT_VOL_PATTERN, devno, ids[i]);

		fd_out = open(path, O_RDWR);
		if (fd_out < 0){
			EBUF("open error destination volume %d", ids[i]);
			rc = open_error;
			goto err;
		}
		rc = compare_files(fd_in, fd_out);
		if (rc < 0) {
			EBUF("compare error volume %d and %d", src_id, ids[i]);
			goto err;
		} else if (rc == compare_different) {
			rc = copy_files(ulib, fd_in, fd_out);
			if (rc != 0) {
				EBUF("mirror error volume %d to %d", src_id,
						ids[i]);
				goto err;
			}
		}
		if ((rc = close(fd_out)) == -1) {
			EBUF("close error volume %d", ids[i]);
			rc = close_error;
			goto err;
		} else
			fd_out = -1;
	}
err:
	if (fd_out != -1)
		close(fd_out);
	if (fd_in != -1)
		close(fd_in);
	if (ulib != NULL)
		libubi_close(ulib);
	return rc;
}
