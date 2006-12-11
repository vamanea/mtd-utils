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
 * Author: Artem B. Bityutskiy <dedekind@linutronix.de>
 *         Frank Haverkamp <haver@vnet.ibm.com>
 *
 * 1.0 Initial release
 * 1.1 Does not support erase blocks anymore. This is replaced by
 *     the number of bytes.
 * 1.2 Reworked the user-interface to use argp.
 */

#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <config.h>
#include <libubi.h>

#define PROGRAM_VERSION "1.2"

extern char *optarg;
extern int optind;

/*
 * The variables below	are set by command line arguments.
 */
struct args {
	int devn;
	int vol_id;
	int vol_type;
	long long bytes;
	int alignment;
	char *name;
	int nlen;

	/* special stuff needed to get additional arguments */
	char *arg1;
	char **options;		/* [STRING...] */
};

static struct args myargs = {
	.vol_type = UBI_DYNAMIC_VOLUME,
	.devn = -1,
	.bytes = 0,
	.alignment = 1,
	.vol_id = UBI_VOL_NUM_AUTO,
	.name = NULL,
	.nlen = 0,
};

static int param_sanity_check(struct args *args, ubi_lib_t lib);

static char doc[] = "\nVersion: " PROGRAM_VERSION "\n\t"
	BUILD_OS" "BUILD_CPU" at "__DATE__" "__TIME__"\n"
	"\nMake UBI Volume.\n";

static const char *optionsstr =
"  -a, --alignment=<alignment>   volume alignment (default is 1)\n"
"  -d, --devn=<devn>          UBI device\n"
"  -n, --vol_id=<volume id>   UBI volume id, if not specified, the volume ID\n"
"                             will be assigned automatically\n"
"  -N, --name=<name>          volume name\n"
"  -s, --size=<bytes>         volume size volume size in bytes, kilobytes (KiB)\n"
"                             or megabytes (MiB)\n"
"  -t, --type=<static|dynamic>   volume type (dynamic, static), default is\n"
"                             dynamic\n"
"  -?, --help                 Give this help list\n"
"      --usage                Give a short usage message\n"
"  -V, --version              Print program version\n";

static const char *usage =
"Usage: ubimkvol [-?V] [-a <alignment>] [-d <devn>] [-n <volume id>]\n"
"            [-N <name>] [-s <bytes>] [-t <static|dynamic>]\n"
"            [--alignment=<alignment>] [--devn=<devn>] [--vol_id=<volume id>]\n"
"            [--name=<name>] [--size=<bytes>] [--type=<static|dynamic>] [--help]\n"
"            [--usage] [--version]\n";

