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
 * Create boot-parameter/pdd data from an ASCII-text input file.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <argp.h>
#include <unistd.h>
#include <errno.h>
#include <mtd/ubi-header.h>

#include "config.h"
#include "bootenv.h"
#include "error.h"

const char *argp_program_version = PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;
static char doc[] = "\nVersion: " PACKAGE_VERSION "\n\tBuilt on "
	BUILD_CPU" "BUILD_OS" at "__DATE__" "__TIME__"\n"
	"\n"
	"mkbootenv - processes bootenv text files and convertes "
	"them into a binary format.\n";

static const char copyright [] __attribute__((unused)) =
	"Copyright (c) International Business Machines Corp., 2006";

static struct argp_option options[] = {
	{ .name = "copyright",
	  .key = 'c',
	  .arg = NULL,
	  .flags = 0,
	  .doc = "Print copyright information.",
	  .group = 1 },
	{ .name = "output",
	  .key = 'o',
	  .arg = "<output>",
	  .flags = 0,
	  .doc = "Write the the output data to <output> instead of stdout.",
	  .group = 1 },
	{ .name = NULL,
	  .key = 0,
	  .arg = NULL,
	  .flags = 0,
	  .doc = NULL,
	  .group = 0 },
};

typedef struct myargs {
	FILE* fp_in;
	FILE* fp_out;

	char *arg1;
	char **options;			/* [STRING...] */
} myargs;



static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
	int err = 0;

	myargs *args = state->input;

	switch (key) {
	case 'c':
		fprintf(stderr, "%s\n", copyright);
		exit(0);
		break;
	case 'o':
		args->fp_out = fopen(arg, "wb");
		if ((args->fp_out) == NULL) {
			fprintf(stderr,
			"Cannot open file %s for output\n", arg);
			exit(1);
		}
		break;
	case ARGP_KEY_ARG:
		args->fp_in = fopen(arg, "rb");
		if ((args->fp_in) == NULL) {
			fprintf(stderr,
			"Cannot open file %s for input\n", arg);
			exit(1);
		}
		args->arg1 = arg;
		args->options = &state->argv[state->next];
		state->next = state->argc;
		break;
	case ARGP_KEY_END:
		if (err) {
			fprintf(stderr, "\n");
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
	.options     = options,
	.parser	     = parse_opt,
	.args_doc    = "[bootenv-txt-file]",
	.doc	     = doc,
	.children    = NULL,
	.help_filter = NULL,
	.argp_domain = NULL,
};

int
main(int argc, char **argv) {
	int rc = 0;
	bootenv_t env;

	myargs args = {
		.fp_in = stdin,
		.fp_out = stdout,
		.arg1 = NULL,
		.options = NULL,
	};

	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &args);

	rc = bootenv_create(&env);
	if (rc != 0) {
		err_msg("Cannot create bootenv handle.");
		goto err;
	}
	rc = bootenv_read_txt(args.fp_in, env);
	if (rc != 0) {
		err_msg("Cannot read bootenv from input file.");
		goto err;
	}
	rc = bootenv_write(args.fp_out, env);
	if (rc != 0) {
		err_msg("Cannot write bootenv to output file.");
		goto err;
	}

	if (args.fp_in != stdin) {
		fclose(args.fp_in);
	}
	if (args.fp_out != stdout) {
		fclose(args.fp_out);
	}

err:
	bootenv_destroy(&env);
	return rc;
}
