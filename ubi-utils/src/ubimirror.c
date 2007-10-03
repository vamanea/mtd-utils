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
 *
 * Author: Oliver Lohmann
 *
 * 1.2 Removed argp because we want to use uClibc.
 * 1.3 Minor cleanups
 * 1.4 Migrated to new libubi
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <mtd/ubi-header.h>

#include "config.h"
#include "error.h"
#include "example_ubi.h"
#include "ubimirror.h"

#define PROGRAM_VERSION "1.4"

typedef enum action_t {
	ACT_NORMAL = 0,
	ACT_ARGP_ABORT,
	ACT_ARGP_ERR,
} action_t;

#define ABORT_ARGP do {			\
	args->action = ACT_ARGP_ABORT;	\
} while (0)

#define ERR_ARGP do {			\
	args->action = ACT_ARGP_ERR;	\
} while (0)

#define VOL_ARGS_MAX 2

static char doc[] = "\nVersion: " PROGRAM_VERSION "\n"
	"ubimirror - mirrors ubi volumes.\n";

static const char *optionsstr =
"  -c, --copyright            Print copyright information.\n"
"  -s, --side=<seqnum>        Use the side <seqnum> as source.\n"
"  -?, --help                 Give this help list\n"
"      --usage                Give a short usage message\n"
"  -V, --version              Print program version\n";

static const char *usage =
"Usage: ubimirror [-c?V] [-s <seqnum>] [--copyright] [--side=<seqnum>]\n"
"            [--help] [--usage] [--version] <source> <destination>\n";

static const char copyright [] __attribute__((unused)) =
	"(C) IBM Coorporation 2007";

struct option long_options[] = {
	{ .name = "copyright", .has_arg = 0, .flag = NULL, .val = 'c' },
	{ .name = "side", .has_arg = 1, .flag = NULL, .val = 's' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = '?' },
	{ .name = "usage", .has_arg = 0, .flag = NULL, .val = 0 },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

typedef struct myargs {
	action_t action;
	int side;
	int vol_no;			/* index of current volume */
	/* @FIXME replace by bootenv_list, makes live easier */
	/* @FIXME remove the constraint of two entries in the array */
	const char* vol[VOL_ARGS_MAX];	/* comma separated list of src/dst
					   volumes */
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
parse_opt(int argc, char **argv, myargs *args)
{
	while (1) {
		int key;

		key = getopt_long(argc, argv, "cs:?V", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
			case 'c':
				err_msg("%s", copyright);
				ABORT_ARGP;
				break;
			case 's':
				args->side = get_update_side(optarg);
				if (args->side < 0) {
					err_msg("Unsupported seqnum: %s.\n"
						"Supported seqnums are '0' "
						"and '1'\n", optarg);
					ERR_ARGP;
				}
				break;
			case '?': /* help */
				err_msg("Usage: ubimirror [OPTION...] "
					"<source> <destination>\n");
				err_msg("%s", doc);
				err_msg("%s", optionsstr);
				err_msg("\nReport bugs to %s\n",
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

	while (optind < argc) {
		/* only two entries allowed */
		if (args->vol_no >= VOL_ARGS_MAX) {
			err_msg("%s", usage);
			ERR_ARGP;
		}
		args->vol[(args->vol_no)++] = argv[optind++];
	}

	return 0;
}


int
main(int argc, char **argv) {
	int rc = 0;
	unsigned int ids[VOL_ARGS_MAX];
	char err_buf[1024];

	myargs args = {
		.action = ACT_NORMAL,
		.side = -1,
		.vol_no = 0,
		.vol = {"", ""},
		.options = NULL,
	};

	parse_opt(argc, argv, &args);
	if (args.action == ACT_ARGP_ERR) {
		rc = 127;
		goto err;
	}
	if (args.action == ACT_ARGP_ABORT) {
		rc = 126;
		goto out;
	}
	if (args.vol_no < VOL_ARGS_MAX) {
		fprintf(stderr, "missing volume number for %s\n",
			args.vol_no == 0 ? "source and target" : "target");
		rc = 125;
		goto out;
	}
	for( rc = 0; rc < args.vol_no; ++rc){
		char *endp;
		ids[rc] = strtoul(args.vol[rc], &endp, 0);
		if( *endp != '\0' ){
			fprintf(stderr, "invalid volume number %s\n",
					args.vol[rc]);
			rc = 125;
			goto out;
		}
	}
	rc = ubimirror(EXAMPLE_UBI_DEVICE, args.side, ids, args.vol_no,
		       err_buf, sizeof(err_buf));
	if( rc ){
		err_buf[sizeof err_buf - 1] = '\0';
		fprintf(stderr, err_buf);
		if( rc < 0 )
			rc = -rc;
	}
 out:
 err:
	return rc;
}
