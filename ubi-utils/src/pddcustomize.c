/*
 * Copyright (c) International Business Machines Corp., 2008
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
 * Author: Oliver Lohmann
 *
 * PDD (platform description data) contains a set of system specific
 * boot-parameters. Some of those parameters need to be handled
 * special on updates, e.g. the MAC addresses. They must also be kept
 * if the system is updated and one must be able to modify them when
 * the system has booted the first time. This tool is intended to do
 * PDD modification.
 *
 * 1.3 Removed argp because we want to use uClibc.
 * 1.4 Minor cleanups
 * 1.5 Migrated to new libubi
 * 1.6 Fixed broken volume update
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <mtd/ubi-header.h>

#include "config.h"
#include "bootenv.h"
#include "error.h"
#include "example_ubi.h"
#include "libubi.h"
#include "ubimirror.h"

#define PROGRAM_VERSION "1.6"

#define DEFAULT_DEV_PATTERN    "/dev/ubi%d"
#define DEFAULT_VOL_PATTERN    "/dev/ubi%d_%d"

typedef enum action_t {
	ACT_NORMAL   = 0,
	ACT_LIST,
	ACT_ARGP_ABORT,
	ACT_ARGP_ERR,
} action_t;

#define ABORT_ARGP do {			\
	args->action = ACT_ARGP_ABORT;	\
} while (0)

#define ERR_ARGP do {			\
	args->action = ACT_ARGP_ERR;	\
} while (0)

static char doc[] = "\nVersion: " PROGRAM_VERSION "\n"
	"pddcustomize - customize bootenv and pdd values.\n";

static const char *optionsstr =
"  -b, --both                 Mirror updated PDD to redundand copy.\n"
"  -c, --copyright            Print copyright information.\n"
"  -i, --input=<input>        Binary input file. For debug purposes.\n"
"  -l, --list                 List card bootenv/pdd values.\n"
"  -o, --output=<output>      Binary output file. For debug purposes.\n"
"  -s, --side=<seqnum>        The side/seqnum to update.\n"
"  -x, --host                 use x86 platform for debugging.\n"
"  -?, --help                 Give this help list\n"
"      --usage                Give a short usage message\n"
"  -V, --version              Print program version\n";

static const char *usage =
"Usage: pddcustomize [-bclx?V] [-i <input>] [-o <output>] [-s <seqnum>]\n"
"           [--both] [--copyright] [--input=<input>] [--list]\n"
"           [--output=<output>] [--side=<seqnum>] [--host] [--help] [--usage]\n"
"           [--version] [key=value] [...]\n";

struct option long_options[] = {
	{ .name = "both", .has_arg = 0, .flag = NULL, .val = 'b' },
	{ .name = "copyright", .has_arg = 0, .flag = NULL, .val = 'c' },
	{ .name = "input", .has_arg = 1, .flag = NULL, .val = 'i' },
	{ .name = "list", .has_arg = 0, .flag = NULL, .val = 'l' },
	{ .name = "output", .has_arg = 1, .flag = NULL, .val = 'o' },
	{ .name = "side", .has_arg = 1, .flag = NULL, .val = 's' },
	{ .name = "host", .has_arg = 0, .flag = NULL, .val = 'x' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = '?' },
	{ .name = "usage", .has_arg = 0, .flag = NULL, .val = 0 },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

static const char copyright [] __attribute__((unused)) =
	"Copyright IBM Corp 2006";

typedef struct myargs {
	action_t action;
	const char* file_in;
	const char* file_out;
	int both;
	int side;
	int x86;		/* X86 host, use files for testing */
	bootenv_t env_in;

	char *arg1;
	char **options;		/* [STRING...] */
} myargs;

static int
get_update_side(const char* str)
{
	uint32_t i = strtoul(str, NULL, 0);

	if ((i != 0) && (i != 1)) {
		return -1;
	}

	return i;
}

static int
extract_pair(bootenv_t env, const char* str)
{
	int rc = 0;
	char* key;
	char* val;

	key = strdup(str);
	if (key == NULL)
		return -ENOMEM;

	val = strstr(key, "=");
	if (val == NULL) {
		err_msg("Wrong argument: %s\n"
			"Expecting key=value pair.\n", str);
		rc = -1;
		goto err;
	}

	*val = '\0'; /* split strings */
	val++;
	rc = bootenv_set(env, key, val);

err:
	free(key);
	return rc;
}

