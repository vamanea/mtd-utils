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
 * An utility to attach MTD devices to UBI.
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
#define PROGRAM_NAME    "ubiattach"

/* The variables below are set by command line arguments */
struct args {
	int devn;
	int mtdn;
	int vidoffs;
	const char *node;
};

static struct args args = {
	.devn = UBI_DEV_NUM_AUTO,
	.mtdn = -1,
	.vidoffs = 0,
	.node = NULL,
};

static const char *doc = PROGRAM_NAME " version " PROGRAM_VERSION
			 " - a tool to attach MTD device to UBI.";

static const char *optionsstr =
"-d, --devn=<UBI device number>  the number to assign to the newly created UBI device\n"
"                                (the number is assigned automatically if this is not\n"
"                                specified\n"
"-m, --mtdn=<MTD device number>  MTD device number to attach\n"
"-O, --vid-hdr-offset            VID header offset (do not specify this unless you\n"
"                                really know what you do and the optimal defaults will\n"
"                                be used)\n"
"-h, --help                      print help message\n"
"-V, --version                   print program version";

static const char *usage =
"Usage: " PROGRAM_NAME " <UBI control device node file name> [-m <MTD device number>] [-d <UBI device number>]\n"
"\t\t[--mtdn=<MTD device number>] [--devn <UBI device number>]\n"
"Example 1: " PROGRAM_NAME " /dev/ubi_ctrl -m 0 - attach MTD device 0 (mtd0) to UBI\n"
"Example 2: " PROGRAM_NAME " /dev/ubi_ctrl -m 0 -d 3 - attach MTD device 0 (mtd0) to UBI and\n"
"           and create UBI device number 3 (ubi3)";

static const struct option long_options[] = {
	{ .name = "devn",           .has_arg = 1, .flag = NULL, .val = 'd' },
	{ .name = "mtdn",           .has_arg = 1, .flag = NULL, .val = 'm' },
	{ .name = "vid-hdr-offset", .has_arg = 1, .flag = NULL, .val = 'O' },
	{ .name = "help",           .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",        .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0},
};

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key;
		char *endp;

		key = getopt_long(argc, argv, "m:d:O:hV", long_options, NULL);
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

		case 'O':
			args.vidoffs = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || args.vidoffs <= 0)
				return errmsg("bad VID header offset: \"%s\"", optarg);

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

	if (args.mtdn == -1)
		return errmsg("MTD device number was not specified (use -h for help)");

	args.node = argv[optind];
	return 0;
}

int main(int argc, char * const argv[])
{
	int err;
	libubi_t libubi;
	struct ubi_info ubi_info;
	struct ubi_dev_info dev_info;
	struct ubi_attach_request req;

	err = parse_opt(argc, argv);
	if (err)
		return -1;

	libubi = libubi_open();
	if (!libubi) {
		if (errno == 0)
			return errmsg("UBI is not present in the system");
		return sys_errmsg("cannot open libubi");
	}

	/*
	 * Make sure the kernel is fresh enough and this feature is supported.
	 */
	err = ubi_get_info(libubi, &ubi_info);
	if (err) {
		sys_errmsg("cannot get UBI information");
		goto out_libubi;
	}

	if (ubi_info.ctrl_major == -1) {
		errmsg("MTD attach/detach feature is not supported by your kernel");
		goto out_libubi;
	}

	req.dev_num = args.devn;
	req.mtd_num = args.mtdn;
	req.vid_hdr_offset = args.vidoffs;

	err = ubi_attach_mtd(libubi, args.node, &req);
	if (err) {
		sys_errmsg("cannot attach mtd%d", args.mtdn);
		goto out_libubi;
	}

	/* Print some information about the new UBI device */
	err = ubi_get_dev_info1(libubi, req.dev_num, &dev_info);
	if (err) {
		sys_errmsg("cannot get information about newly created UBI device");
		goto out_libubi;
	}

	printf("UBI device number %d, total %d LEBs (", dev_info.dev_num, dev_info.total_lebs);
	ubiutils_print_bytes(dev_info.total_bytes, 0);
	printf("), available %d LEBs (", dev_info.avail_lebs);
	ubiutils_print_bytes(dev_info.avail_bytes, 0);
	printf("), LEB size ");
	ubiutils_print_bytes(dev_info.leb_size, 1);
	printf("\n");

	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}
