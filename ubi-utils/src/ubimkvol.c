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
 * Author: Artem B. Bityutskiy <dedekind@oktetlabs.ru>
 *
 * 1.0 Initial release
 * 1.1 Does not support erase blocks anymore. This is replaced by
 *     the number of bytes.
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
 * The variables below  are set by command line arguments.
 */
static int vol_type = UBI_DYNAMIC_VOLUME;
static int devn = -1;
static long long bytes = 0;
static int alignment = 1;
static int vol_id = UBI_VOL_NUM_AUTO;
static char *name = NULL;
static int nlen = 0;

int main(int argc, char * const argv[])
{
	int err;
	ubi_lib_t lib;

	err = parse_options(argc, argv);
	if (err) {
		fprintf(stderr, "Wrong options ...\n");
		return err == 1 ? 0 : -1;
	}

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

	err = ubi_mkvol(lib, devn, vol_id, vol_type, bytes, alignment, name);
	if (err < 0) {
		perror("Cannot create volume");
		fprintf(stderr, "  err=%d\n", err);
		goto out_libubi;
	}

	/* printf("Created volume %d, %lld bytes, type %s, name %s\n",
	   vol_id, bytes, vol_type == UBI_DYNAMIC_VOLUME ?
	   "dynamic" : "static", name); */

	vol_id = err;
	ubi_close(&lib);
	return 0;

out_libubi:
	ubi_close(&lib);
	return -1;
}

/* 'getopt()' option string */
static const char *optstring = "ht:s:n:N:d:a:";

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
		case 't':
			if (!strcmp(optarg, "dynamic"))
				vol_type = UBI_DYNAMIC_VOLUME;
			else if (!strcmp(optarg, "static"))
				vol_type = UBI_STATIC_VOLUME;
			else {
				fprintf(stderr, "Bad volume type: \"%s\"\n",
					optarg);
				goto out;
			}
			break;
		case 's':
			bytes = strtoull(optarg, &endp, 0);
			if (endp == optarg || bytes < 0) {
				fprintf(stderr, "Bad volume size: \"%s\"\n",
					optarg);
				goto out;
			}
			if (endp != '\0') {
				if (strcmp(endp, "KiB") == 0)
					bytes *= 1024;
				else if (strcmp(endp, "MiB") == 0)
					bytes *= 1024*1024;
			}
			break;
		case 'a':
			alignment = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg ||
			    alignment <= 0) {
				fprintf(stderr, "Bad volume alignment: "
					"\"%s\"\n", optarg);
				goto out;
			}
			break;
		case 'd':
			devn = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || devn < 0) {
				fprintf(stderr, "Bad UBI device number: "
					"\"%s\"\n", optarg);
				goto out;
			}
			break;
		case 'n':
			vol_id = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg ||
			    (vol_id < 0 && vol_id != UBI_DYNAMIC_VOLUME)) {
				fprintf(stderr, "Bad volume ID: "
					"\"%s\"\n", optarg);
				goto out;
			}
			break;
		case 'N':
			name = optarg;
			nlen = strlen(name);
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
	int err, len;
	struct ubi_info ubi;

	if (bytes == 0) {
		fprintf(stderr, "Volume size was not specified\n");
		goto out;
	}

	if (name == NULL) {
		fprintf(stderr, "Volume name was not specified\n");
		goto out;
	}

	err = ubi_get_info(lib, &ubi);
	if (err)
		return -1;

	if (devn >= (int)ubi.dev_count) {
		fprintf(stderr, "Device %d does not exist\n", devn);
		goto out;
	}

	len = strlen(name);
	if (len > (int)ubi.nlen_max) {
		fprintf(stderr, "Too long name (%d symbols), max is %d\n",
			len, ubi.nlen_max);
		goto out;
	}

	return 0;

out:
	errno = EINVAL;
	return -1;
}

static void usage(void)
{
	printf("Usage: ubi_mkvol OPTIONS\n"
	       "Version: " PACKAGE_VERSION "\n"
	       "The command line options:\n"
	       "\t-h - this help message\n"
	       "\t-d - UBI device number\n"
	       "\t-t TYPE  - volume type (dynamic, static) "
	       "(default is dynamic)\n"
	       "\t-n VOLID - volume ID to assign to the new volume. If not"
	       "specified, \n"
	       "\t           the volume ID will be assigned automatically\n"
	       "\t-s BYTES - volume size in bytes, "
	       "kilobytes (KiB) or megabytes (MiB)\n"
	       "\t-N NAME  - volume name\n"
	       "\t-a ALIGNMENT - volume alignment (default is 1)\n");
}
