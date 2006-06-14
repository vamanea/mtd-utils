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
 * An utility to decompose UBI images. Not yet finished ...
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

#include "crc32.h"
#include <mtd/ubi-header.h>

#define MAXPATH		1024
#define MIN(x,y)	((x)<(y)?(x):(y))

static uint32_t crc32_table[256];

struct args {
	const char *output_dir;
	uint32_t hdr_offs;
	uint32_t data_offs;
	uint32_t blocksize;

	/* special stuff needed to get additional arguments */
	char *arg1;
	char **options;		/* [STRING...] */
};

static struct args myargs = {
	.output_dir = "unubi",
	.hdr_offs = 64,
	.data_offs = 128,
	.blocksize = 128 * 1024,
	.arg1 = NULL,
	.options = NULL,
};

static error_t parse_opt (int key, char *arg, struct argp_state *state);

const char *argp_program_bug_address = "<haver@vnet.ibm.com>";

static char doc[] = "\nVersion: " VERSION "\n\t"
	HOST_OS" "HOST_CPU" at "__DATE__" "__TIME__"\n"
	"\nWrite to UBI Volume.\n";

static struct argp_option options[] = {
	{ .name = "dir",
	  .key = 'd',
	  .arg = "<output-dir>",
	  .flags = 0,
	  .doc = "output directory",
	  .group = OPTION_ARG_OPTIONAL },

	{ .name = "blocksize",
	  .key = 'b',
	  .arg = "<blocksize>",
	  .flags = 0,
	  .doc = "blocksize",
	  .group = OPTION_ARG_OPTIONAL },

	{ .name = "data-offs",
	  .key = 'x',
	  .arg = "<data-offs>",
	  .flags = 0,
	  .doc = "data offset",
	  .group = OPTION_ARG_OPTIONAL },

	{ .name = "hdr-offs",
	  .key = 'x',
	  .arg = "<hdr-offs>",
	  .flags = 0,
	  .doc = "hdr offset",
	  .group = OPTION_ARG_OPTIONAL },

	{ .name = NULL, .key = 0, .arg = NULL, .flags = 0,
	  .doc = NULL, .group = 0 },
};

static struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.args_doc = "image-file",
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
	case 'b': /* --blocksize<blocksize> */
		args->blocksize = str_to_num(arg);
		break;

	case 'x': /* --data-offs=<data-offs> */
		args->data_offs = str_to_num(arg);
		break;

	case 'y': /* --hdr-offs=<hdr-offs> */
		args->hdr_offs = str_to_num(arg);
		break;

	case 'd': /* --dir=<output-dir> */
		args->output_dir = arg;
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
                   of the arguments, we can force argp to stop parsing
                   here and return. */

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

static inline void
hexdump(FILE *fp, const void *p, size_t size)
{
	int k;
	const uint8_t *buf = p;

	for (k = 0; k < size; k++) {
		fprintf(fp, "%02x ", buf[k]);
		if ((k & 15) == 15)
			fprintf(fp, "\n");
	}
}

/*
 * This was put together in 1.5 hours and this is exactly how it looks
 * like! FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * FIXME FIXME FIXME FIXME FIXME FIXME!
 */
