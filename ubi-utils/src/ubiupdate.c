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
 * An utility to update UBI volumes.
 *
 * Authors: Frank Haverkamp
 *          Joshua W. Boyer
 *          Artem Bityutskiy
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <libubi.h>
#include "common.h"

#define PROGRAM_VERSION "1.3"
#define PROGRAM_NAME    "ubiupdate"

struct args {
	int truncate;
	const char *node;
	const char *img;
};

static struct args myargs = {
	.truncate = 0,
	.node = NULL,
	.img = NULL,
};

static const char *doc = "Version " PROGRAM_VERSION "\n"
	PROGRAM_NAME " - a tool to write data to UBI volumes.";

static const char *optionsstr =
"-n, --vol_id=<volume id>   ID of UBI volume to update\n"
"-t, --truncate             truncate volume (wipe it out)\n"
"-h, --help                 print help message\n"
"-V, --version              print program version";

static const char *usage =
"Usage: " PROGRAM_NAME " <UBI volume node file name> [-t] [-h] [-V] [--truncate] [--help]\n"
"\t\t[--version] <image file>\n\n"
"Example 1: " PROGRAM_NAME " /dev/ubi0_1 fs.img - write file \"fs.img\" to UBI volume /dev/ubi0_1\n"
"Example 2: " PROGRAM_NAME " /dev/ubi0_1 -t - wipe out UBI volume /dev/ubi0_1";

struct option long_options[] = {
	{ .name = "truncate", .has_arg = 0, .flag = NULL, .val = 't' },
	{ .name = "help",     .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",  .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key;

		key = getopt_long(argc, argv, "n:thV", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 't':
			myargs.truncate = 1;
			break;

		case 'h':
			fprintf(stderr, "%s\n\n", doc);
			fprintf(stderr, "%s\n\n", usage);
			fprintf(stderr, "%s\n", optionsstr);
			exit(0);

		case 'V':
			fprintf(stderr, "%s\n", PROGRAM_VERSION);
			exit(0);

		case ':':
			errmsg("parameter is missing");
			return -1;

		default:
			fprintf(stderr, "Use -h for help\n");
			exit(-1);
		}
	}

	if (optind == argc) {
		errmsg("UBI device name was not specified (use -h for help)");
		return -1;
	} else if (optind != argc - 2) {
		errmsg("specify UBI device name and image file name as first 2 "
		       "parameters (use -h for help)");
		return -1;
	}

	myargs.node = argv[optind];
	myargs.img  = argv[optind + 1];

	return 0;
}

static int truncate_volume(libubi_t libubi)
{
	int err, fd;

	fd = open(myargs.node, O_RDWR);
	if (fd == -1) {
		errmsg("cannot open \"%s\"", myargs.node);
		perror("open");
		return -1;
	}

	err = ubi_update_start(libubi, fd, 0);
	if (err) {
		errmsg("cannot truncate volume \"%s\"", myargs.node);
		perror("ubi_update_start");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

static int ubi_write(int fd, const void *buf, int len)
{
	int ret;

	while (len) {
		ret = write(fd, buf, len);
		if (ret < 0) {
			if (errno == EINTR) {
				warnmsg("do not interrupt me!");
				continue;
			}
			errmsg("cannot write %d bytes to volume \"%s\"",
			       len, myargs.node);
			perror("write");
			return -1;
		}

		if (ret == 0) {
			errmsg("cannot write %d bytes to volume \"%s\"",
			       len, myargs.node);
			return -1;
		}

		len -= ret;
		buf += ret;
	}

	return 0;
}

static int update_volume(libubi_t libubi, struct ubi_vol_info *vol_info)
{
	int err, fd, ifd;
	long long bytes;
	struct stat st;
	char *buf;

	buf = malloc(vol_info->leb_size);
	if (!buf) {
		errmsg("cannot allocate %d bytes of memory", vol_info->leb_size);
		return -1;
	}

	err = stat(myargs.img, &st);
	if (err < 0) {
		errmsg("stat failed on \"%s\"", myargs.node);
		goto out_free;
	}

	bytes = st.st_size;
	if (bytes > vol_info->rsvd_bytes) {
		errmsg("\"%s\" (size %lld) will not fit volume \"%s\" (size %lld)",
		       myargs.img, bytes, myargs.node, vol_info->rsvd_bytes);
		goto out_free;
	}

	fd = open(myargs.node, O_RDWR);
	if (fd == -1) {
		errmsg("cannot open UBI volume \"%s\"", myargs.node);
		perror("open");
		goto out_free;
	}

	ifd = open(myargs.img, O_RDONLY);
	if (ifd == -1) {
		errmsg("cannot open \"%s\"", myargs.img);
		perror("open");
		goto out_close1;
	}

	err = ubi_update_start(libubi, fd, bytes);
	if (err) {
		errmsg("cannot start volume \"%s\" update", myargs.node);
		perror("ubi_update_start");
		goto out_close;
	}

	while (bytes) {
		int tocopy = vol_info->leb_size;

		if (tocopy > bytes)
			tocopy = bytes;

		err = read(ifd, buf, tocopy);
		if (err != tocopy) {
			if (errno == EINTR) {
				warnmsg("do not interrupt me!");
				continue;
			} else {
				errmsg("cannot read %d bytes from \"%s\"",
				       tocopy, myargs.img);
				perror("read");
				goto out_close;
			}
		}

		err = ubi_write(fd, buf, tocopy);
		if (err)
			goto out_close;
		bytes -= tocopy;
	}

	close(ifd);
	close(fd);
	free(buf);
	return 0;

out_close:
	close(ifd);
out_close1:
	close(fd);
out_free:
	free(buf);
	return -1;
}

int main(int argc, char * const argv[])
{
	int err;
	libubi_t libubi;
	struct ubi_vol_info vol_info;

	err = parse_opt(argc, argv);
	if (err)
		return -1;

	if (!myargs.img && !myargs.truncate) {
		errmsg("incorrect arguments, use -h for help");
		return -1;
	}

	libubi = libubi_open();
	if (libubi == NULL) {
		perror("Cannot open libubi");
		goto out_libubi;
	}

	err = ubi_node_type(libubi, myargs.node);
	if (err == 1) {
		errmsg("\"%s\" is an UBI device node, not an UBI volume node",
		       myargs.node);
		goto out_libubi;
	} else if (err < 0) {
		errmsg("\"%s\" is not an UBI volume node", myargs.node);
		goto out_libubi;
	}

	err = ubi_get_vol_info(libubi, myargs.node, &vol_info);
	if (err) {
		errmsg("cannot get information about UBI volume \"%s\"",
		       myargs.node);
		perror("ubi_get_dev_info");
		goto out_libubi;
	}

	if (myargs.truncate)
		err = truncate_volume(libubi);
	else
		err = update_volume(libubi, &vol_info);
	if (err)
		goto out_libubi;

	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}
