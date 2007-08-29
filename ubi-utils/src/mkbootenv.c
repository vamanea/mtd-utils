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
 *
 * 1.2 Removed argp because we want to use uClibc.
 * 1.3 Minor cleanup
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <mtd/ubi-header.h>

#include "config.h"
#include "bootenv.h"
#include "error.h"

#define PROGRAM_VERSION "1.3"

static char doc[] = "\nVersion: " PROGRAM_VERSION "\n"
	"mkbootenv - processes bootenv text files and convertes "
	"them into a binary format.\n";

static const char copyright [] __attribute__((unused)) =
	"Copyright (c) International Business Machines Corp., 2006";

static const char *optionsstr =
"  -c, --copyright          Print copyright informatoin.\n"
"  -o, --output=<fname>     Write the output data to <output> instead of\n"
"                           stdout.\n"
"  -?, --help               Give this help list\n"
"      --usage              Give a short usage message\n"
"  -V, --version            Print program version\n";

static const char *usage =
"Usage: mkbootenv [-c?V] [-o <output>] [--copyright] [--output=<output>]\n"
"            [--help] [--usage] [--version] [bootenv-txt-file]\n";

struct option long_options[] = {
	{ .name = "copyright", .has_arg = 0, .flag = NULL, .val = 'c' },
	{ .name = "output", .has_arg = 1, .flag = NULL, .val = 'o' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = '?' },
	{ .name = "usage", .has_arg = 0, .flag = NULL, .val = 0 },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

typedef struct myargs {
	FILE* fp_in;
	FILE* fp_out;

	char *arg1;
	char **options;			/* [STRING...] */
} myargs;

static int
parse_opt(int argc, char **argv, myargs *args)
{
	while (1) {
		int key;

		key = getopt_long(argc, argv, "co:?V", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
			case 'c':
				fprintf(stderr, "%s\n", copyright);
				exit(0);
				break;
			case 'o':
				args->fp_out = fopen(optarg, "wb");
				if ((args->fp_out) == NULL) {
					fprintf(stderr, "Cannot open file %s "
						"for output\n", optarg);
					exit(1);
				}
				break;
			case '?': /* help */
				printf("%s", doc);
				printf("%s", optionsstr);
				printf("\nReport bugs to %s\n",
				       PACKAGE_BUGREPORT);
				exit(0);
				break;
			case 'V':
				printf("%s\n", PROGRAM_VERSION);
				exit(0);
				break;
			default:
				printf("%s", usage);
				exit(-1);
		}
	}

	if (optind < argc) {
		args->fp_in = fopen(argv[optind++], "rb");
		if ((args->fp_in) == NULL) {
			fprintf(stderr,	"Cannot open file %s for input\n",
				argv[optind]);
			exit(1);
		}
	}

	return 0;
}

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

	parse_opt(argc, argv, &args);

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
