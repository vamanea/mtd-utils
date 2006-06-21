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
 *         Frank Haverkamp
 *
 * Process a PFI (partial flash image) and write the data to the
 * specified UBI volumes. This tool is intended to be used for system
 * update using PFI files.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <argp.h>
#include <unistd.h>
#include <errno.h>

#include <pfiflash.h>
#include "error.h"
#include "config.h"

const char *argp_program_version = PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;
static char doc[] = "\nVersion: " PACKAGE_VERSION "\n\tBuilt on "
	BUILD_CPU" "BUILD_OS" at "__DATE__" "__TIME__"\n"
	"\n"
	"pfiflash - a tool for updating a controller with PFI files.\n";

static const char copyright [] __attribute__((unused)) =
	"FIXME: insert license type."; /* FIXME */

static struct argp_option options[] = {
	/* Output options */
	{ name: NULL, key: 0, arg: NULL, flags: 0,
	  doc: "Standard options:",
	  group: 1 },

	{ name: "copyright", key: 'c', arg: NULL, flags: 0,
	  doc: "Print copyright information.",
	  group: 1 },

	{ name: "verbose", key: 'v', arg: NULL, flags: 0,
	  doc: "Be verbose during program execution.",
	  group: 1 },

	{ name: "logfile", key: 'l', arg: "<file>", flags: 0,
	  doc: "Write a logfile to <file>.",
	  group: 1 },

	/* Output options */
	{ name: NULL, key: 0, arg: NULL, flags: 0,
	  doc: "Process options:",
	  group: 2 },

	{ name: "complete", key: 'C', arg: NULL, flags: 0,
	  doc: "Execute a complete system update. Updates both sides.",
	  group: 2 },

	{ name: "side", key: 's', arg: "<seqnum>", flags: 0,
	  doc: "Select the side which shall be updated.",
	  group: 2 },

	{ name: "pdd-update", key: 'p', arg: "<type>", flags: 0,
	  doc: "Specify the pdd-update algorithm. <type> is either "
	  "'keep', 'merge' or 'overwrite'.",
	  group: 2 },

	{ name: NULL, key: 0, arg: NULL, flags: 0, doc: NULL, group: 0 },
};

typedef struct myargs {
	int verbose;
	const char *logfile;

	pdd_handling_t pdd_handling;
	int seqnum;
	int complete;

	FILE* fp_in;

	/* special stuff needed to get additional arguments */
	char *arg1;
	char **options;		/* [STRING...] */
} myargs;

static pdd_handling_t
get_pdd_handling(const char* str)
{
	if (strcmp(str, "keep") == 0) {
		return PDD_KEEP;
	}
	if (strcmp(str, "merge") == 0) {
		return PDD_MERGE;
	}
	if (strcmp(str, "overwrite") == 0) {
		return PDD_OVERWRITE;
	}

	return -1;
}

static int
get_update_seqnum(const char* str)
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
		/* standard options */
	case 'c':
		err_msg("%s\n", copyright);
		exit(0);
		break;
	case 'v':
		args->verbose = 1;
		break;
	case 'l':
		args->logfile = arg;
		break;
		/* process options */
	case 'C':
		args->complete = 1;
		break;
	case 'p':
		args->pdd_handling = get_pdd_handling(arg);
		if ((int)args->pdd_handling < 0) {
			err_quit("Unknown PDD handling: %s.\n"
				 "Please use either 'keep', 'merge' or"
				 "'overwrite'.\n'");
		}
		break;
	case 's':
		args->seqnum = get_update_seqnum(arg);
		if (args->seqnum < 0) {
			err_quit("Unsupported side: %s.\n"
				 "Supported sides are '0' and '1'\n", arg);
		}
		break;

	case ARGP_KEY_ARG: /* input file */
		args->fp_in = fopen(arg, "r");
		if ((args->fp_in) == NULL) {
			err_sys("Cannot open PFI file %s for input", arg);
		}
		args->arg1 = arg;
		args->options = &state->argv[state->next];
		state->next = state->argc;
		break;
	case ARGP_KEY_END:
		if (err) {
			err_msg("\n");
			argp_usage(state);
			exit(1);
		}
		break;
	default:
		return(ARGP_ERR_UNKNOWN);
	}

	return 0;
}

static struct argp argp = {
	options:     options,
	parser:      parse_opt,
	args_doc:    "[pfifile]",
	doc:	     doc,
	children:    NULL,
	help_filter: NULL,
	argp_domain: NULL,
};

int main (int argc, char** argv)
{
	int rc = 0;
	char err_buf[PFIFLASH_MAX_ERR_BUF_SIZE];
	memset(err_buf, '\0', PFIFLASH_MAX_ERR_BUF_SIZE);

	myargs args = {
		.verbose    = 0,
		.seqnum	    = -1,
		.complete   = 0,
		.logfile    = "/tmp/pfiflash.log",
		.pdd_handling = PDD_KEEP,
		.fp_in	  = stdin,
	};

	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &args);
	error_initlog(args.logfile);

	if (!args.fp_in) {
		rc = -1;
		snprintf(err_buf, PFIFLASH_MAX_ERR_BUF_SIZE,
			 "No PFI input file specified!\n");
		goto err;
	}

	rc = pfiflash(args.fp_in, args.complete, args.seqnum,
		      args.pdd_handling, err_buf, PFIFLASH_MAX_ERR_BUF_SIZE);
	if (rc != 0) {
		goto err_fp;
	}

 err_fp:
	if (args.fp_in != stdin)
		fclose(args.fp_in);
 err:
	if (rc != 0)
		err_msg("Error: %s\nrc: %d\n", err_buf, rc);
	return rc;
}
