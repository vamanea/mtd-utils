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
 * 1.2 Removed argp because we want to use uClibc.
 * 1.3 Minor cleanups
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <mtd/ubi-header.h>

#include "ubigen.h"
#include "config.h"

#define PROGRAM_VERSION "1.3"

typedef enum action_t {
	ACT_NORMAL	     = 0x00000001,
	ACT_BROKEN_UPDATE    = 0x00000002,
} action_t;

static char doc[] = "\nVersion: " PROGRAM_VERSION "\n"
	"ubigen - a tool for adding UBI information to a binary input file.\n";

static const char *optionsstr =
" Common settings:\n"
"  -c, --copyright            Print copyright information.\n"
"  -d, --debug\n"
"  -v, --verbose              Print more progress information.\n"
"\n"
" UBI Settings:\n"
"  -A, --alignment=<num>      Set the alignment size to <num> (default 1).\n"
"                             Values can be specified as bytes, 'ki' or 'Mi'.\n"
"  -B, --blocksize=<num>      Set the eraseblock size to <num> (default 128\n"
"                             KiB).\n"
"                             Values can be specified as bytes, 'ki' or 'Mi'.\n"
"  -E, --erasecount=<num>     Set the erase count to <num> (default 0)\n"
"  -I, --id=<num>             The UBI volume id.\n"
"  -O, --offset=<num>         Offset from start of an erase block to the UBI\n"
"                             volume header.\n"
"  -T, --type=<num>           The UBI volume type:\n"
"                             1 = dynamic, 2 = static\n"
"  -X, --setver=<num>         Set UBI version number to <num> (default 1)\n"
"\n"
" Input/Output:\n"
"  -i, --infile=<filename>    Read input from file.\n"
"  -o, --outfile=<filename>   Write output to file (default is stdout).\n"
"\n"
" Special options:\n"
"  -U, --broken-update=<leb>  Create an ubi image which simulates a broken\n"
"                             update.\n"
"                             <leb> specifies the logical eraseblock number to\n"
"                             update.\n"
"\n"
"  -?, --help                 Give this help list\n"
"      --usage                Give a short usage message\n"
"  -V, --version              Print program version\n";

static const char *usage =
"Usage: ubigen [-cdv?V] [-A <num>] [-B <num>] [-E <num>] [-I <num>]\n"
"          [-O <num>] [-T <num>] [-X <num>] [-i <filename>] [-o <filename>]\n"
"          [-U <leb>] [--copyright] [--debug] [--verbose] [--alignment=<num>]\n"
"          [--blocksize=<num>] [--erasecount=<num>] [--id=<num>]\n"
"          [--offset=<num>] [--type=<num>] [--setver=<num>]\n"
"          [--infile=<filename>] [--outfile=<filename>]\n"
"          [--broken-update=<leb>] [--help] [--usage] [--version]\n";

struct option long_options[] = {
	{ .name = "copyright", .has_arg = 0, .flag = NULL, .val = 'c' },
	{ .name = "debug", .has_arg = 0, .flag = NULL, .val = 'd' },
	{ .name = "verbose", .has_arg = 0, .flag = NULL, .val = 'v' },
	{ .name = "alignment", .has_arg = 1, .flag = NULL, .val = 'A' },
	{ .name = "blocksize", .has_arg = 1, .flag = NULL, .val = 'B' },
	{ .name = "erasecount", .has_arg = 1, .flag = NULL, .val = 'E' },
	{ .name = "id", .has_arg = 1, .flag = NULL, .val = 'I' },
	{ .name = "offset", .has_arg = 1, .flag = NULL, .val = 'O' },
	{ .name = "type", .has_arg = 1, .flag = NULL, .val = 'T' },
	{ .name = "setver", .has_arg = 1, .flag = NULL, .val = 'X' },
	{ .name = "infile", .has_arg = 1, .flag = NULL, .val = 'i' },
	{ .name = "outfile", .has_arg = 1, .flag = NULL, .val = 'o' },
	{ .name = "broken-update", .has_arg = 1, .flag = NULL, .val = 'U' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = '?' },
	{ .name = "usage", .has_arg = 0, .flag = NULL, .val = 0 },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

static const char copyright [] __attribute__((unused)) =
	"Copyright IBM Corp 2006";

#define CHECK_ENDP(option, endp) do {			\
	if (*endp) {					\
		fprintf(stderr,				\
			"Parse error option \'%s\'. "	\
			"No correct numeric value.\n"	\
			, option);			\
		exit(EXIT_FAILURE);			\
	}						\
} while(0)

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

static int
parse_opt(int argc, char **argv, myargs *args)
{
	int err = 0;
	char* endp;

	while (1) {
		int key;

		key = getopt_long(argc, argv, "cdvA:B:E:I:O:T:X:i:o:U:?V",
				long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
			case 'c':
				fprintf(stderr, "%s\n", copyright);
				exit(0);
				break;
			case 'o': /* output */
				args->fp_out = fopen(optarg, "wb");
				if ((args->fp_out) == NULL) {
					fprintf(stderr, "Cannot open file %s "
						"for output\n", optarg);
					exit(1);
				}
				break;
			case 'i': /* input */
				args->fp_in = fopen(optarg, "rb");
				if ((args->fp_in) == NULL) {
					fprintf(stderr, "Cannot open file %s "
						"for input\n", optarg);
					exit(1);
				}
				break;
			case 'v': /* verbose */
				args->verbose = 1;
				break;

			case 'B': /* eb_size */
				args->eb_size =
					(uint32_t)ustrtoul(optarg, &endp, 0);
				CHECK_ENDP("B", endp);
				break;
			case 'E': /* erasecount */
				args->ec = (uint64_t)strtoul(optarg, &endp, 0);
				CHECK_ENDP("E", endp);
				break;
			case 'I': /* id */
				args->id = (uint16_t)strtoul(optarg, &endp, 0);
				CHECK_ENDP("I", endp);
				break;
			case 'T': /* type */
				args->type =
					(uint16_t)strtoul(optarg, &endp, 0);
				CHECK_ENDP("T", endp);
				break;
			case 'X': /* versionnr */
				args->version =
					(uint8_t)strtoul(optarg, &endp, 0);
				CHECK_ENDP("X", endp);
				break;
			case 'O': /* offset for volume hdr */
				args->hdr_offset =
					(uint32_t) strtoul(optarg, &endp, 0);
				CHECK_ENDP("O", endp);
				break;

			case 'U': /* broken update */
				args->action = ACT_BROKEN_UPDATE;
				args->update_block =
					(uint32_t) strtoul(optarg, &endp, 0);
				CHECK_ENDP("U", endp);
				break;

			case '?': /* help */
				fprintf(stderr, "Usage: ubigen [OPTION...]\n");
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

	if (optind < argc) {
		if (!args->fp_in) {
			args->fp_in = fopen(argv[optind++], "rb");
			if ((args->fp_in) == NULL) {
				fprintf(stderr,	"Cannot open file %s for "
					"input\n", argv[optind]);
				exit(1);
			}
		}
	}
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
		fprintf(stderr, "%s", usage);
		exit(1);
	}

	return 0;
}


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
	parse_opt(argc, argv, &args);

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