static int
parse_opt(int argc, char **argv, myargs *args)
{
	int rc = 0;

	while (1) {
		int key;

		key = getopt_long(argc, argv, "clbxs:i:o:?V",
				  long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
			case 'c':
				err_msg("%s\n", copyright);
				ABORT_ARGP;
				break;
			case 'l':
				args->action = ACT_LIST;
				break;
			case 'b':
				args->both = 1;
				break;
			case 'x':
				args->x86 = 1;
				break;
			case 's':
				args->side = get_update_side(optarg);
				if (args->side < 0) {
					err_msg("Unsupported seqnum: %d.\n"
						"Supported seqnums are "
						"'0' and '1'\n",
						args->side, optarg);
					ERR_ARGP;
				}
				break;
			case 'i':
				args->file_in = optarg;
				break;
			case 'o':
				args->file_out = optarg;
				break;
			case '?': /* help */
				err_msg("Usage: pddcustomize [OPTION...] "
					"[key=value] [...]");
				err_msg("%s", doc);
				err_msg("%s", optionsstr);
				err_msg("\nReport bugs to %s",
					PACKAGE_BUGREPORT);
				exit(0);
				break;
			case 'V':
				err_msg("%s", PROGRAM_VERSION);
				exit(0);
				break;
			default:
				err_msg("%s", usage);
				exit(-1);
		}
	}

	if (optind < argc) {
		rc = extract_pair(args->env_in, argv[optind++]);
		if (rc != 0)
			ERR_ARGP;
	}

	return 0;
}

static int
list_bootenv(bootenv_t env)
{
	int rc = 0;
	rc = bootenv_write_txt(stdout, env);
	if (rc != 0) {
		err_msg("Cannot list bootenv/pdd. rc: %d\n", rc);
		goto err;
	}
err:
	return rc;
}

static int
process_key_value(bootenv_t env_in, bootenv_t env)
{
	int rc = 0;
	size_t size, i;
	const char* tmp;
	const char** key_vec = NULL;

	rc = bootenv_get_key_vector(env_in, &size, 0, &key_vec);
	if (rc != 0)
		goto err;

	for (i = 0; i < size; i++) {
		rc = bootenv_get(env_in, key_vec[i], &tmp);
		if (rc != 0) {
			err_msg("Cannot read value to input key: %s. rc: %d\n",
					key_vec[i], rc);
			goto err;
		}
		rc = bootenv_set(env, key_vec[i], tmp);
		if (rc != 0) {
			err_msg("Cannot set value key: %s. rc: %d\n",
					key_vec[i], rc);
			goto err;
		}
	}

err:
	if (key_vec != NULL)
		free(key_vec);
	return rc;
}

static int
read_bootenv(const char* file, bootenv_t env)
{
	int rc = 0;
	FILE* fp_in = NULL;

	fp_in = fopen(file, "rb");
	if (fp_in == NULL) {
		err_msg("Cannot open file: %s\n", file);
		return -EIO;
	}

	rc = bootenv_read(fp_in, env, BOOTENV_MAXSIZE);
	if (rc != 0) {
		err_msg("Cannot read bootenv from file %s. rc: %d\n",
			file, rc);
		goto err;
	}

err:
	fclose(fp_in);
	return rc;
}

/*
 * Read bootenv from ubi volume
 */
static int
ubi_read_bootenv(uint32_t devno, uint32_t id, bootenv_t env)
{
	libubi_t ulib;
	int rc = 0;
	char path[PATH_MAX];
	FILE* fp_in = NULL;

	ulib = libubi_open();
	if (ulib == NULL) {
		err_msg("Cannot allocate ubi structure\n");
		return -1;
	}

	snprintf(path, PATH_MAX, DEFAULT_VOL_PATTERN, devno, id);

	fp_in = fopen(path, "r");
	if (fp_in == NULL) {
		err_msg("Cannot open volume:%d number:%d\n", devno, id);
		goto err;
	}

	rc = bootenv_read(fp_in, env, BOOTENV_MAXSIZE);
	if (rc != 0) {
		err_msg("Cannot read volume:%d number:%d\n", devno, id);
		goto err;
	}

err:
	if (fp_in)
		fclose(fp_in);
	libubi_close(ulib);
	return rc;
}