static int extract_volume(struct args *args, const uint8_t *buf,
			  int len, int volume, FILE *fp)
{
	int i, rc;
	int nrblocks = len / args->blocksize;
	int max_lnum = -1, lnum = 0;
	const uint8_t *ptr;
	uint8_t **vol_tab;
	int *vol_len;

	vol_tab = calloc(nrblocks, sizeof(uint8_t *));
	vol_len = calloc(nrblocks, sizeof(int));

	if (!buf || !vol_tab || !vol_len)
		exit(EXIT_FAILURE);

	for (i = 0, ptr = buf; i < nrblocks; i++, ptr += args->blocksize) {
		uint32_t crc;
		struct ubi_ec_hdr *ec_hdr = (struct ubi_ec_hdr *)ptr;
		struct ubi_vid_hdr *vid_hdr = NULL;
		uint8_t *data;

		/* default */
		vol_len[lnum] = args->blocksize - (2 * 1024);

		/* Check UBI EC header */
		crc = clc_crc32(crc32_table, UBI_CRC32_INIT, ec_hdr,
				UBI_EC_HDR_SIZE_CRC);
		if (crc != ubi32_to_cpu(ec_hdr->hdr_crc))
			continue;

		vid_hdr = (struct ubi_vid_hdr *)
			(ptr + ubi32_to_cpu(ec_hdr->vid_hdr_offset));
		data = (uint8_t *)(ptr + ubi32_to_cpu(ec_hdr->data_offset));

		crc = clc_crc32(crc32_table, UBI_CRC32_INIT, vid_hdr,
				UBI_VID_HDR_SIZE_CRC);
		if (crc != ubi32_to_cpu(vid_hdr->hdr_crc))
			continue;

		if (volume == ubi32_to_cpu(vid_hdr->vol_id)) {

			printf("****** block %4d volume %2d **********\n",
			       i, volume);

			hexdump(stdout, ptr, 64);

			printf("--- vid_hdr\n");
			hexdump(stdout, vid_hdr, 64);

			printf("--- data\n");
			hexdump(stdout, data, 64);

			lnum = ubi32_to_cpu(vid_hdr->lnum);
			vol_tab[lnum] = data;
			if (max_lnum < lnum)
				max_lnum = lnum;
			if (vid_hdr->vol_type == UBI_VID_STATIC)
				vol_len[lnum] =
					ubi32_to_cpu(vid_hdr->data_size);

		}
	}

	for (lnum = 0; lnum <= max_lnum; lnum++) {
		if (vol_tab[lnum]) {
			rc = fwrite(vol_tab[lnum], 1, vol_len[lnum], fp);
			if (ferror(fp) || (vol_len[lnum] != rc)) {
				perror("could not write file");
				exit(EXIT_FAILURE);
			}
		} else {
			/* Fill up empty areas by 0xff, for static
			 * volumes this means they are broken!
			 */
			for (i = 0; i < vol_len[lnum]; i++) {
				if (fputc(0xff, fp) == EOF) {
					perror("could not write char");
					exit(EXIT_FAILURE);
				}
			}
		}
	}

	free(vol_tab);
	free(vol_len);
	return 0;
}

int
main(int argc, char *argv[])
{
	int len, rc;
	FILE *fp;
	struct stat file_info;
	uint8_t *buf;
	int i;

	init_crc32_table(crc32_table);

	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &myargs);

	if (!myargs.arg1) {
		fprintf(stderr, "Please specify input file!\n");
		exit(EXIT_FAILURE);
	}

	fp = fopen(myargs.arg1, "r");
	if (!fp) {
		perror("Cannot open file");
		exit(EXIT_FAILURE);
	}
	if (fstat(fileno(fp), &file_info) != 0) {
		fprintf(stderr, "Cannot fetch file size "
			"from input file.\n");
	}
	len = file_info.st_size;
	buf = malloc(len);
	if (!buf) {
		perror("out of memory!");
		exit(EXIT_FAILURE);
	}
	rc = fread(buf, 1, len, fp);
	if (ferror(fp) || (len != rc)) {
		perror("could not read file");
		exit(EXIT_FAILURE);
	}
	if (!myargs.output_dir) {
		fprintf(stderr, "No output directory specified!\n");
		exit(EXIT_FAILURE);
	}

	rc = mkdir(myargs.output_dir, 0777);
	if (rc && errno != EEXIST) {
		perror("Cannot create output directory");
		exit(EXIT_FAILURE);
	}
	for (i = 0; i < 32; i++) {
		char fname[1024];
		FILE *fpout;

		printf("######### VOLUME %d ############################\n",
		       i);

		sprintf(fname, "%s/ubivol_%d.bin", myargs.output_dir, i);
		fpout = fopen(fname, "w+");
		if (!fpout) {
			perror("Cannot open file");
			exit(EXIT_FAILURE);
		}
		extract_volume(&myargs, buf, len, i, fpout);
		fclose(fpout);
	}

	fclose(fp);
	free(buf);
	exit(EXIT_SUCCESS);
}
