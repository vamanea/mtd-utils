/*
 * Copyright (c) International Business Machines Corp., 2007
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

/*
 * Create a flashable NAND image from a binary image
 *
 * History:
 * 1.0 Initial release (tglx)
 * 1.1 Understands hex and dec input parameters (tglx)
 * 1.2 Generates separated OOB data, if needed. (oloh)
 * 1.3 Padds data/oob to a given size. (oloh)
 * 1.4 Removed argp because we want to use uClibc.
 * 1.5 Minor cleanup
 * 1.6 written variable not initialized (-j did not work) (haver)
 */

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "error.h"
#include "config.h"
#include "nandecc.h"

#define PROGRAM_VERSION "1.6"

#define CHECK_ENDP(option, endp) do {			\
	if (*endp) {					\
		fprintf(stderr,				\
			"Parse error option \'%s\'. "	\
			"No correct numeric value.\n"	\
			, option);			\
		exit(EXIT_FAILURE);			\
	}						\
} while(0)

typedef enum action_t {
	ACT_NORMAL	    = 0x00000001,
} action_t;

#define PAGESIZE	2048
#define PADDING		   0 /* 0 means, do not adjust anything */
#define BUFSIZE		4096

static char doc[] = "\nVersion: " PROGRAM_VERSION "\n"
	"bin2nand - a tool for adding OOB information to a "
	"binary input file.\n";

static const char *optionsstr =
"  -c, --copyright          Print copyright informatoin.\n"
"  -j, --padding=<num>      Padding in Byte/Mi/ki. Default = no padding\n"
"  -p, --pagesize=<num>     Pagesize in Byte/Mi/ki. Default = 2048\n"
"  -o, --output=<fname>     Output filename.  Interleaved Data/OOB if\n"
"                           output-oob not specified.\n"
"  -q, --output-oob=<fname> Write OOB data in separate file.\n"
"  -?, --help               Give this help list\n"
"      --usage              Give a short usage message\n"
"  -V, --version            Print program version\n";

static const char *usage =
"Usage: bin2nand [-c?V] [-j <num>] [-p <num>] [-o <fname>] [-q <fname>]\n"
"            [--copyright] [--padding=<num>] [--pagesize=<num>]\n"
"            [--output=<fname>] [--output-oob=<fname>] [--help] [--usage]\n"
"            [--version]\n";

