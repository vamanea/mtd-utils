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
 *
 * 1.1 fixed output to stderr and stdout in logfile mode.
 * 1.2 updated.
 * 1.3 removed argp parsing to be able to use uClib.
 * 1.4 Minor cleanups.
 * 1.5 Forgot to delete raw block before updating it.
 * 1.6 Migrated to new libubi.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>

#include <pfiflash.h>
#undef DEBUG
#include "error.h"
#include "config.h"

#define PROGRAM_VERSION  "1.6"

static char doc[] = "\nVersion: " PROGRAM_VERSION "\n"
	"pfiflash - a tool for updating a controller with PFI files.\n";

static const char *optionsstr =
" Standard options:\n"
"  -c, --copyright            Print copyright information.\n"
"  -l, --logfile=<file>       Write a logfile to <file>.\n"
"  -v, --verbose              Be verbose during program execution.\n"
"\n"
" Process options:\n"
"  -C, --complete             Execute a complete system update. Updates both\n"
"                             sides.\n"
"  -p, --pdd-update=<type>    Specify the pdd-update algorithm. <type> is either\n"
"                             'keep', 'merge' or 'overwrite'.\n"
"  -r, --raw-flash=<dev>      Flash the raw data. Use the specified mtd device.\n"
"  -s, --side=<seqnum>        Select the side which shall be updated.\n"
"  -x, --compare              Only compare on-flash and pfi data, print info if\n"
"                             an update is neccessary and return appropriate\n"
"                             error code.\n"
"\n"
"  -?, --help                 Give this help list\n"
"      --usage                Give a short usage message\n"
"  -V, --version              Print program version\n";

static const char *usage =
"Usage: pfiflash [-cvC?V] [-l <file>] [-p <type>] [-r <dev>] [-s <seqnum>]\n"
"            [--copyright] [--logfile=<file>] [--verbose] [--complete]\n"
"            [--pdd-update=<type>] [--raw-flash=<dev>] [--side=<seqnum>]\n"
"            [--compare] [--help] [--usage] [--version] [pfifile]\n";

static const char copyright [] __attribute__((unused)) =
	"Copyright IBM Corp 2006";

struct option long_options[] = {
	{ .name = "copyright", .has_arg = 0, .flag = NULL, .val = 'c' },
	{ .name = "logfile", .has_arg = 1, .flag = NULL, .val = 'l' },
	{ .name = "verbose", .has_arg = 0, .flag = NULL, .val = 'v' },
	{ .name = "complete", .has_arg = 0, .flag = NULL, .val = 'C' },
	{ .name = "pdd-update", .has_arg = 1, .flag = NULL, .val = 'p' },
	{ .name = "raw-flash", .has_arg = 1, .flag = NULL, .val = 'r' },
	{ .name = "side", .has_arg = 1, .flag = NULL, .val = 's' },
	{ .name = "compare", .has_arg = 0, .flag = NULL, .val = 'x' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = '?' },
	{ .name = "usage", .has_arg = 0, .flag = NULL, .val = 0 },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

typedef struct myargs {
	int verbose;
	const char *logfile;
	const char *raw_dev;

	pdd_handling_t pdd_handling;
	int seqnum;
	int compare;
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


static int
parse_opt(int argc, char **argv, myargs *args)
{
	while (1) {
		int key;

		key = getopt_long(argc, argv, "cl:vCp:r:s:x?V",
				  long_options, NULL);
		if (key == -1)
			break;

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
				args->logfile = optarg;
				break;
				/* process options */
			case 'C':
				args->complete = 1;
				break;
			case 'p':
				args->pdd_handling = get_pdd_handling(optarg);
				if ((int)args->pdd_handling < 0) {
					err_quit("Unknown PDD handling: %s.\n"
						 "Please use either "
						 "'keep', 'merge' or"
						 "'overwrite'.\n'");
				}
				break;
			case 's':
				args->seqnum = get_update_seqnum(optarg);
				if (args->seqnum < 0) {
					err_quit("Unsupported side: %s.\n"
						 "Supported sides are '0' "
						 "and '1'\n", optarg);
				}
				break;
			case 'x':
				args->compare = 1;
				break;
			case 'r':
				args->raw_dev = optarg;
				break;
			case '?': /* help */
				err_msg("Usage: pfiflash [OPTION...] [pfifile]");
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

	if (optind < argc) {
		args->fp_in = fopen(argv[optind++], "r");
		if ((args->fp_in) == NULL) {
			err_sys("Cannot open PFI file %s for input",
				argv[optind]);
		}
	}

	return 0;
}

int main (int argc, char** argv)
{
	int rc = 0;
	char err_buf[PFIFLASH_MAX_ERR_BUF_SIZE];
	memset(err_buf, '\0', PFIFLASH_MAX_ERR_BUF_SIZE);

	myargs args = {
		.verbose    = 0,
		.seqnum	    = -1,
		.compare    = 0,
		.complete   = 0,
		.logfile    = NULL, /* "/tmp/pfiflash.log", */
		.pdd_handling = PDD_KEEP,
		.fp_in	    = stdin,
		.raw_dev    = NULL,
	};

	parse_opt(argc, argv, &args);
	error_initlog(args.logfile);

	if (!args.fp_in) {
		rc = -1;
		snprintf(err_buf, PFIFLASH_MAX_ERR_BUF_SIZE,
			 "No PFI input file specified!\n");
		goto err;
	}

	rc = pfiflash_with_options(args.fp_in, args.complete, args.seqnum,
			args.compare, args.pdd_handling, args.raw_dev, err_buf,
			PFIFLASH_MAX_ERR_BUF_SIZE);
	if (rc < 0) {
		goto err_fp;
	}

 err_fp:
	if (args.fp_in != stdin)
		fclose(args.fp_in);
 err:
	if (rc != 0)
		err_msg("pfiflash: %s\nrc: %d\n", err_buf, rc);
	return rc;
}
