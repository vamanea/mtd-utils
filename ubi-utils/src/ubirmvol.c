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
 * Author: Artem B. Bityutskiy <dedekind@linutronix.de>
 *         Frank Haverkamp <haver@vnet.ibm.com>
 *
 * 1.1 Reworked the userinterface to use argp.
 * 1.2 Removed argp because we want to use uClibc.
 * 1.3 Minor cleanups
 */

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <config.h>
#include <libubi.h>

#define PROGRAM_VERSION "1.3"

/*
 * The below variables are set by command line options.
 */
struct args {
	int devn;
	int vol_id;

	/* special stuff needed to get additional arguments */
	char *arg1;
	char **options;		/* [STRING...] */
};

static struct args myargs = {
	.devn = -1,
	.vol_id = -1,

	.arg1 = NULL,
	.options = NULL,
};

static int param_sanity_check(struct args *args, ubi_lib_t lib);

static char doc[] = "\nVersion: " PROGRAM_VERSION "\n\t"
	BUILD_OS" "BUILD_CPU" at "__DATE__" "__TIME__"\n"
	"\nMake UBI Volume.\n";

static const char *optionsstr =
"  -d, --devn=<devn>          UBI device\n"
"  -n, --vol_id=<volume id>   UBI volume id, if not specified, the volume ID\n"
"                             will be assigned automatically\n"
"  -?, --help                 Give this help list\n"
"      --usage                Give a short usage message\n"
"  -V, --version              Print program version\n";

static const char *usage =
"Usage: ubirmvol [-?V] [-d <devn>] [-n <volume id>] [--devn=<devn>]\n"
"            [--vol_id=<volume id>] [--help] [--usage] [--version]\n";

struct option long_options[] = {
	{ .name = "devn", .has_arg = 1, .flag = NULL, .val = 'd' },
	{ .name = "vol_id", .has_arg = 1, .flag = NULL, .val = 'n' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = '?' },
	{ .name = "usage", .has_arg = 0, .flag = NULL, .val = 0 },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

/*
 * @brief Parse the arguments passed into the test case.
 *
 * @param argc		 The number of arguments
 * @param argv		 The list of arguments
 * @param args		 Pointer to argument structure
 *
 * @return error
 *
 */
static int
parse_opt(int argc, char **argv, struct args *args)
{
	char *endp;

	while (1) {
		int key;

		key = getopt_long(argc, argv, "d:n:?V", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
			case 'd': /* --devn=<device number> */
				args->devn = strtoul(optarg, &endp, 0);
				if (*endp != '\0' || endp == optarg ||
					args->devn < 0) {
					fprintf(stderr,
						"Bad UBI device number: "
						"\"%s\"\n", optarg);
					goto out;
				}
				break;
			case 'n': /* --volid=<volume id> */
				args->vol_id = strtoul(optarg, &endp, 0);
				if (*endp != '\0' || endp == optarg ||
				    (args->vol_id < 0 &&
				     args->vol_id != UBI_DYNAMIC_VOLUME)) {
					fprintf(stderr, "Bad volume ID: "
							"\"%s\"\n", optarg);
					goto out;
				}
				break;
			case ':':
				fprintf(stderr, "Parameter is missing\n");
				goto out;
			case '?': /* help */
				fprintf(stderr,
					"Usage: ubirmvol [OPTION...]\n");
				fprintf(stderr, "%s", doc);
				fprintf(stderr, "%s", optionsstr);
				fprintf(stderr, "\nReport bugs to %s\n",
					PACKAGE_BUGREPORT);
				exit(0);
				break;
			case 'V':
				fprintf(stderr, "%s\n", PROGRAM_VERSION);
				exit(0);
				break;
			default:
				fprintf(stderr, "%s", usage);
				exit(-1);
		}
	}

	return 0;
 out:
	return -1;
}

static int param_sanity_check(struct args *args, ubi_lib_t lib)
{
	int err;
	struct ubi_info ubi;

	if (args->vol_id == -1) {
		fprintf(stderr, "Volume ID was not specified\n");
		goto out;
	}

	err = ubi_get_info(lib, &ubi);
	if (err)
		return -1;

	if (args->devn >= (int)ubi.dev_count) {
		fprintf(stderr, "Device %d does not exist\n", args->devn);
		goto out;
	}

	return 0;

out:
	errno = EINVAL;
	return -1;
}

int main(int argc, char * const argv[])
{
	int err, old_errno;
	ubi_lib_t lib;

	err = parse_opt(argc, (char **)argv, &myargs);
	if (err)
		return err == 1 ? 0 : -1;

	if (myargs.devn == -1) {
		fprintf(stderr, "Device number was not specified\n");
		fprintf(stderr, "Use -h option for help\n");
		return -1;
	}

	err = ubi_open(&lib);
	if (err) {
		perror("Cannot open libubi");
		return -1;
	}

	err = param_sanity_check(&myargs, lib);
	if (err) {
		perror("Input parameters check");
		fprintf(stderr, "Use -h option for help\n");
		goto out_libubi;
	}

	err = ubi_rmvol(lib, myargs.devn, myargs.vol_id);
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