struct option long_options[] = {
	{ .name = "copyright", .has_arg = 0, .flag = NULL, .val = 'c' },
	{ .name = "padding", .has_arg = 1, .flag = NULL, .val = 'j' },
	{ .name = "pagesize", .has_arg = 1, .flag = NULL, .val = 'p' },
	{ .name = "output", .has_arg = 1, .flag = NULL, .val = 'o' },
	{ .name = "output-oob", .has_arg = 1, .flag = NULL, .val = 'q' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = '?' },
	{ .name = "usage", .has_arg = 0, .flag = NULL, .val = 0 },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

static const char copyright [] __attribute__((unused)) =
	"Copyright IBM Corp. 2006";

typedef struct myargs {
	action_t action;

	size_t pagesize;
	size_t padding;

	FILE* fp_in;
	char *file_out_data; /* Either: Data and OOB interleaved
				or plain data */
	char *file_out_oob; /* OOB Data only. */

	/* special stuff needed to get additional arguments */
	char *arg1;
	char **options;			/* [STRING...] */
} myargs;


static int ustrtoull(const char *cp, char **endp, unsigned int base)
{
	unsigned long long res = strtoull(cp, endp, base);

	switch (**endp) {
	case 'G':
		res *= 1024;
	case 'M':
		res *= 1024;
	case 'k':
	case 'K':
		res *= 1024;
	/* "Ki", "ki", "Mi" or "Gi" are to be used. */
		if ((*endp)[1] == 'i')
			(*endp) += 2;
	}
	return res;
}

static int
parse_opt(int argc, char **argv, myargs *args)
{
	char* endp;

	while (1) {
		int key;

		key = getopt_long(argc, argv, "cj:p:o:q:?V", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
			case 'p': /* pagesize */
				args->pagesize = (size_t)
					ustrtoull(optarg, &endp, 0);
				CHECK_ENDP("p", endp);
				break;
			case 'j': /* padding */
				args->padding = (size_t)
					ustrtoull(optarg, &endp, 0);
				CHECK_ENDP("j", endp);
				break;
			case 'o': /* output */
				args->file_out_data = optarg;
				break;
			case 'q': /* output oob */
				args->file_out_oob = optarg;
				break;
			case '?': /* help */
				printf("%s", doc);
				printf("%s", optionsstr);
				exit(0);
				break;
			case 'V':
				printf("%s\n", PROGRAM_VERSION);
				exit(0);
				break;
			case 'c':
				printf("%s\n", copyright);
				exit(0);
			default:
				printf("%s", usage);
				exit(-1);
		}
	}

	if (optind < argc) {
		args->fp_in = fopen(argv[optind++], "rb");
		if ((args->fp_in) == NULL) {
			err_quit("Cannot open file %s for input\n",
				 argv[optind++]);
		}
	}

	return 0;
}

static int
process_page(uint8_t* buf, size_t pagesize,
	FILE *fp_data, FILE* fp_oob, size_t* written)
{
	int eccpoi, oobsize;
	size_t i;
	uint8_t oobbuf[64];

	memset(oobbuf, 0xff, sizeof(oobbuf));

	switch(pagesize) {
	case 2048: oobsize = 64; eccpoi = 64 / 2; break;
	case 512:  oobsize = 16; eccpoi = 16 / 2; break;
	default:
		err_msg("Unsupported page size: %d\n", pagesize);
		return -EINVAL;
	}

	for (i = 0; i < pagesize; i += 256, eccpoi += 3) {
		oobbuf[eccpoi++] = 0x0;
		/* Calculate ECC */
		nand_calculate_ecc(&buf[i], &oobbuf[eccpoi]);
	}

	/* write data */
	*written += fwrite(buf, 1, pagesize, fp_data);

	/* either separate oob or interleave with data */
	if (fp_oob) {
		i = fwrite(oobbuf, 1, oobsize, fp_oob);
		if (ferror(fp_oob)) {
			err_msg("IO error\n");
			return -EIO;
		}
	}
	else {
		i = fwrite(oobbuf, 1, oobsize, fp_data);
		if (ferror(fp_data)) {
			err_msg("IO error\n");
			return -EIO;
		}
	}

	return 0;
}

int main (int argc, char** argv)
{
	int rc = -1;
	int res = 0;
	size_t written = 0, read;
	myargs args = {
		.action	  = ACT_NORMAL,
		.pagesize = PAGESIZE,
		.padding  = PADDING,
		.fp_in	  = NULL,
		.file_out_data = NULL,
		.file_out_oob = NULL,
	};

	FILE* fp_out_data = stdout;
	FILE* fp_out_oob = NULL;

	parse_opt(argc, argv, &args);

	uint8_t* buf = calloc(1, BUFSIZE);
	if (!buf) {
		err_quit("Cannot allocate page buffer.\n");
	}

	if (!args.fp_in) {
		err_msg("No input image specified!\n");
		goto err;
	}

	if (args.file_out_data) {
		fp_out_data = fopen(args.file_out_data, "wb");
		if (fp_out_data == NULL) {
			err_sys("Cannot open file %s for output\n",
					args.file_out_data);
			goto err;
		}
	}

	if (args.file_out_oob) {
		fp_out_oob = fopen(args.file_out_oob, "wb");
		if (fp_out_oob == NULL) {
			err_sys("Cannot open file %s for output\n",
					args.file_out_oob);
			goto err;
		}
	}


	while(1) {
		read = fread(buf, 1, args.pagesize, args.fp_in);
		if (feof(args.fp_in) && read == 0)
			break;

		if (read < args.pagesize) {
			err_msg("Image not page aligned\n");
			goto err;
		}

		if (ferror(args.fp_in)) {
			err_msg("Read error\n");
			goto err;
		}

		res = process_page(buf, args.pagesize, fp_out_data,
				fp_out_oob, &written);
		if (res != 0)
			goto err;
	}

	while (written < args.padding) {
		memset(buf, 0xff, args.pagesize);
		res = process_page(buf, args.pagesize, fp_out_data,
				fp_out_oob, &written);
		if (res != 0)
			goto err;
	}

	rc = 0;
err:
	free(buf);

	if (args.fp_in)
		fclose(args.fp_in);

	if (fp_out_oob)
		fclose(fp_out_oob);

	if (fp_out_data && fp_out_data != stdout)
		fclose(fp_out_data);

	if (rc != 0) {
		err_msg("Error during conversion. rc: %d\n", rc);
		if (args.file_out_data)
			remove(args.file_out_data);
		if (args.file_out_oob)
			remove(args.file_out_oob);
	}
	return rc;
}