static int
write_bootenv(const char* file, bootenv_t env)
{
	int rc = 0;
	FILE* fp_out;

	fp_out = fopen(file, "wb");
	if (fp_out == NULL) {
		err_msg("Cannot open file: %s\n", file);
		return -EIO;
	}

	rc = bootenv_write(fp_out, env);
	if (rc != 0) {
		err_msg("Cannot write bootenv to file %s. rc: %d\n", file, rc);
		goto err;
	}

err:
	fclose(fp_out);
	return rc;
}

/*
 * Read bootenv from ubi volume
 */
static int
ubi_write_bootenv(uint32_t devno, uint32_t id, bootenv_t env)
{
	libubi_t ulib;
	int rc = 0;
	char path[PATH_MAX];
	FILE* fp_out = NULL;
	size_t nbytes;

	rc = bootenv_size(env, &nbytes);
	if (rc) {
		err_msg("Cannot determine size of bootenv structure\n");
		return rc;
	}
	ulib = libubi_open();
	if (ulib == NULL) {
		err_msg("Cannot allocate ubi structure\n");
		return rc;
	}

	snprintf(path, PATH_MAX, DEFAULT_VOL_PATTERN, devno, id);

	fp_out = fopen(path, "r+");
	if (fp_out == NULL) {
		err_msg("Cannot fopen volume:%d number:%d\n", devno, id);
		rc = -EBADF;
		goto err;
	}

	rc = ubi_update_start(ulib, fileno(fp_out), nbytes);
	if (rc != 0) {
		err_msg("Cannot start update for %s\n", path);
		goto err;
	}

	rc = bootenv_write(fp_out, env);
	if (rc != 0) {
		err_msg("Cannot write bootenv to volume %d number:%d\n",
			devno, id);
		goto err;
	}
err:
	if( fp_out )
		fclose(fp_out);
	libubi_close(ulib);
	return rc;
}

static int
do_mirror(int volno)
{
	char errbuf[1024];
	uint32_t ids[2];
	int rc;
	int src_volno_idx = 0;

	ids[0] = EXAMPLE_BOOTENV_VOL_ID_1;
	ids[1] = EXAMPLE_BOOTENV_VOL_ID_2;

	if (volno == EXAMPLE_BOOTENV_VOL_ID_2)
		src_volno_idx = 1;

	rc = ubimirror(EXAMPLE_UBI_DEVICE, src_volno_idx, ids, 2, errbuf,
		       sizeof errbuf);
	if( rc )
		err_msg(errbuf);
	return rc;
}

int
main(int argc, char **argv) {
	int rc = 0;
	bootenv_t env = NULL;
	uint32_t boot_volno;
	myargs args = {
		.action = ACT_NORMAL,
		.file_in  = NULL,
		.file_out = NULL,
		.side = -1,
		.x86 = 0,
		.both = 0,
		.env_in = NULL,

		.arg1 = NULL,
		.options = NULL,
	};

	rc = bootenv_create(&env);
	if (rc != 0) {
		err_msg("Cannot create bootenv handle. rc: %d", rc);
		goto err;
	}

	rc = bootenv_create(&(args.env_in));
	if (rc != 0) {
		err_msg("Cannot create bootenv handle. rc: %d", rc);
		goto err;
	}

	parse_opt(argc, argv, &args);
	if (args.action == ACT_ARGP_ERR) {
		rc = -1;
		goto err;
	}
	if (args.action == ACT_ARGP_ABORT) {
		rc = 0;
		goto out;
	}

	if ((args.side == 0) || (args.side == -1))
		boot_volno = EXAMPLE_BOOTENV_VOL_ID_1;
	else
		boot_volno = EXAMPLE_BOOTENV_VOL_ID_2;

	if( args.x86 )
		rc = read_bootenv(args.file_in, env);
	else
		rc = ubi_read_bootenv(EXAMPLE_UBI_DEVICE, boot_volno, env);
	if (rc != 0) {
		goto err;
	}

	if (args.action == ACT_LIST) {
		rc = list_bootenv(env);
		if (rc != 0) {
			goto err;
		}
		goto out;
	}

	rc = process_key_value(args.env_in, env);
	if (rc != 0) {
		goto err;
	}

	if( args.x86 )
		rc = write_bootenv(args.file_in, env);
	else
		rc = ubi_write_bootenv(EXAMPLE_UBI_DEVICE, boot_volno, env);
	if (rc != 0)
		goto err;

	if( args.both )		/* No side specified, update both */
		rc = do_mirror(boot_volno);

 out:
 err:
	bootenv_destroy(&env);
	bootenv_destroy(&(args.env_in));
	return rc;
}
