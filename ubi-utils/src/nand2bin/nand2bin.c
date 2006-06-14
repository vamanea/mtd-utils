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
 * Author: Frank Haverkamp
 *
 * An utility to decompose NAND images and strip OOB off. Not yet finished ...
 */
#include <config.h>
#include <argp.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "nandecc.h"

#define MAXPATH		1024
#define MIN(x,y)	((x)<(y)?(x):(y))

struct args {
	const char *oob_file;
	const char *output_file;
	size_t pagesize;

	/* special stuff needed to get additional arguments */
	char *arg1;
	char **options;		/* [STRING...] */
};

static struct args myargs = {
	.output_file = "data.bin",
	.oob_file = "oob.bin",
	.pagesize = 2048,
	.arg1 = NULL,
	.options = NULL,
};

static error_t parse_opt (int key, char *arg, struct argp_state *state);

const char *argp_program_bug_address = "<haver@vnet.ibm.com>";

static char doc[] = "\nVersion: " VERSION "\n\t"
	HOST_OS" "HOST_CPU" at "__DATE__" "__TIME__"\n"
	"\nSplit data and OOB.\n";

static struct argp_option options[] = {
	{ .name = "pagesize",
	  .key = 'p',
	  .arg = "<pagesize>",
	  .flags = 0,
	  .doc = "NAND pagesize",
	  .group = OPTION_ARG_OPTIONAL },

	{ .name = "output",
	  .key = 'o',
	  .arg = "<output>",
	  .flags = 0,
	  .doc = "Data output file",
	  .group = OPTION_ARG_OPTIONAL },

	{ .name = "oob",
	  .key = 'O',
	  .arg = "<oob>",
	  .flags = 0,
	  .doc = "OOB output file",
	  .group = OPTION_ARG_OPTIONAL },

	{ .name = NULL, .key = 0, .arg = NULL, .flags = 0,
	  .doc = NULL, .group = 0 },
};

static struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.args_doc = "input.mif",
	.doc =	doc,
	.children = NULL,
	.help_filter = NULL,
	.argp_domain = NULL,
};

/*
 * str_to_num - Convert string into number and cope with endings like
 *              k, K, kib, KiB for kilobyte
 *              m, M, mib, MiB for megabyte
 */
uint32_t str_to_num(char *str)
{
	char *s = str;
	ulong num = strtoul(s, &s, 0);

	if (*s != '\0') {
		if (strcmp(s, "KiB") == 0)
			num *= 1024;
		else if (strcmp(s, "MiB") == 0)
			num *= 1024*1024;
		else {
			fprintf(stderr, "WARNING: Wrong number format "
				"\"%s\", check your paramters!\n", str);
		}
	}
	return num;
}

/*
 * @brief Parse the arguments passed into the test case.
 *
 * @param key            The parameter.
 * @param arg            Argument passed to parameter.
 * @param state          Location to put information on parameters.
 *
 * @return error
 *
 * Get the `input' argument from `argp_parse', which we know is a
 * pointer to our arguments structure.
 */
static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
	struct args *args = state->input;

	switch (key) {
	case 'p': /* --pagesize<pagesize> */
		args->pagesize = str_to_num(arg);		break;

	case 'o': /* --output=<output.bin> */
		args->output_file = arg;
		break;

	case 'O': /* --oob=<oob.bin> */
		args->output_file = arg;
		break;

	case ARGP_KEY_NO_ARGS:
		/* argp_usage(state); */
		break;

	case ARGP_KEY_ARG:
		args->arg1 = arg;
		/* Now we consume all the rest of the arguments.
                   `state->next' is the index in `state->argv' of the
                   next argument to be parsed, which is the first STRING
                   we're interested in, so we can just use
                   `&state->argv[state->next]' as the value for
                   arguments->strings.

                   _In addition_, by setting `state->next' to the end
                   of the arguments, we can force argp to stop parsing here and
                   return. */

		args->options = &state->argv[state->next];
		state->next = state->argc;
		break;

	case ARGP_KEY_END:
		/* argp_usage(state); */
		break;

	default:
		return(ARGP_ERR_UNKNOWN);
	}

	return 0;
}

