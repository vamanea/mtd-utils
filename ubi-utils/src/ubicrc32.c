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
 * Calculate CRC32 with UBI start value for a given binary image.
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
#include "crc32.h"

#define BUFSIZE 4096

const char *argp_program_version = PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;
static char doc[] = "\nVersion: " PACKAGE_VERSION "\n"
	"ubicrc32 - calculates the UBI CRC32 value and prints it to stdout.\n";

static const char copyright [] __attribute__((unused)) =
	"FIXME: insert license type"; /* FIXME */


static struct argp_option options[] = {
	{ name: "copyright", key: 'c', arg: NULL,    flags: 0,
	  doc: "Print copyright information.",
	  group: 1 },

	{ name: NULL, key: 0, arg: NULL, flags: 0, doc: NULL, group: 0 },
};

typedef struct myargs {
	FILE* fp_in;

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
	options:     options,
	parser:	     parse_opt,
	args_doc:    "[file]",
	doc:	     doc,
	children:    NULL,
	help_filter: NULL,
	argp_domain: NULL,
};

int
main(int argc, char **argv) {
	int rc = 0;
	uint32_t crc32_table[256];
	uint8_t buf[BUFSIZE];
	size_t read;
	uint32_t crc32;

	myargs args = {
		.fp_in = stdin,
		.arg1 = NULL,
		.options = NULL,
	};

	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &args);

	init_crc32_table(crc32_table);
	crc32 = UBI_CRC32_INIT;
	while (!feof(args.fp_in)) {
		read = fread(buf, 1, BUFSIZE, args.fp_in);
		if (ferror(args.fp_in)) {
			fprintf(stderr, "I/O Error.");
			exit(EXIT_FAILURE);
		}
		crc32 = clc_crc32(crc32_table, crc32, buf, read);
	}

	if (args.fp_in != stdin) {
		fclose(args.fp_in);
	}

	fprintf(stdout, "0x%08x\n", crc32);
	return rc;
}
