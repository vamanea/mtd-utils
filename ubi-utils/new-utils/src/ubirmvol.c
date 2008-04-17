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
 * An utility to remove UBI volumes.
 *
 * Authors: Artem Bityutskiy <dedekind@infradead.org>
 *          Frank Haverkamp <haver@vnet.ibm.com>
 */

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <libubi.h>
#include "common.h"

#define PROGRAM_VERSION "1.0"
#define PROGRAM_NAME    "ubirmvol"

/* The variables below are set by command line arguments */
struct args {
	int vol_id;
	const char *node;
	/* For deprecated -d option handling */
	int devn;
	char dev_name[256];
};

static struct args args = {
	.vol_id = -1,
	.devn = -1,
};

static const char *doc = PROGRAM_NAME " version " PROGRAM_VERSION
				 " - a tool to remove UBI volumes.";

static const char *optionsstr =
"-n, --vol_id=<volume id>   volume ID to remove\n"
"-h, -?, --help             print help message\n"
"-V, --version              print program version\n\n"
"The following is a compatibility option which is deprecated, do not use it\n"
"-d, --devn=<devn>          UBI device number - may be used instead of the UBI\n"
"                           device node name in which case the utility assumes\n"
"                           that the device node is \"/dev/ubi<devn>\"";

static const char *usage =
"Usage: " PROGRAM_NAME " <UBI device node file name> [-n <volume id>] [--vol_id=<volume id>] [-h] [--help]\n\n"
"Example: " PROGRAM_NAME "/dev/ubi0 -n 1 - remove UBI volume 1 from UBI device corresponding\n"
"         to the node file /dev/ubi0.";

static const struct option long_options[] = {
	{ .name = "vol_id",  .has_arg = 1, .flag = NULL, .val = 'n' },
	{ .name = "help",    .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	/* Deprecated -d option */
	{ .name = "devn",    .has_arg = 1, .flag = NULL, .val = 'd' },
	{ NULL, 0, NULL, 0},
};

static int param_sanity_check(void)
{
	if (args.vol_id == -1) {
		errmsg("volume ID is was not specified");
		return -1;
	}

	return 0;
}

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key;
		char *endp;

		key = getopt_long(argc, argv, "n:h?Vd:", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {

		case 'n':
			args.vol_id = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || args.vol_id < 0) {
				errmsg("bad volume ID: " "\"%s\"", optarg);
				return -1;
			}
			break;

		case 'h':
		case '?':
			fprintf(stderr, "%s\n\n", doc);
			fprintf(stderr, "%s\n\n", usage);
			fprintf(stderr, "%s\n", optionsstr);
			exit(EXIT_SUCCESS);

		case 'd':
			/* Handle deprecated -d option */
			warnmsg("-d is depricated and will be removed, do not use it");
			args.devn = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || args.devn < 0)
				return errmsg("bad UBI device number: " "\"%s\"", optarg);
			break;

		case 'V':
			fprintf(stderr, "%s\n", PROGRAM_VERSION);
			exit(EXIT_SUCCESS);

		case ':':
			errmsg("parameter is missing");
			return -1;

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}

	/* Handle deprecated -d option */
	if (args.devn != -1) {
		sprintf(args.dev_name, "/dev/ubi%d", args.devn);
		args.node = args.dev_name;
	} else {
		if (optind == argc) {
			errmsg("UBI device name was not specified (use -h for help)");
			return -1;
		} else if (optind != argc - 1) {
			errmsg("more then one UBI device specified (use -h for help)");
			return -1;
		}

		args.node = argv[optind];
	}


	if (param_sanity_check())
		return -1;

	return 0;
}

int main(int argc, char * const argv[])
{
	int err;
	libubi_t libubi;

	err = parse_opt(argc, argv);
	if (err)
		return -1;

	libubi = libubi_open(1);
	if (libubi == NULL)
		return sys_errmsg("cannot open libubi");

	err = ubi_node_type(libubi, args.node);
	if (err == 2) {
		errmsg("\"%s\" is an UBI volume node, not an UBI device node",
		       args.node);
		goto out_libubi;
	} else if (err < 0) {
		errmsg("\"%s\" is not an UBI device node", args.node);
		goto out_libubi;
	}

	err = ubi_rmvol(libubi, args.node, args.vol_id);
	if (err) {
		sys_errmsg("cannot UBI remove volume");
		goto out_libubi;
	}

	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}
