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

/*
 * Create a flashable NAND image from a binary image
 *
 * History:
 *	1.0:	Initial release (tglx)
 *
 *	1.1:	Understands hex and dec input parameters (tglx)
 *	1.2:	Generates separated OOB data, if needed. (oloh)
 *	1.3:	Padds data/oob to a given size. (oloh)
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <argp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "error.h"
#include "config.h"
#include "nandecc.h"

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

const char *argp_program_version = PACKAGE_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;
static char doc[] = "\nVersion: " PACKAGE_VERSION "\n\tBuilt on "
	BUILD_CPU" "BUILD_OS" at "__DATE__" "__TIME__"\n"
	"\n"
	"bin2nand - a tool for adding OOB information to a "
	"binary input file.\n";

static const char copyright [] __attribute__((unused)) =
	"FIXME: insert license type."; /* FIXME */

static struct argp_option options[] = {
	{ name: "copyright", key: 'c', arg: NULL, flags: 0,
	  doc: "Print copyright information.",
	  group: 1 },

	{ name: "pagesize", key: 'p', arg: "<num>", flags: 0,
	  doc: "Pagesize in Byte/Mi/ki. Default: 2048",
	  group: 1 },

	{ name: "padding", key: 'j', arg: "<num>", flags: 0,
	  doc: "Padding in Byte/Mi/ki. Default: no padding",
	  group: 1 },

	/* Output options */
	{ name: NULL, key: 0, arg: NULL, flags: 0,
	  doc: "Output settings:",
	  group: 2 },

	{ name: "output", key: 'o', arg: "<fname>", flags: 0,
	  doc: "Output filename. Interleaved Data/OOB if output-oob not "
	       "specified.",
	  group: 2 },

	{ name: "output-oob", key: 'q', arg: "<fname>", flags: 0,
	  doc: "Write OOB data in separate file.",
	  group: 2 },

	{ name: NULL, key: 0, arg: NULL, flags: 0, doc: NULL, group: 0 },
};

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

static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
	int err = 0;
	char* endp;

	myargs *args = state->input;

	switch (key) {
	case 'p': /* pagesize */
		args->pagesize = (size_t) ustrtoull(arg, &endp, 0);
		CHECK_ENDP("p", endp);
		break;
	case 'j': /* padding */
		args->padding = (size_t) ustrtoull(arg, &endp, 0);
		CHECK_ENDP("j", endp);
		break;
	case 'o': /* output */
		args->file_out_data = arg;
		break;
	case 'q': /* output oob */
		args->file_out_oob = arg;
		break;
	case ARGP_KEY_ARG: /* input file */
		args->fp_in = fopen(arg, "rb");
		if ((args->fp_in) == NULL) {
			err_quit("Cannot open file %s for input\n", arg);
		}
		args->arg1 = arg;
		args->options = &state->argv[state->next];
		state->next = state->argc;
		break;
	case ARGP_KEY_END:
		if (err) {
			err_msg("\n");
			argp_usage(state);
			exit(EXIT_FAILURE);
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
		fwrite(oobbuf, 1, oobsize, fp_oob);
		if (ferror(fp_oob)) {
			err_msg("IO error\n");
			return -EIO;
		}
	}
	else {
		fwrite(oobbuf, 1, oobsize, fp_data);
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
	size_t written, read;
	myargs args = {
		.action	  = ACT_NORMAL,
		.pagesize = PAGESIZE,
		.padding  = PADDING,
		.fp_in	  = NULL,
		.file_out_data = "",
		.file_out_oob = "",
	};

	FILE* fp_out_data = stdout;
	FILE* fp_out_oob = NULL;

	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &args);

	uint8_t* buf = calloc(1, BUFSIZE);
	if (!buf) {
		err_quit("Cannot allocate page buffer.\n");
	}

	if (!args.fp_in) {
		err_msg("No input image specified!\n");
		goto err;
	}

	if (strcmp(args.file_out_data, "") != 0) {
		fp_out_data = fopen(args.file_out_data, "wb");
		if (fp_out_data == NULL) {
			err_sys("Cannot open file %s for output\n",
					args.file_out_data);
			goto err;
		}
	}

	if (strcmp(args.file_out_oob, "") != 0) {
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
		remove(args.file_out_data);
		remove(args.file_out_oob);
	}
	return rc;
}
