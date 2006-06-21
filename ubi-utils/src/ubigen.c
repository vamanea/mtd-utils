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
 * Tool to add UBI headers to binary images.
 *
 * 1.0 Initial version
 * 1.1 Different CRC32 start value
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <argp.h>
#include <unistd.h>
#include <sys/stat.h>
#include <mtd/ubi-header.h>

#include "ubigen.h"
#include "config.h"

typedef enum action_t {
	ACT_NORMAL	     = 0x00000001,
	ACT_BROKEN_UPDATE    = 0x00000002,
} action_t;


const char *argp_program_version = PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;
static char doc[] = "\nVersion: " PACKAGE_VERSION "\n\tBuilt on "
	BUILD_CPU" "BUILD_OS" at "__DATE__" "__TIME__"\n"
	"\n"
	"ubigen - a tool for adding UBI information to a binary input file.\n";

static const char copyright [] __attribute__((unused)) =
	"FIXME: insert license type"; /* FIXME */

#define CHECK_ENDP(option, endp) do {			\
	if (*endp) {					\
		fprintf(stderr,				\
			"Parse error option \'%s\'. "	\
			"No correct numeric value.\n"	\
			, option);			\
		exit(EXIT_FAILURE);			\
	}						\
} while(0)

static struct argp_option options[] = {
	/* COMMON */
	{ name: NULL, key: 0, arg: NULL, flags: 0,
	  doc: "Common settings:",
	  group: 1},

	{ name: "copyright", key: 'c', arg: NULL,    flags: 0,
	  doc: "Print copyright information.",
	  group: 1 },

	{ name: "verbose", key: 'v', arg: NULL,	   flags: 0,
	  doc: "Print more progress information.",
	  group: 1 },

	{ name: "debug", key: 'd', arg: NULL, flags: 0,
	  group: 1 },


	/* INPUT */
	{ name: NULL, key: 0, arg: NULL, flags: 0,
	  doc: "UBI Settings:",
	  group: 4},

	{ name: "alignment", key: 'A', arg: "<num>", flags: 0,
	  doc: "Set the alignment size to <num> (default 1).\n"
	       "Values can be specified as bytes, 'ki' or 'Mi'.",
	  group: 4 },

	{ name: "blocksize", key: 'B', arg: "<num>", flags: 0,
	  doc: "Set the eraseblock size to <num> (default 128 KiB).\n"
	       "Values can be specified as bytes, 'ki' or 'Mi'.",
	  group: 4 },

	{ name: "erasecount", key: 'E', arg: "<num>", flags: 0,
	  doc: "Set the erase count to <num> (default 0)",
	  group: 4 },

	{ name: "setver", key: 'X', arg: "<num>", flags: 0,
	  doc: "Set UBI version number to <num> (default 1)",
	  group: 4 },

	{ name: "id", key: 'I', arg: "<num>", flags: 0,
	  doc: "The UBI volume id.",
	  group: 4 },


	{ name: "offset", key: 'O', arg: "<num>", flags: 0,
	  doc: "Offset from start of an erase block to the UBI volume header.",
	  group: 4 },

	{ name: "type", key: 'T', arg: "<num>", flags: 0,
	  doc: "The UBI volume type:\n1 = dynamic, 2 = static",
	  group: 4 },

	/* INPUT/OUTPUT */
	{ name: NULL, key: 0, arg: NULL, flags: 0,
	  doc: "Input/Output:",
	  group: 5 },

	{ name: "infile", key: 'i', arg: "<filename>", flags: 0,
	  doc: "Read input from file.",
	  group: 5 },

	{ name: "outfile", key: 'o', arg: "<filename>", flags: 0,
	  doc: "Write output to file (default is stdout).",
	  group: 5 },

	/* Special options */
	{ name: NULL, key: 0, arg: NULL, flags: 0,
	  doc: "Special options:",
	  group: 6 },

	{ name: "broken-update", key: 'U', arg: "<leb>", flags: 0,
	  doc: "Create an ubi image which simulates a broken update.\n"
	       "<leb> specifies the logical eraseblock number to update.\n",
	  group: 6 },

	{ name: NULL, key: 0, arg: NULL, flags: 0, doc: NULL, group: 0 },
};

typedef struct myargs {
	/* common settings */
	action_t action;
	int verbose;

	int32_t id;
	uint8_t type;
	uint32_t eb_size;
	uint64_t ec;
	uint8_t version;
	uint32_t hdr_offset;
	uint32_t update_block;
	uint32_t alignment;

	FILE* fp_in;
	FILE* fp_out;

	/* special stuff needed to get additional arguments */
	char *arg1;
	char **options;			/* [STRING...] */
} myargs;


static int ustrtoul(const char *cp, char **endp, unsigned int base)
{
	unsigned long result = strtoul(cp, endp, base);

	switch (**endp) {
	case 'G':
		result *= 1024;
	case 'M':
		result *= 1024;
	case 'k':
	case 'K':
		result *= 1024;
	/* "Ki", "ki", "Mi" or "Gi" are to be used. */
		if ((*endp)[1] == 'i')
			(*endp) += 2;
	}
	return result;
}

