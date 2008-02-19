/*
 * Copyright (c) International Business Machines Corp., 2006, 2007
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
 *
 * 1.2 Removed argp because we want to use uClibc.
 * 1.3 Minor cleanup
 * 1.4 Fixed OOB output file
 * 1.5 Added verbose output and option to set blocksize.
 *     Added split block mode for more convenient analysis.
 * 1.6 Fixed ECC error detection and correction.
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

#define PROGRAM_VERSION "1.6"

#define MAXPATH		1024
#define MIN(x,y)	((x)<(y)?(x):(y))

struct args {
	const char *oob_file;
	const char *output_file;
	size_t pagesize;
	size_t blocksize;
	int split_blocks;
	size_t in_len;		/* size of input file */
	int correct_ecc;

	/* special stuff needed to get additional arguments */
	char *arg1;
	char **options;		/* [STRING...] */
};

static struct args myargs = {
	.output_file = "data.bin",
	.oob_file = "oob.bin",
	.pagesize = 2048,
	.blocksize = 128 * 1024,
	.in_len = 0,
	.split_blocks = 0,
	.correct_ecc = 0,
	.arg1 = NULL,
	.options = NULL,
};

static char doc[] = "\nVersion: " PROGRAM_VERSION "\n"
	"nand2bin - split data and OOB.\n";

static const char *optionsstr =
"  -o, --output=<output>      Data output file\n"
"  -O, --oob=<oob>            OOB output file\n"
"  -p, --pagesize=<pagesize>  NAND pagesize\n"
"  -b, --blocksize=<blocksize> NAND blocksize\n"
"  -s, --split-blocks         generate binaries for each block\n"
"  -e, --correct-ecc          Correct data according to ECC info\n"
"  -v, --verbose              verbose output\n"
"  -?, --help                 Give this help list\n"
"      --usage                Give a short usage message\n";

static const char *usage =
"Usage: nand2bin [-?] [-o <output>] [-O <oob>] [-p <pagesize>]\n"
"          [--output=<output>] [--oob=<oob>] [--pagesize=<pagesize>] [--help]\n"
"          [--usage] input.mif\n";

static int verbose = 0;

static struct option long_options[] = {
	{ .name = "output", .has_arg = 1, .flag = NULL, .val = 'o' },
	{ .name = "oob", .has_arg = 1, .flag = NULL, .val = 'O' },
	{ .name = "pagesize", .has_arg = 1, .flag = NULL, .val = 'p' },
	{ .name = "blocksize", .has_arg = 1, .flag = NULL, .val = 'b' },
	{ .name = "split-blocks", .has_arg = 0, .flag = NULL, .val = 's' },
	{ .name = "correct-ecc", .has_arg = 0, .flag = NULL, .val = 'e' },
	{ .name = "verbose", .has_arg = 0, .flag = NULL, .val = 'v' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = '?' },
	{ .name = "usage", .has_arg = 0, .flag = NULL, .val = 0 },
	{ NULL, 0, NULL, 0}
};

/*
 * str_to_num - Convert string into number and cope with endings like
 *              k, K, kib, KiB for kilobyte
 *              m, M, mib, MiB for megabyte
 */
static uint32_t str_to_num(char *str)
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
 * @param argc           The number of arguments
 * @param argv           The argument list
 * @param args           Pointer to program args structure
 *
 * @return error
 *
 */
