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

#include "config.h"
#include "nandecc.h"

#define MAXPATH		1024
#define MIN(x,y)	((x)<(y)?(x):(y))

extern char *optarg;
extern int optind;

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

static char doc[] = "\nVersion: " PACKAGE_VERSION "\n\t"
	BUILD_OS" "BUILD_CPU" at "__DATE__" "__TIME__"\n"
	"\nSplit data and OOB.\n";

static const char *optionsstr =
"  -o, --output=<output>      Data output file\n"
"  -O, --oob=<oob>            OOB output file\n"
"  -p, --pagesize=<pagesize>  NAND pagesize\n"
"  -?, --help                 Give this help list\n"
"      --usage                Give a short usage message\n";

static const char *usage =
"Usage: nand2bin [-?] [-o <output>] [-O <oob>] [-p <pagesize>]\n"
"          [--output=<output>] [--oob=<oob>] [--pagesize=<pagesize>] [--help]\n"
"          [--usage] input.mif\n";

struct option long_options[] = {
	{ .name = "output", .has_arg = 1, .flag = NULL, .val = 'o' },
	{ .name = "oob", .has_arg = 1, .flag = NULL, .val = 'O' },
	{ .name = "pagesize", .has_arg = 1, .flag = NULL, .val = 'p' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = '?' },
	{ .name = "usage", .has_arg = 0, .flag = NULL, .val = 0 },
	{ NULL, 0, NULL, 0}
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
static int
parse_opt(int argc, char **argv, struct args *args)
{
	while (1) {
		int key;

		key = getopt_long(argc, argv, "o:O:p:?", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
			case 'p': /* --pagesize<pagesize> */
				args->pagesize = str_to_num(optarg);
				break;

			case 'o': /* --output=<output.bin> */
				args->output_file = optarg;
				break;

			case 'O': /* --oob=<oob.bin> */
				args->output_file = optarg;
				break;

			case '?': /* help */
				printf("Usage: nand2bin [OPTION...] input.mif\n");
				printf("%s", doc);
				printf("%s", optionsstr);
				printf("\nReport bugs to %s\n", PACKAGE_BUGREPORT);
				exit(0);
				break;

			case 'V':
				printf("%s\n", PACKAGE_VERSION);
				exit(0);
				break;

			default:
				printf("%s", usage);
				exit(-1);
		}
	}

	if (optind < argc)
		args->arg1 = argv[optind++];

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
hexdump(FILE *fp, const uint8_t *buf, ssize_t size)
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

	parse_opt(argc, argv, &myargs);

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