struct option long_options[] = {
	{ .name = "alignment", .has_arg = 1, .flag = NULL, .val = 'a' },
	{ .name = "devn", .has_arg = 1, .flag = NULL, .val = 'd' },
	{ .name = "vol_id", .has_arg = 1, .flag = NULL, .val = 'n' },
	{ .name = "name", .has_arg = 1, .flag = NULL, .val = 'N' },
	{ .name = "size", .has_arg = 1, .flag = NULL, .val = 's' },
	{ .name = "type", .has_arg = 1, .flag = NULL, .val = 't' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = '?' },
	{ .name = "usage", .has_arg = 0, .flag = NULL, .val = 0 },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

/*
 * @brief Parse the arguments passed into the test case.
 *
 * @param key		 The parameter.
 * @param arg		 Argument passed to parameter.
 * @param state		 Location to put information on parameters.
 *
 * @return error
 *
 * Get the `input' argument from `argp_parse', which we know is a
 * pointer to our arguments structure.
 */
static int
parse_opt(int argc, char **argv, struct args *args)
{
	char *endp;

	while (1) {
		int key;

		key = getopt_long(argc, argv, "a:d:n:N:s:t:?V", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
			case 't':
				if (!strcmp(optarg, "dynamic"))
					args->vol_type = UBI_DYNAMIC_VOLUME;
				else if (!strcmp(optarg, "static"))
					args->vol_type = UBI_STATIC_VOLUME;
				else {
					fprintf(stderr, "Bad volume type: \"%s\"\n",
							optarg);
					goto out;
				}
				break;
			case 's':
				args->bytes = strtoull(optarg, &endp, 0);
				if (endp == optarg || args->bytes < 0) {
					fprintf(stderr, "Bad volume size: \"%s\"\n",
							optarg);
					goto out;
				}
				if (endp != '\0') {
					if (strcmp(endp, "KiB") == 0)
						args->bytes *= 1024;
					else if (strcmp(endp, "MiB") == 0)
						args->bytes *= 1024*1024;
				}
				break;
			case 'a':
				args->alignment = strtoul(optarg, &endp, 0);
				if (*endp != '\0' || endp == optarg ||
						args->alignment <= 0) {
					fprintf(stderr, "Bad volume alignment: "
							"\"%s\"\n", optarg);
					goto out;
				}
				break;
			case 'd': /* --devn=<device number> */
				args->devn = strtoul(optarg, &endp, 0);
				if (*endp != '\0' || endp == optarg || args->devn < 0) {
					fprintf(stderr, "Bad UBI device number: "
							"\"%s\"\n", optarg);
					goto out;
				}
				break;
			case 'n': /* --volid=<volume id> */
				args->vol_id = strtoul(optarg, &endp, 0);
				if (*endp != '\0' || endp == optarg ||
						(args->vol_id < 0 && args->vol_id != UBI_DYNAMIC_VOLUME)) {
					fprintf(stderr, "Bad volume ID: "
							"\"%s\"\n", optarg);
					goto out;
				}
				break;
			case 'N':
				args->name = optarg;
				args->nlen = strlen(args->name);
				break;

			case ':':
				fprintf(stderr, "Parameter is missing\n");
				goto out;

			case '?': /* help */
				fprintf(stderr, "Usage: ubimkvol [OPTION...]\n");
				fprintf(stderr, "%s", doc);
				fprintf(stderr, "%s", optionsstr);
				fprintf(stderr, "\nReport bugs to %s\n", PACKAGE_BUGREPORT);
				exit(0);
				break;

			case 'V':
				fprintf(stderr, "%s\n", PACKAGE_VERSION);
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
	int err, len;
	struct ubi_info ubi;

	if (args->bytes == 0) {
		fprintf(stderr, "Volume size was not specified\n");
		goto out;
	}

	if (args->name == NULL) {
		fprintf(stderr, "Volume name was not specified\n");
		goto out;
	}

	err = ubi_get_info(lib, &ubi);
	if (err)
		return -1;

	if (args->devn >= (int)ubi.dev_count) {
		fprintf(stderr, "Device %d does not exist\n", args->devn);
		goto out;
	}

	len = strlen(args->name);
	if (len > UBI_MAX_VOLUME_NAME) {
		fprintf(stderr, "Too long name (%d symbols), max is %d\n",
			len, UBI_MAX_VOLUME_NAME);
		goto out;
	}

	return 0;
out:
	errno = EINVAL;
	return -1;
}

int main(int argc, char * const argv[])
{
	int err;
	ubi_lib_t lib;

	err = parse_opt(argc, (char **)argv, &myargs);
	if (err) {
		fprintf(stderr, "Wrong options ...\n");
		return err == 1 ? 0 : -1;
	}

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

	err = ubi_mkvol(lib, myargs.devn, myargs.vol_id, myargs.vol_type,
			myargs.bytes, myargs.alignment, myargs.name);
	if (err < 0) {
		perror("Cannot create volume");
		fprintf(stderr, "  err=%d\n", err);
		goto out_libubi;
	}

	/* printf("Created volume %d, %lld bytes, type %s, name %s\n",
	   vol_id, bytes, vol_type == UBI_DYNAMIC_VOLUME ?
	   "dynamic" : "static", name); */

	myargs.vol_id = err;
	ubi_close(&lib);
	return 0;

out_libubi:
	ubi_close(&lib);
	return -1;
}
