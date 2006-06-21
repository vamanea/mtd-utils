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
 * An utility to create UBI volumes.
 *
 * Autor: Artem B. Bityutskiy <dedekind@oktetlabs.ru>
 */

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include <libubi.h>

static void usage(void);
static int param_sanity_check(ubi_lib_t lib);
static int parse_options(int argc, char * const argv[]);

/*
 * The below variables are set by command line options.
 */
static int vol_id = -1;
static int devn = -1;

int main(int argc, char * const argv[])
{
	int err, old_errno;
	ubi_lib_t lib;

	err = parse_options(argc, argv);
	if (err)
		return err == 1 ? 0 : -1;

	if (devn == -1) {
		fprintf(stderr, "Device number was not specified\n");
		fprintf(stderr, "Use -h option for help\n");
		return -1;
	}

	err = ubi_open(&lib);
	if (err) {
		perror("Cannot open libubi");
		return -1;
	}

	err = param_sanity_check(lib);
	if (err) {
		perror("Input parameters check");
		fprintf(stderr, "Use -h option for help\n");
		goto out_libubi;
	}

	err = ubi_rmvol(lib, devn, vol_id);
	old_errno = errno;
	if (err < 0) {
		perror("Cannot remove volume");
		fprintf(stderr, "    err=%d errno=%d\n", err, old_errno);
		goto out_libubi;
	}

	return 0;

out_libubi:
	ubi_close(&lib);
	return -1;
}

/* 'getopt()' option string */
static const char *optstring = "hd:n:";

static int parse_options(int argc, char * const argv[])
{
	int opt = 0;

	while (opt != -1) {
		char *endp;

		opt = getopt(argc, argv, optstring);

		switch (opt) {
		case 'h':
			usage();
			return 1;
		case 'n':
			vol_id = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || vol_id < 0) {
				fprintf(stderr, "Bad volume "
					"number: \"%s\"\n", optarg);
				goto out;
			}
			break;
		case 'd':
			devn = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || devn < 0) {
				fprintf(stderr, "Bad UBI device "
					"number: \"%s\"\n", optarg);
				goto out;
			}
			break;
		case ':':
			fprintf(stderr, "Parameter is missing\n");
			goto out;
		case '?':
			fprintf(stderr, "Unknown parameter\n");
			goto out;
		case -1:
			break;
		default:
			fprintf(stderr, "Internal error\n");
			goto out;
		}
	}

	return 0;

out:
	errno = EINVAL;
	return -1;
}

static int param_sanity_check(ubi_lib_t lib)
{
	int err;
	struct ubi_info ubi;

	if (vol_id == -1) {
		fprintf(stderr, "Volume ID was not specified\n");
		goto out;
	}

	err = ubi_get_info(lib, &ubi);
	if (err)
		return -1;

	if (devn >= (int)ubi.dev_count) {
		fprintf(stderr, "Device %d does not exist\n", devn);
		goto out;
	}

	return 0;

out:
	errno = EINVAL;
	return -1;
}

static void usage(void)
{
	printf("Usage: ubi_rmvol OPTIONS\n"
	       "Command line options:\n"
	       "\t-h - this help message\n"
	       "\t-d - UBI device number\n"
	       "\t-n VOLNUM - volume number to remove\n");
}
