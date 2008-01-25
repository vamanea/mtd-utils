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
 * This test does a lot of I/O to volumes in parallel.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libubi.h"
#define TESTNAME "io_paral"
#include "common.h"

#define THREADS_NUM 3
#define ITERATIONS  10

static libubi_t libubi;
static struct ubi_dev_info dev_info;
const char *node;
static int iterations = ITERATIONS;
int total_bytes;

static long long memory_limit(void)
{
	long long result = 0;
	FILE *f;

	f = fopen("/proc/meminfo", "r");
	if (!f)
		return 0;
	fscanf(f, "%*s %lld", &result);
	fclose(f);
	return result * 1024 / 4;
}

/**
 * the_thread - the testing thread.
 *
 * @ptr  thread number
 */
static void * the_thread(void *ptr)
{
	int fd, iter = iterations, vol_id = (int)ptr;
	unsigned char *wbuf, *rbuf;
	char vol_node[strlen(UBI_VOLUME_PATTERN) + 100];

	wbuf = malloc(total_bytes);
	rbuf = malloc(total_bytes);
	if (!wbuf || !rbuf) {
		failed("malloc");
		goto free;
	}

	sprintf(vol_node, UBI_VOLUME_PATTERN, dev_info.dev_num, vol_id);

	while (iter--) {
		int i, ret, written = 0, rd = 0;
		int bytes = (random() % (total_bytes - 1)) + 1;

		fd = open(vol_node, O_RDWR);
		if (fd == -1) {
			failed("open");
			err_msg("cannot open \"%s\"\n", node);
			goto free;
		}

		for (i = 0; i < bytes; i++)
			wbuf[i] = random() % 255;
		memset(rbuf, '\0', bytes);

		do {
			ret = ubi_update_start(libubi, fd, bytes);
			if (ret && errno != EBUSY) {
				failed("ubi_update_start");
				err_msg("vol_id %d", vol_id);
				goto close;
			}
		} while (ret);

		while (written < bytes) {
			int to_write = random() % (bytes - written);

			if (to_write == 0)
				to_write = 1;

			ret = write(fd, wbuf, to_write);
			if (ret != to_write) {
				failed("write");
				err_msg("failed to write %d bytes at offset %d "
					"of volume %d", to_write, written,
					vol_id);
				err_msg("update: %d bytes", bytes);
				goto close;
			}

			written += to_write;
		}

		close(fd);

		fd = open(vol_node, O_RDONLY);
		if (fd == -1) {
			failed("open");
			err_msg("cannot open \"%s\"\n", node);
			goto free;
		}

		/* read data back and check */
		while (rd < bytes) {
			int to_read = random() % (bytes - rd);

			if (to_read == 0)
				to_read = 1;

			ret = read(fd, rbuf, to_read);
			if (ret != to_read) {
				failed("read");
				err_msg("failed to read %d bytes at offset %d "
					"of volume %d", to_read, rd, vol_id);
				goto close;
			}

			rd += to_read;
		}

		close(fd);

	}

	free(wbuf);
	free(rbuf);
	return NULL;

close:
	close(fd);
free:
	free(wbuf);
	free(rbuf);
	return NULL;
}

int main(int argc, char * const argv[])
{
	int i, ret;
	pthread_t threads[THREADS_NUM];
	struct ubi_mkvol_request req;
	long long mem_limit;

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

	req.alignment = 1;
	mem_limit = memory_limit();
	if (mem_limit && mem_limit < dev_info.avail_bytes)
		total_bytes = req.bytes =
				(mem_limit / dev_info.leb_size / THREADS_NUM)
				* dev_info.leb_size;
	else
		total_bytes = req.bytes =
				((dev_info.avail_lebs - 3) / THREADS_NUM)
				* dev_info.leb_size;
	for (i = 0; i < THREADS_NUM; i++) {
		char name[100];

		req.vol_id = i;
		sprintf(name, TESTNAME":%d", i);
		req.name = name;
		req.vol_type = (i & 1) ? UBI_STATIC_VOLUME : UBI_DYNAMIC_VOLUME;

		if (ubi_mkvol(libubi, node, &req)) {
			failed("ubi_mkvol");
			goto remove;
		}
	}

	/* Create one volume with static data to make WL work more */
	req.vol_id = THREADS_NUM;
	req.name = TESTNAME ":static";
	req.vol_type = UBI_DYNAMIC_VOLUME;
	req.bytes = 3*dev_info.leb_size;
	if (ubi_mkvol(libubi, node, &req)) {
		failed("ubi_mkvol");
		goto remove;
	}

	for (i = 0; i < THREADS_NUM; i++) {
		ret = pthread_create(&threads[i], NULL, &the_thread, (void*)i);
		if (ret) {
			failed("pthread_create");
			goto remove;
		}
	}

	for (i = 0; i < THREADS_NUM; i++)
		pthread_join(threads[i], NULL);

	for (i = 0; i <= THREADS_NUM; i++) {
		if (ubi_rmvol(libubi, node, i)) {
			failed("ubi_rmvol");
			goto remove;
		}
	}

	libubi_close(libubi);
	return 0;

remove:
	for (i = 0; i <= THREADS_NUM; i++)
		ubi_rmvol(libubi, node, i);

close:
	libubi_close(libubi);
	return 1;
}