static int parse_opt(int argc, char **argv, struct args *args)
{
	while (1) {
		int key;

		key = getopt_long(argc, argv, "b:eo:O:p:sv?", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'p': /* --pagesize<pagesize> */
			args->pagesize = str_to_num(optarg);
			break;

		case 'b': /* --blocksize<blocksize> */
			args->blocksize = str_to_num(optarg);
			break;

		case 'v': /* --verbose */
			verbose++;
			break;

		case 's': /* --split-blocks */
			args->split_blocks = 1;
			break;

		case 'e': /* --correct-ecc */
			args->correct_ecc = 1;
			break;

		case 'o': /* --output=<output.bin> */
			args->output_file = optarg;
			break;

		case 'O': /* --oob=<oob.bin> */
			args->oob_file = optarg;
			break;

		case '?': /* help */
			printf("Usage: nand2bin [OPTION...] input.mif\n");
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

static inline void hexdump(FILE *fp, const uint8_t *buf, ssize_t size)
{
	int k;

	for (k = 0; k < size; k++) {
		fprintf(fp, "%02x ", buf[k]);
		if ((k & 15) == 15)
			fprintf(fp, "\n");
	}
}

static int process_page(uint8_t* buf, uint8_t *oobbuf, size_t pagesize)
{
	int eccpoi, oobsize;
	size_t i;

	switch (pagesize) {
	case 2048: oobsize = 64; eccpoi = 64 / 2; break;
	case 512:  oobsize = 16; eccpoi = 16 / 2; break;
	default:
		fprintf(stderr, "Unsupported page size: %zd\n", pagesize);
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

static int bad_marker_offs_in_oob(int pagesize)
{
	switch (pagesize) {
	case 2048: return 0;
	case 512:  return 5;
	}
	return -EINVAL;
}

static int decompose_image(struct args *args, FILE *in_fp,
			   FILE *bin_fp, FILE *oob_fp)
{
	int read, rc, page = 0;
	size_t oobsize = calc_oobsize(args->pagesize);
	uint8_t *buf = malloc(args->pagesize);
	uint8_t *oob = malloc(oobsize);
	uint8_t *calc_oob = malloc(oobsize);
	uint8_t *calc_buf = malloc(args->pagesize);
	uint8_t *page_buf;
	int pages_per_block = args->blocksize / args->pagesize;
	int eccpoi = 0, eccpoi_start;
	unsigned int i;
	int badpos = bad_marker_offs_in_oob(args->pagesize);

	switch (args->pagesize) {
	case 2048: eccpoi_start = 64 / 2; break;
	case 512:  eccpoi_start = 16 / 2; break;
	default:   exit(EXIT_FAILURE);
	}

	if (!buf)
		exit(EXIT_FAILURE);
	if (!oob)
		exit(EXIT_FAILURE);
	if (!calc_oob)
		exit(EXIT_FAILURE);
	if (!calc_buf)
		exit(EXIT_FAILURE);

	while (!feof(in_fp)) {
		/* read page by page */
		read = fread(buf, 1, args->pagesize, in_fp);
		if (ferror(in_fp)) {
			fprintf(stderr, "I/O Error.");
			exit(EXIT_FAILURE);
		}
		if (read != (ssize_t)args->pagesize)
			break;

		read = fread(oob, 1, oobsize, in_fp);
		if (ferror(in_fp)) {
			fprintf(stderr, "I/O Error.");
			exit(EXIT_FAILURE);
		}

		page_buf = buf;	/* default is unmodified data */

		if ((page == 0 || page == 1) && (oob[badpos] != 0xff)) {
			if (verbose)
				printf("Block %d is bad\n",
				       page / pages_per_block);
			goto write_data;
		}
		if (args->correct_ecc)
			page_buf = calc_buf;

		process_page(buf, calc_oob, args->pagesize);
		memcpy(calc_buf, buf, args->pagesize);

		/*
		 * Our oob format uses only the last 3 bytes out of 4.
		 * The first byte is 0x00 when the ECC is generated by
		 * our toolset and 0xff when generated by Linux. This
		 * is to be fixed when we want nand2bin work for other
		 * ECC layouts too.
		 */
		for (i = 0, eccpoi = eccpoi_start; i < args->pagesize;
		     i += 256, eccpoi += 4)
			oob[eccpoi] = calc_oob[eccpoi] = 0xff;

		if (verbose && memcmp(oob, calc_oob, oobsize) != 0) {
			printf("\nECC compare mismatch found at block %d page %d!\n",
			       page / pages_per_block, page % pages_per_block);

			printf("Read out OOB Data:\n");
			hexdump(stdout, oob, oobsize);

			printf("Calculated OOB Data:\n");
			hexdump(stdout, calc_oob, oobsize);
		}

		/* Do correction on subpage base */
		for (i = 0, eccpoi = eccpoi_start; i < args->pagesize;
		     i += 256, eccpoi += 4) {
			rc = nand_correct_data(calc_buf + i, &oob[eccpoi + 1],
					       &calc_oob[eccpoi + 1]);

			if (rc == -1)
				fprintf(stdout, "Uncorrectable ECC error at "
					"block %d page %d/%d\n",
					page / pages_per_block,
					page % pages_per_block, i / 256);
			else if (rc > 0)
				fprintf(stdout, "Correctable ECC error at "
					"block %d page %d/%d\n",
					page / pages_per_block,
					page % pages_per_block, i / 256);
		}

	write_data:
		rc = fwrite(page_buf, 1, args->pagesize, bin_fp);
		if (ferror(bin_fp)) {
			fprintf(stderr, "I/O Error.");
			exit(EXIT_FAILURE);
		}
		rc = fwrite(oob, 1, oobsize, oob_fp);
		if (ferror(bin_fp)) {
			fprintf(stderr, "I/O Error.");
			exit(EXIT_FAILURE);
		}

		page++;
	}
	free(calc_buf);
	free(calc_oob);
	free(oob);
	free(buf);
	return 0;
}

static int split_blocks(struct args *args, FILE *in_fp)
{
	uint8_t *buf;
	size_t oobsize = calc_oobsize(args->pagesize);
	int pages_per_block = args->blocksize / args->pagesize;
	int block_len = pages_per_block * (args->pagesize + oobsize);
	int blocks = args->in_len / block_len;
	char bname[256] = { 0, };
	int badpos = bad_marker_offs_in_oob(args->pagesize);
	int bad_blocks = 0, i, bad_block = 0;
	ssize_t rc;
	FILE *b;

	buf = malloc(block_len);
	if (!buf) {
		perror("Not enough memory");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < blocks; i++) {
		rc = fread(buf, 1, block_len, in_fp);
		if (rc != block_len) {
			fprintf(stderr, "cannot read enough data!\n");
			exit(EXIT_FAILURE);
		}

		/* do block analysis */
		bad_block = 0;
		if ((buf[args->pagesize + badpos] != 0xff) ||
		    (buf[2 * args->pagesize + oobsize + badpos] != 0xff)) {
			bad_blocks++;
			bad_block = 1;
		}
		if ((verbose && bad_block) || (verbose > 1)) {
			printf("-- (block %d oob of page 0 and 1)\n", i);
			hexdump(stdout, buf + args->pagesize, oobsize);
			printf("--\n");
			hexdump(stdout, buf + 2 * args->pagesize +
				oobsize, oobsize);
		}

		/* write complete block out */
		snprintf(bname, sizeof(bname) - 1, "%s.%d", args->arg1, i);
		b = fopen(bname, "w+");
		if (!b) {
			perror("Cannot open file");
			exit(EXIT_FAILURE);
		}
		rc = fwrite(buf, 1, block_len, b);
		if (rc != block_len) {
			fprintf(stderr, "could not write all data!\n");
			exit(EXIT_FAILURE);
		}
		fclose(b);
	}

	free(buf);
	if (bad_blocks || verbose)
		fprintf(stderr, "%d blocks, %d bad blocks\n",
			blocks, bad_blocks);
	return 0;
}

int
main(int argc, char *argv[])
{
	FILE *in, *bin = NULL, *oob = NULL;
	struct stat file_info;

	parse_opt(argc, argv, &myargs);

	if (!myargs.arg1) {
		fprintf(stderr, "Please specify input file!\n");
		exit(EXIT_FAILURE);
	}

	if (lstat(myargs.arg1, &file_info) != 0) {
		perror("Cannot fetch file size from input file.\n");
		exit(EXIT_FAILURE);
	}
	myargs.in_len = file_info.st_size;

	in = fopen(myargs.arg1, "r");
	if (!in) {
		perror("Cannot open file");
		exit(EXIT_FAILURE);
	}

	if (myargs.split_blocks) {
		split_blocks(&myargs, in);
		goto out;
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
	decompose_image(&myargs, in, bin, oob);

 out:
	if (in)	 fclose(in);
	if (bin) fclose(bin);
	if (oob) fclose(oob);
	exit(EXIT_SUCCESS);
}
