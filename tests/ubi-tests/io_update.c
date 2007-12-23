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
 * Author: Artem B. Bityutskiy
 *
 * Test UBI volume update.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libubi.h"
#define TESTNAME "io_update"
#include "common.h"

static libubi_t libubi;
static struct ubi_dev_info dev_info;
const char *node;

static int test_update(int type);
static int test_update_ff(void);

int main(int argc, char * const argv[])
{
	if (initial_check(argc, argv))
		return 1;

	node = argv[1];

	libubi = libubi_open();
	if (libubi == NULL) {
		failed("libubi_open");
		return 1;
	}

	if (ubi_get_dev_info(libubi, node, &dev_info)) {
		failed("ubi_get_dev_info");
		goto close;
	}

	if (test_update(UBI_DYNAMIC_VOLUME))
		goto close;
	if (test_update(UBI_STATIC_VOLUME))
		goto close;
	if (test_update_ff())
		goto close;

	libubi_close(libubi);
	return 0;

close:
	libubi_close(libubi);
	return 1;
}

static int test_update1(struct ubi_vol_info *vol_info);

/**
 * test_update - check volume update capabilities.
 *
 * @type  volume type (%UBI_DYNAMIC_VOLUME or %UBI_STATIC_VOLUME)
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int test_update(int type)
{
	struct ubi_mkvol_request req;
	const char *name = TESTNAME ":io_update()";
	int alignments[] = ALIGNMENTS(dev_info.leb_size);
	struct ubi_vol_info vol_info;
	char vol_node[strlen(UBI_VOLUME_PATTERN) + 100];
	int i;

	for (i = 0; i < sizeof(alignments)/sizeof(int); i++) {
		int leb_size;

		req.vol_id = UBI_VOL_NUM_AUTO;
		req.vol_type = type;
		req.name = name;

		req.alignment = alignments[i];
		req.alignment -= req.alignment % dev_info.min_io_size;
		if (req.alignment == 0)
			req.alignment = dev_info.min_io_size;

		leb_size = dev_info.leb_size - dev_info.leb_size % req.alignment;
		req.bytes =  MIN_AVAIL_EBS * leb_size;

		if (ubi_mkvol(libubi, node, &req)) {
			failed("ubi_mkvol");
			return -1;
		}

		sprintf(&vol_node[0], UBI_VOLUME_PATTERN, dev_info.dev_num,
			req.vol_id);
		if (ubi_get_vol_info(libubi, vol_node, &vol_info)) {
			failed("ubi_get_vol_info");
			goto remove;
		}

		if (test_update1(&vol_info)) {
			err_msg("alignment = %d", req.alignment);
			goto remove;
		}

		if (ubi_rmvol(libubi, node, req.vol_id)) {
			failed("ubi_rmvol");
			return -1;
		}
	}

	return 0;

remove:
	ubi_rmvol(libubi, node, req.vol_id);
	return -1;
}

#define SEQUENCES(io, s) {           \
	{3*(s)-(io)-1, 1},           \
	{512},                       \
	{666},                       \
	{2048},                      \
	{(io), (io), PAGE_SIZE},     \
	{(io)+1, (io)+1, PAGE_SIZE}, \
	{PAGE_SIZE},                 \
	{PAGE_SIZE-1},               \
	{PAGE_SIZE+(io)},            \
	{(s)},                       \
	{(s)-1},                     \
	{(s)+1},                     \
	{(io), (s)+1},               \
	{(s)+(io), PAGE_SIZE},       \
	{2*(s), PAGE_SIZE},          \
	{PAGE_SIZE, 2*(s), 1},       \
	{PAGE_SIZE, 2*(s)},          \
	{2*(s)-1, 2*(s)-1},          \
	{3*(s), PAGE_SIZE + 1},      \
	{1, PAGE_SIZE},              \
	{(io), (s)}                  \
}
#define SEQ_SZ 21

/*
 * test_update1 - helper function for test_update().
 */
