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
 * 1.7 Made NAND ECC layout configurable, the holes which were previously
 *     filled with 0x00 are untouched now and will be 0xff just like MTD
 *     behaves when writing the oob (haver)
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
#include "ecclayouts.h"

#define PROGRAM_VERSION "1.7"

#define ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))
#define MAXPATH		1024
#define MIN(x,y)	((x)<(y)?(x):(y))

struct args {
	const char *oob_file;
	const char *output_file;
	size_t pagesize;
	size_t oobsize;
	int bad_marker_offs_in_oob;
	size_t blocksize;
	int split_blocks;
	size_t in_len;		/* size of input file */
	int correct_ecc;
	struct nand_ecclayout *nand_oob;

	/* special stuff needed to get additional arguments */
	char *arg1;
	char **options;		/* [STRING...] */
};

static struct args myargs = {
	.output_file = "data.bin",
	.oob_file = "oob.bin",
	.pagesize = 2048,
	.blocksize = 128 * 1024,
	.nand_oob = &ibm_nand_oob_64,
	.in_len = 0,
	.split_blocks = 0,
	.correct_ecc = 0,
	.arg1 = NULL,
	.options = NULL,
};

static char doc[] = "\nVersion: " PROGRAM_VERSION "\n"
	"nand2bin - split data and OOB.\n";

static const char *optionsstr =
"  -l, --ecc-placement=<MTD,IBM> OOB placement scheme (default is IBM).\n"
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
	{ .name = "ecc-layout", .has_arg = 1, .flag = NULL, .val = 'l' },
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
	unsigned int i, oob_idx = 0;
	const char *ecc_layout = NULL;

	while (1) {
		int key;

		key = getopt_long(argc, argv, "b:el:o:O:p:sv?", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'b': /* --blocksize<blocksize> */
			args->blocksize = str_to_num(optarg);
			break;
		case 'e': /* --correct-ecc */
			args->correct_ecc = 1;
			break;
		case 'l': /* --ecc-layout=<...> */
			ecc_layout = optarg;
			break;
		case 'o': /* --output=<output.bin> */
			args->output_file = optarg;
			break;
		case 'O': /* --oob=<oob.bin> */
			args->oob_file = optarg;
			break;
		case 'p': /* --pagesize<pagesize> */
			args->pagesize = str_to_num(optarg);
			break;
		case 's': /* --split-blocks */
			args->split_blocks = 1;
			break;
		case 'v': /* --verbose */
			verbose++;
			break;
		case 'V':
			printf("%s\n", PROGRAM_VERSION);
			exit(0);
			break;
		case '?': /* help */
			printf("Usage: nand2bin [OPTION...] input.mif\n");
			printf("%s%s", doc, optionsstr);
			printf("\nReport bugs to %s\n",
			       PACKAGE_BUGREPORT);
			exit(0);
			break;
		default:
			printf("%s", usage);
			exit(-1);
		}
	}

	if (optind < argc)
		args->arg1 = argv[optind++];

	switch (args->pagesize) {
	case 512:
		args->oobsize = 16;
		args->bad_marker_offs_in_oob = 5;
		oob_idx = 0;
		break;
	case 2048:
		args->oobsize = 64;
		args->bad_marker_offs_in_oob = 0;
		oob_idx = 1;
		break;
	default:
		fprintf(stderr, "Unsupported page size: %d\n", args->pagesize);
		return -EINVAL;
	}

	/* Figure out correct oob layout if it differs from default */
	if (ecc_layout) {
		for (i = 0; i < ARRAY_SIZE(oob_placement); i++)
			if (strcmp(ecc_layout, oob_placement[i].name) == 0)
				args->nand_oob =
					oob_placement[i].nand_oob[oob_idx];
	}
	return 0;
}

/*
 * We must only compare the relevant bytes in the OOB area. All other
 * bytes can be ignored. The information we need to do this is in
 * nand_oob.
 */
static int oob_cmp(struct nand_ecclayout *nand_oob, uint8_t *oob,
		   uint8_t *calc_oob)
{
	unsigned int i;
	for (i = 0; i < nand_oob->eccbytes; i++)
		if (oob[nand_oob->eccpos[i]] != calc_oob[nand_oob->eccpos[i]])
			return 1;
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

static int process_page(struct args *args, uint8_t *buf, uint8_t *oobbuf)
{
	size_t i, j;
	int eccpoi;
	uint8_t ecc_code[3] = { 0, }; /* temp */

	/* Calculate ECC */
	memset(oobbuf, 0xff, args->oobsize);
	for (eccpoi = 0, i = 0; i < args->pagesize; i += 256, eccpoi += 3) {
		nand_calculate_ecc(&buf[i], ecc_code);
		for (j = 0; j < 3; j++)
			oobbuf[args->nand_oob->eccpos[eccpoi + j]] = ecc_code[j];
	}
	return 0;
}

static int decompose_image(struct args *args, FILE *in_fp,
			   FILE *bin_fp, FILE *oob_fp)
{
	unsigned int i, eccpoi;
	int read, rc, page = 0;
	uint8_t *buf = malloc(args->pagesize);
	uint8_t *oob = malloc(args->oobsize);
	uint8_t *calc_oob = malloc(args->oobsize);
	uint8_t *calc_buf = malloc(args->pagesize);
	uint8_t *page_buf;
	int pages_per_block = args->blocksize / args->pagesize;
	int badpos = args->bad_marker_offs_in_oob;
	uint8_t ecc_code[3] = { 0, }; /* temp */
	uint8_t calc_ecc_code[3] = { 0, }; /* temp */

	if (!buf || !oob || !calc_oob || !calc_buf)
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

		read = fread(oob, 1, args->oobsize, in_fp);
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

		process_page(args, buf, calc_oob);
		memcpy(calc_buf, buf, args->pagesize);

		if (verbose && oob_cmp(args->nand_oob, oob, calc_oob) != 0) {
			printf("\nECC compare mismatch found at block %d page %d!\n",
			       page / pages_per_block, page % pages_per_block);

			printf("Read out OOB Data:\n");
			hexdump(stdout, oob, args->oobsize);

			printf("Calculated OOB Data:\n");
			hexdump(stdout, calc_oob, args->oobsize);
		}

		/* Do correction on subpage base */
		for (i = 0, eccpoi = 0; i < args->pagesize; i += 256, eccpoi += 3) {
			int j;

			for (j = 0; j < 3; j++) {
				ecc_code[j] = oob[args->nand_oob->eccpos[eccpoi + j]];
				calc_ecc_code[j] =
					calc_oob[args->nand_oob->eccpos[eccpoi + j]];
			}
			rc = nand_correct_data(calc_buf + i, ecc_code,
					       calc_ecc_code);
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
		rc = fwrite(oob, 1, args->oobsize, oob_fp);
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
	int pages_per_block = args->blocksize / args->pagesize;
	int block_len = pages_per_block * (args->pagesize + args->oobsize);
	int blocks = args->in_len / block_len;
	char bname[256] = { 0, };
	int badpos = args->bad_marker_offs_in_oob;
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
		    (buf[2 * args->pagesize + args->oobsize + badpos] != 0xff)) {
			bad_blocks++;
			bad_block = 1;
		}
		if ((verbose && bad_block) || (verbose > 1)) {
			printf("-- (block %d oob of page 0 and 1)\n", i);
			hexdump(stdout, buf + args->pagesize, args->oobsize);
			printf("--\n");
			hexdump(stdout, buf + 2 * args->pagesize +
				args->oobsize, args->oobsize);
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