static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
	int err = 0;
	char* endp;

	myargs *args = state->input;

	switch (key) {
	case 'c':
		fprintf(stderr, "%s\n", copyright);
		exit(0);
		break;
	case 'o': /* output */
		args->fp_out = fopen(arg, "wb");
		if ((args->fp_out) == NULL) {
			fprintf(stderr, "Cannot open file %s for output\n",
					arg);
			exit(1);
		}
		break;
	case 'i': /* input */
		args->fp_in = fopen(arg, "rb");
		if ((args->fp_in) == NULL) {
			fprintf(stderr, "Cannot open file %s for input\n",
					arg);
			exit(1);
		}
		break;
	case 'v': /* verbose */
		args->verbose = 1;
		break;

	case 'B': /* eb_size */
		args->eb_size = (uint32_t) ustrtoul(arg, &endp, 0);
		CHECK_ENDP("B", endp);
		break;
	case 'E': /* erasecount */
		args->ec = (uint64_t) strtoul(arg, &endp, 0);
		CHECK_ENDP("E", endp);
		break;
	case 'I': /* id */
		args->id = (uint16_t) strtoul(arg, &endp, 0);
		CHECK_ENDP("I", endp);
		break;
	case 'T': /* type */
		args->type =  (uint16_t) strtoul(arg, &endp, 0);
		CHECK_ENDP("T", endp);
		break;
	case 'X': /* versionnr */
		args->version =	 (uint8_t) strtoul(arg, &endp, 0);
		CHECK_ENDP("X", endp);
		break;
	case 'O': /* offset for volume hdr */
		args->hdr_offset =
			(uint32_t) strtoul(arg, &endp, 0);
		CHECK_ENDP("O", endp);
		break;

	case 'U': /* broken update */
		args->action = ACT_BROKEN_UPDATE;
		args->update_block =
			(uint32_t) strtoul(arg, &endp, 0);
		CHECK_ENDP("U", endp);
		break;

	case ARGP_KEY_ARG:
		if (!args->fp_in) {
			args->fp_in = fopen(arg, "rb");
			if ((args->fp_in) == NULL) {
				fprintf(stderr,
				"Cannot open file %s for input\n", arg);
				exit(1);
			}
		}
		args->arg1 = arg;
		args->options = &state->argv[state->next];
		state->next = state->argc;
		break;
	case ARGP_KEY_END:

		if (args->id < 0) {
			err = 1;
			fprintf(stderr,
				"Please specify an UBI Volume ID.\n");
		}
		if (args->type == 0) {
			err = 1;
			fprintf(stderr,
				"Please specify an UBI Volume type.\n");
		}
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
	args_doc:    0,
	doc:	     doc,
	children:    NULL,
	help_filter: NULL,
	argp_domain: NULL,
};


int
main(int argc, char **argv)
{
	int rc = 0;
	ubi_info_t u;
	struct stat file_info;
	off_t input_len = 0; /* only used in static volumes */

	myargs args = {
		.action = ACT_NORMAL,
		.verbose = 0,

		.id = -1,
		.type = 0,
		.eb_size = 0,
		.update_block = 0,
		.ec = 0,
		.version = 0,
		.hdr_offset = (DEFAULT_PAGESIZE) - (UBI_VID_HDR_SIZE),
		.alignment = 1,

		.fp_in = NULL,
		.fp_out = stdout,
		/* arguments */
		.arg1 = NULL,
		.options = NULL,
	};

	ubigen_init(); /* Init CRC32 table in ubigen */

	/* parse arguments */
	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &args);

	if (fstat(fileno(args.fp_in), &file_info) != 0) {
		fprintf(stderr, "Cannot fetch file size "
				"from input file.\n");
	}
	input_len = file_info.st_size;

	rc = ubigen_create(&u, (uint32_t)args.id, args.type,
			args.eb_size, args.ec, args.alignment,
			args.version, args.hdr_offset, 0 ,input_len,
			args.fp_in, args.fp_out);

	if  (rc != 0) {
		fprintf(stderr, "Cannot create UBI info handler rc: %d\n", rc);
		exit(EXIT_FAILURE);
	}

	if (!args.fp_in || !args.fp_out) {
		fprintf(stderr, "Input/Output error.\n");
		exit(EXIT_FAILURE);

	}

	if (args.action & ACT_NORMAL) {
		rc = ubigen_write_complete(u);
	}
	else if (args.action & ACT_BROKEN_UPDATE) {
		rc = ubigen_write_broken_update(u, args.update_block);
	}
	if  (rc != 0) {
		fprintf(stderr, "Error converting input data.\n");
		exit(EXIT_FAILURE);
	}

	rc = ubigen_destroy(&u);
	return rc;
}