static int calc_oobsize(size_t pagesize)
{
	switch (pagesize) {
	case 512:  return 16;
	case 2048: return 64;
	default:
		exit(EXIT_FAILURE);
	}
	return 0;
}

static inline void
hexdump(FILE *fp, const uint8_t *buf, size_t size)
{
	int k;

	for (k = 0; k < size; k++) {
		fprintf(fp, "%02x ", buf[k]);
		if ((k & 15) == 15)
			fprintf(fp, "\n");
	}
}

static int
process_page(uint8_t* buf, uint8_t *oobbuf, size_t pagesize)
{
	int eccpoi, oobsize;
	size_t i;

	switch (pagesize) {
	case 2048: oobsize = 64; eccpoi = 64 / 2; break;
	case 512:  oobsize = 16; eccpoi = 16 / 2; break;
	default:
		fprintf(stderr, "Unsupported page size: %d\n", pagesize);
		return -EINVAL;
	}
	memset(oobbuf, 0xff, oobsize);

	for (i = 0; i < pagesize; i += 256, eccpoi += 3) {
		oobbuf[eccpoi++] = 0x0;
		/* Calculate ECC */
		nand_calculate_ecc(&buf[i], &oobbuf[eccpoi]);
	}
	return 0;
}

static int decompose_image(FILE *in_fp, FILE *bin_fp, FILE *oob_fp,
			   size_t pagesize)
{
	int read, rc, page = 0;
	size_t oobsize = calc_oobsize(pagesize);
	uint8_t *buf = malloc(pagesize);
	uint8_t *oob = malloc(oobsize);
	uint8_t *calc_oob = malloc(oobsize);
	uint8_t *calc_buf = malloc(pagesize);

	if (!buf)
		exit(EXIT_FAILURE);
	if (!oob)
		exit(EXIT_FAILURE);
	if (!calc_oob)
		exit(EXIT_FAILURE);
	if (!calc_buf)
		exit(EXIT_FAILURE);

	while (!feof(in_fp)) {
		read = fread(buf, 1, pagesize, in_fp);
		if (ferror(in_fp)) {
			fprintf(stderr, "I/O Error.");
			exit(EXIT_FAILURE);
		}
		rc = fwrite(buf, 1, pagesize, bin_fp);
		if (ferror(bin_fp)) {
			fprintf(stderr, "I/O Error.");
			exit(EXIT_FAILURE);
		}
		read = fread(oob, 1, oobsize, in_fp);
		if (ferror(in_fp)) {
			fprintf(stderr, "I/O Error.");
			exit(EXIT_FAILURE);
		}
		rc = fwrite(oob, 1, oobsize, oob_fp);
		if (ferror(bin_fp)) {
			fprintf(stderr, "I/O Error.");
			exit(EXIT_FAILURE);
		}
		process_page(buf, calc_oob, pagesize);

		memcpy(calc_buf, buf, pagesize);

		rc = nand_correct_data(calc_buf, oob);
		if ((rc != -1) || (memcmp(buf, calc_buf, pagesize) != 0)) {
			fprintf(stdout, "Page %d: data does not match!\n",
				page);
		}
		page++;
	}
	free(calc_buf);
	free(calc_oob);
	free(oob);
	free(buf);
	return 0;
}

int
main(int argc, char *argv[])
{
	FILE *in, *bin, *oob;

	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &myargs);

	if (!myargs.arg1) {
		fprintf(stderr, "Please specify input file!\n");
		exit(EXIT_FAILURE);
	}

	in = fopen(myargs.arg1, "r");
	if (!in) {
		perror("Cannot open file");
		exit(EXIT_FAILURE);
	}
	bin = fopen(myargs.output_file, "w+");
	if (!bin) {
		perror("Cannot open file");
		exit(EXIT_FAILURE);
	}
	oob = fopen(myargs.oob_file, "w+");
	if (!oob) {
		perror("Cannot open file");
		exit(EXIT_FAILURE);
	}

	decompose_image(in, bin, oob, myargs.pagesize);

	fclose(in);
	fclose(bin);
	fclose(oob);

	exit(EXIT_SUCCESS);
}
