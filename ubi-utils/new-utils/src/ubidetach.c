/*
 * Copyright (C) 2007 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * An utility to delete UBI devices (detach MTD devices from UBI).
 *
 * Author: Artem Bityutskiy
 */

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <libubi.h>
#include "common.h"

#define PROGRAM_VERSION "1.0"
#define PROGRAM_NAME    "ubidetach"

/* The variables below are set by command line arguments */
struct args {
	int devn;
	int mtdn;
	const char *node;
};

static struct args args = {
	.devn = UBI_DEV_NUM_AUTO,
	.mtdn = -1,
	.node = NULL,
};

static const char *doc = PROGRAM_NAME " version " PROGRAM_VERSION
" - a tool to remove UBI devices (detach MTD devices from UBI)";

static const char *optionsstr =
"-d, --devn=<UBI device number>  UBI device number to delete\n"
"-m, --mtdn=<MTD device number>  or altrnatively, MTD device number to detach -\n"
"                                this will delete corresponding UBI device\n"
"-h, --help                      print help message\n"
"-V, --version                   print program version";

static const char *usage =
"Usage: " PROGRAM_NAME "<UBI control device node file name> [-d <UBI device number>] [-m <MTD device number>]\n"
"\t\t[--devn <UBI device number>] [--mtdn=<MTD device number>]\n"
"Example 1: " PROGRAM_NAME " /dev/ubi_ctrl -d 2 - delete UBI device 2 (ubi2)\n"
"Example 2: " PROGRAM_NAME " /dev/ubi_ctrl -m 0 - detach MTD device 0 (mtd0)";

static const struct option long_options[] = {
	{ .name = "devn",    .has_arg = 1, .flag = NULL, .val = 'd' },
	{ .name = "mtdn",    .has_arg = 1, .flag = NULL, .val = 'm' },
	{ .name = "help",    .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0},
};

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key;
		char *endp;

		key = getopt_long(argc, argv, "m:d:hV", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'd':
			args.devn = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || args.devn < 0)
				return errmsg("bad UBI device number: \"%s\"", optarg);

			break;

		case 'm':
			args.mtdn = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || args.mtdn < 0)
				return errmsg("bad MTD device number: \"%s\"", optarg);

			break;

		case 'h':
			fprintf(stderr, "%s\n\n", doc);
			fprintf(stderr, "%s\n\n", usage);
			fprintf(stderr, "%s\n", optionsstr);
			exit(EXIT_SUCCESS);

		case 'V':
			fprintf(stderr, "%s\n", PROGRAM_VERSION);
			exit(EXIT_SUCCESS);

		case ':':
			return errmsg("parameter is missing");

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}

	if (optind == argc)
		return errmsg("UBI control device name was not specified (use -h for help)");
	else if (optind != argc - 1)
		return errmsg("more then one UBI control device specified (use -h for help)");

	if (args.mtdn == -1 && args.devn == -1)
		return errmsg("neither MTD nor UBI devices were specified (use -h for help)");

	if (args.mtdn != -1 && args.devn != -1)
		return errmsg("specify either MTD or UBI device (use -h for help)");

	args.node = argv[optind];
	return 0;
}

int main(int argc, char * const argv[])
{
	int err;
	libubi_t libubi;
	struct ubi_info ubi_info;

	err = parse_opt(argc, argv);
	if (err)
		return -1;

	libubi = libubi_open(1);
	if (libubi == NULL)
		return sys_errmsg("cannot open libubi");

	/*
	 * Make sure the kernel is fresh enough and this feature is supported.
	 */
	err = ubi_get_info(libubi, &ubi_info);
	if (err) {
		sys_errmsg("cannot get UBI information");
		goto out_libubi;
	}

	if (ubi_info.ctrl_major == -1) {
		errmsg("MTD detach/detach feature is not supported by your kernel");
		goto out_libubi;
	}

	if (args.devn != -1) {
		err = ubi_remove_dev(libubi, args.node, args.devn);
		if (err) {
			sys_errmsg("cannot remove ubi%d", args.devn);
			goto out_libubi;
		}
	} else {
		err = ubi_detach_mtd(libubi, args.node, args.mtdn);
		if (err) {
			sys_errmsg("cannot detach mtd%d", args.mtdn);
			goto out_libubi;
		}
	}

	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}

