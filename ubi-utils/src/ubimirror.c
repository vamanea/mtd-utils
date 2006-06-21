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
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <argp.h>
#include <unistd.h>
#include <errno.h>
#include <mtd/ubi-header.h>

#include "config.h"
#include "error.h"
#include "example_ubi.h"
#include "ubimirror.h"

typedef enum action_t {
	ACT_NORMAL = 0,
	ACT_ARGP_ABORT,
	ACT_ARGP_ERR,
} action_t;

#define ABORT_ARGP do {			\
	state->next = state->argc;	\
	args->action = ACT_ARGP_ABORT;	\
} while (0)

#define ERR_ARGP do {			\
	state->next = state->argc;	\
	args->action = ACT_ARGP_ERR;	\
} while (0)

#define VOL_ARGS_MAX 2


const char *argp_program_version = PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;
static char doc[] = "\nVersion: " PACKAGE_VERSION "\n\tBuilt on "
	BUILD_CPU" "BUILD_OS" at "__DATE__" "__TIME__"\n"
	"\n"
	"ubimirror - mirrors ubi volumes.\n";

static const char copyright [] __attribute__((unused)) =
	"(C) IBM Coorporation 2007";


static struct argp_option options[] = {
	{ name: "copyright", key: 'c', arg: NULL,    flags: 0,
	  doc: "Print copyright information.",
	  group: 1 },

	{ name: "side", key: 's', arg: "<seqnum>",    flags: 0,
	  doc: "Use the side <seqnum> as source.",
	  group: 1 },

	{ name: NULL, key: 0, arg: NULL, flags: 0, doc: NULL, group: 0 },
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


static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
	int err = 0;

	myargs *args = state->input;

	switch (key) {
	case 'c':
		err_msg("%s\n", copyright);
		ABORT_ARGP;
		break;
	case 's':
		args->side = get_update_side(arg);
		if (args->side < 0) {
			err_msg("Unsupported seqnum: %s.\n"
				 "Supported seqnums are '0' and '1'\n", arg);
			ERR_ARGP;
		}
		break;
	case ARGP_KEY_ARG:
		/* only two entries allowed */
		if (args->vol_no >= VOL_ARGS_MAX) {
			err_msg("\n");
			argp_usage(state);
			ERR_ARGP;
		}
		args->vol[(args->vol_no)++] = arg;
		break;
	case ARGP_KEY_END:
		if (err) {
			err_msg("\n");
			argp_usage(state);
			ERR_ARGP;
		}
		break;
	default:
		return(ARGP_ERR_UNKNOWN);
	}

	return 0;
}

static struct argp argp = {
	options:     options,
	parser:	     parse_opt,
	args_doc:    "<source> <destination>",
	doc:	     doc,
	children:    NULL,
	help_filter: NULL,
	argp_domain: NULL,
};


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

	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &args);
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
