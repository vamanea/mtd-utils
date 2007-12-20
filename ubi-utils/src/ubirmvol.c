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
 * Authors: Artem B. Bityutskiy <dedekind@infradead.org>
 *          Frank Haverkamp <haver@vnet.ibm.com>
 */

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <config.h>
#include <libubi.h>
#include "common.h"

#define PROGRAM_VERSION "1.5"
#define PROGRAM_NAME    "ubirmvol"

/* The variables below is set by command line arguments */
struct args {
	int devn;
	int vol_id;
	char node[MAX_NODE_LEN + 1];
};

static struct args myargs = {
	.devn = -1,
	.vol_id = -1,
};

static int param_sanity_check(struct args *args, libubi_t libubi);

static const char *doc = "Version: " PROGRAM_VERSION "\n"
	PROGRAM_NAME " - a tool to remove UBI volumes.";

static const char *optionsstr =
"  -n, --vol_id=<volume id>   volume ID to remove\n"
"  -h, --help                 print help message\n"
"  -V, --version              print program version";

static const char *usage =
"Usage: " PROGRAM_NAME " <UBI device node file name> [-n <volume id>] [--vol_id=<volume id>] [-h] [--help]\n\n"
"Example: " PROGRAM_NAME "/dev/ubi0 -n 1 - remove UBI volume 1 from UBI device corresponding\n"
"         to the node file /dev/ubi0.";

static const struct option long_options[] = {
	{ .name = "devn",    .has_arg = 1, .flag = NULL, .val = 'd' },
	{ .name = "vol_id",  .has_arg = 1, .flag = NULL, .val = 'n' },
	{ .name = "help",    .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0},
};

static int parse_opt(int argc, char * const argv[], struct args *args)
{
	char *endp;

	while (1) {
		int key;

		key = getopt_long(argc, argv, "d:n:hV", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {

		case 'd':
			args->devn = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || args->devn < 0) {
				errmsg("bad UBI device number: \"%s\"", optarg);
				return -1;
			}

			warnmsg("'-d' and '--devn' options are deprecated and will be "
				"removed. Specify UBI device node name instead!\n"
				"Example: " PROGRAM_NAME " /dev/ubi0, instead of "
				PROGRAM_NAME " -d 0");
			sprintf(args->node, "/dev/ubi%d", args->devn);
			break;

		case 'n':
			args->vol_id = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || args->vol_id < 0) {
				errmsg("bad volume ID: " "\"%s\"", optarg);
				return -1;
			}
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
			goto out;

		default:
			fprintf(stderr, "Use -h for help\n");
			exit(-1);
		}
	}

	return 0;
 out:
	return -1;
}

static int param_sanity_check(struct args *args, libubi_t libubi)
{
	int err;

	if (strlen(args->node) > MAX_NODE_LEN) {
		errmsg("too long device node name: \"%s\" (%d characters), max. is %d",
		       args->node, strlen(args->node), MAX_NODE_LEN);
		return -1;
	}

	if (args->vol_id == -1) {
		errmsg("volume ID is was not specified");
		return -1;
	}

	if (args->devn != -1) {
		struct ubi_info ubi;

		err = ubi_get_info(libubi, &ubi);
		if (err) {
			errmsg("cannot get UBI information");
			perror("ubi_get_info");
			return -1;
		}

		if (args->devn >= ubi.dev_count) {
			errmsg("UBI device %d does not exist", args->devn);
			return -1;
		}
	}

	return 0;
}

int main(int argc, char * const argv[])
{
	int err;
	libubi_t libubi;

	strncpy(myargs.node, argv[1], MAX_NODE_LEN);

	err = parse_opt(argc, argv, &myargs);
	if (err)
		return -1;

	if (argc < 2) {
		errmsg("UBI device name was not specified (use -h for help)");
		return -1;
	}

	if (argc < 3) {
		errmsg("too few arguments (use -h for help)");
		return -1;
	}

	libubi = libubi_open();
	if (libubi == NULL) {
		errmsg("cannot open libubi");
		perror("libubi_open");
		return -1;
	}

	err = param_sanity_check(&myargs, libubi);
	if (err)
		goto out_libubi;

	err = ubi_rmvol(libubi, myargs.node, myargs.vol_id);
	if (err) {
		errmsg("cannot UBI remove volume");
		perror("ubi_rmvol");
		goto out_libubi;
	}

	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}