static int test_update1(struct ubi_vol_info *vol_info)
{
	int sequences[SEQ_SZ][3] = SEQUENCES(dev_info.min_io_size,
					     vol_info->leb_size);
	char vol_node[strlen(UBI_VOLUME_PATTERN) + 100];
	unsigned char buf[vol_info->rsvd_bytes];
	int fd, i, j;

	sprintf(&vol_node[0], UBI_VOLUME_PATTERN, dev_info.dev_num,
		vol_info->vol_id);

	for (i = 0; i < vol_info->rsvd_bytes; i++)
		buf[i] = (unsigned char)i;

	fd = open(vol_node, O_RDWR);
	if (fd == -1) {
		failed("open");
		err_msg("cannot open \"%s\"\n", node);
		return -1;
	}

	for (i = 0; i < SEQ_SZ; i++) {
		int ret, stop = 0, len;
		off_t off = 0;
		unsigned char buf1[vol_info->rsvd_bytes];

		if (ubi_update_start(libubi, fd, vol_info->rsvd_bytes)) {
			failed("ubi_update_start");
			goto close;
		}

		for (j = 0; off < vol_info->rsvd_bytes; j++) {
			if (!stop) {
				if (sequences[i][j] != 0)
					len = sequences[i][j];
				else
					stop = 1;
			}

			ret = write(fd, &buf[off], len);
			if (ret < 0) {
				failed("write");
				err_msg("failed to write %d bytes at offset "
					"%lld", len, (long long) off);
				goto close;
			}
			if (off + len > vol_info->rsvd_bytes)
				len = vol_info->rsvd_bytes - off;
			if (ret != len) {
				err_msg("failed to write %d bytes at offset "
					"%lld, wrote %d", len, (long long) off, ret);
				goto close;
			}
			off += len;
		}

		/* Check data */
		if ((ret = lseek(fd, SEEK_SET, 0)) != 0) {
			if (ret < 0)
				failed("lseek");
			err_msg("cannot seek to 0");
			goto close;
		}
		memset(&buf1[0], 0x01, vol_info->rsvd_bytes);
		ret = read(fd, &buf1[0], vol_info->rsvd_bytes + 1);
		if (ret < 0) {
			failed("read");
			err_msg("failed to read %d bytes",
				vol_info->rsvd_bytes + 1);
			goto close;
		}
		if (ret != vol_info->rsvd_bytes) {
			err_msg("failed to read %d bytes, read %d",
				vol_info->rsvd_bytes, ret);
			goto close;
		}
		if (memcmp(&buf[0], &buf1[0], vol_info->rsvd_bytes)) {
			err_msg("data corruption");
			goto close;
		}
	}

	close(fd);
	return 0;

close:
	close(fd);
	return -1;
}

/**
 * test_update_ff - check volume with 0xFF data
 *
 * This function returns %0 in case of success and %-1 in case of failure.
 */
static int test_update_ff(void)
{
	struct ubi_mkvol_request req;
	const char *name = TESTNAME ":io_update()";
	struct ubi_vol_info vol_info;
	char vol_node[strlen(UBI_VOLUME_PATTERN) + 100];
	int i, fd, ret, types[2];
	int upd_len = MIN_AVAIL_EBS * dev_info.leb_size;
	char buf[upd_len], buf1[upd_len];

	for(i = 0; i < MIN_AVAIL_EBS; i++) {
		if (i % 1)
			memset(&buf[0], 0xAB, upd_len);
		else
			memset(&buf[0], 0xFF, upd_len);
	}

	types[0] = UBI_DYNAMIC_VOLUME;
	types[1] = UBI_STATIC_VOLUME;

	for (i = 0; i < 2; i++) {
		req.vol_id = UBI_VOL_NUM_AUTO;
		req.vol_type = types[i];
		req.name = name;

		req.alignment = 1;
		req.bytes = upd_len;

		if (ubi_mkvol(libubi, node, &req)) {
			failed("ubi_mkvol");
			return -1;
		}

		sprintf(&vol_node[0], UBI_VOLUME_PATTERN, dev_info.dev_num,
			req.vol_id);
		if (ubi_get_vol_info(libubi, vol_node, &vol_info)) {
			failed("ubi_get_vol_info");
			goto remove;
		}

		fd = open(vol_node, O_RDWR);
		if (fd == -1) {
			failed("open");
			err_msg("cannot open \"%s\"\n", node);
			goto remove;
		}

		if (ubi_update_start(libubi, fd, upd_len)) {
			failed("ubi_update_start");
			goto close;
		}


		ret = write(fd, &buf[0], upd_len);
		if (ret < 0 || ret != upd_len) {
			failed("write");
			err_msg("failed to write %d bytes", upd_len);
			goto close;
		}

		/* Check data */
		if ((ret = lseek(fd, SEEK_SET, 0)) != 0) {
			if (ret < 0)
				failed("lseek");
			err_msg("cannot seek to 0");
			goto close;
		}

		close(fd);

		fd = open(vol_node, O_RDWR);
		if (fd == -1) {
			failed("open");
			err_msg("cannot open \"%s\"\n", node);
			goto remove;
		}

		memset(&buf1[0], 0x00, upd_len);
		ret = read(fd, &buf1[0], upd_len);
		if (ret < 0) {
			failed("read");
			err_msg("failed to read %d bytes", upd_len);
			goto close;
		}
		if (ret != upd_len) {
			err_msg("failed to read %d bytes, read %d",
				upd_len, ret);
			goto close;
		}
		if (memcmp(&buf[0], &buf1[0], upd_len)) {
			err_msg("data corruption");
			goto close;
		}

		close(fd);

		if (ubi_rmvol(libubi, node, req.vol_id)) {
			failed("ubi_rmvol");
			return -1;
		}
	}

	return 0;

close:
	close(fd);
remove:
	ubi_rmvol(libubi, node, req.vol_id);
	return -1;
}
