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
 */

/*
 * Print out information about the UBI table this IPL is using.  This
 * can be used afterwards to analyze misbehavior of the IPL code.  The
 * input this program requires is the last 1 MiB DDRAM of our system
 * where the scanning table is placed into.
 *
 * Author: Frank Haverkamp <haver@vnet.ibm.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <argp.h>
#include <getopt.h>
#include <stdint.h>
#include <sys/time.h>
#include <netinet/in.h>

#define __unused __attribute__((unused))

/* This should hopefully be constant and the same in all
 * configurations.
 */
#define CFG_IPLSIZE	512
#define CFG_SPLCODE	512
#define MEMTOP		0x06600000 /* Sunray 102 MiB */
#define MEMSIZE		0x00100000 /* 1 MiB */
#define CODE_SIZE	(64 * 1024)

/* FIXME Except of the memory size this should be defined via
 * parameters
 *
 * CFG_MEMTOP_BAMBOO  0x02000000
 * CFG_MEMTOP_SUNRAY  0x06600000
 */

#include "ubiipl.h"
#include "ubiflash.h"

#define MIN(x,y) ((x)<(y)?(x):(y))

#define ERR_RET(rc) {						   \
		fprintf(stderr, "%s:%d failed rc=%d\n", __func__,	\
			__LINE__, (rc));				\
		return (rc);					    \
	}

#define VERSION "1.3"

static error_t parse_opt (int key, char *arg, struct argp_state *state);
const char *argp_program_version = VERSION;
const char *argp_program_bug_address = "<haver@vnet.ibm.com>";

static char doc[] = "\nVersion: " VERSION "\n\t"
	" at "__DATE__" "__TIME__"\n"
	"\n"
	"Test program\n";

static struct argp_option options[] = {
	/* common settings */
	{ .name = "verbose",
	  .key = 'v',
	  .arg = "<level>",
	  .flags = 0,
	  .doc = "Set verbosity level to <level>",
	  .group = OPTION_ARG_OPTIONAL },

	{ .name = "memtop",
	  .key = 'm',
	  .arg = "<memtop>",
	  .flags = 0,
	  .doc = "Set top of memory, 102 MiB for Sunray and 16 MiB for Bamboo",
	  .group = OPTION_ARG_OPTIONAL },

	{ .name = NULL,
	  .key = 0,
	  .arg = NULL,
	  .flags = 0,
	  .doc = NULL,
	  .group = 0 },
};

typedef struct test_args {
	int verbose;
	unsigned long memtop;
	char *arg1;
	char **options;
} test_args;

static struct test_args g_args = {
	.memtop = MEMTOP,
	.verbose = 0,
	.arg1 = NULL,
	.options = NULL,
};

static struct argp argp = {
	options:     options,
	parser:	     parse_opt,
	args_doc:    "[last_1MiB_memory.bin]",
	doc:	 doc,
	children:    NULL,
	help_filter: NULL,
	argp_domain: NULL,
};

static int verbose = 0;

/**
 * @brief Parse the arguments passed into the test case.
 *
 * @param key	    The parameter.
 * @param arg	    Argument passed to parameter.
 * @param state	  Location to put information on parameters.
 *
 * @return error_t
 */
static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
	/* Get the `input' argument from `argp_parse', which we
	   know is a pointer to our arguments structure. */
	test_args *args = state->input;

	switch (key) {
		/* common settings */
	case 'v': /* --verbose=<level> */
		verbose = args->verbose = strtoul(arg, (char **)NULL, 0);
		break;

	case 'm': /* --memtop */
		args->memtop = strtoul(arg, (char **)NULL, 0);
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
		/* print out message if no arguments are given but PFI
		   write should be done */
		break;

	default:
		return(ARGP_ERR_UNKNOWN);
	}
	return 0;
}

static void
hexdump(const char *buf, int len)
{
	char line[16];
	char str[256];
	char dummy[256];
	int j = 0;

	while (len > 0) {
		int i;

		strcpy(str, " ");

		for (j = 0; j < MIN(16, len); j++)
			line[j] = *buf++;

		for (i = 0; i < j; i++) {
			if (!(i & 3)) {
				sprintf(dummy, " %.2x", line[i] & 0xff);
				strcat(str, dummy);
			} else {
				sprintf(dummy, "%.2x", line[i] & 0xff);
				strcat(str, dummy);
			}
		}

		/* Print empty space */
		for (; i < 16; i++)
			if (!(i & 1))
				strcat(str, "	");
			else
				strcat(str, "  ");

		strcat(str, "  ");
		for (i = 0; i < j; i++) {
			if (isprint(line[i])) {
				sprintf(dummy, "%c", line[i]);
				strcat(str, dummy);
			} else {
				strcat(str, ".");
			}
		}
		printf("%s\n", str);
		len -= 16;
	}
}

static void
print_status_help(void)
{
	printf("Error Codes from IPL\n");
	printf("   IO Error	     %d\n", STAT_IO_FAILED);
	printf("   Block is bad	     %d\n", STAT_BLOCK_BAD);
	printf("   ECC unrec error   %d\n", STAT_ECC_ERROR);
	printf("   CRC csum failed   %d\n", STAT_CRC_ERROR);
	printf("   Magic not avail   %d\n", STAT_NO_MAGIC);
	printf("   No image avail    %d\n", STAT_NO_IMAGE);
	printf("   Image is invalid  %d\n", STAT_INVALID_IMAGE);
	printf("   Image is defect   %d\n\n", STAT_DEFECT_IMAGE);

}

static void
print_ubi_scan_info(struct ubi_scan_info *fi)
{
	int i;

	printf("ubi_scan_info\n");
	printf("    version	 %08x\n", ntohl(fi->version));
	printf("    bootstatus	 %08x\n", ntohl(fi->bootstatus));
	printf("    flashtype	 %08x\n", ntohl(fi->flashtype));
	printf("    flashid	 %08x\n", ntohl(fi->flashid));
	printf("    flashmfgr	 %08x\n", ntohl(fi->flashmfr));
	printf("    flashsize	 %d bytes (%dM)\n",
	       ntohl(fi->flashsize), ntohl(fi->flashsize) / (1024 * 1024));
	printf("    blocksize	 %d bytes\n", ntohl(fi->blocksize));
	printf("    blockshift	 %d\n", ntohl(fi->blockshift));
	printf("    nrblocks	 %d\n", ntohl(fi->nrblocks));
	printf("    pagesize	 %d\n", ntohl(fi->pagesize));
	printf("    imagelen	 %d\n", ntohl(fi->imagelen));
	printf("    blockinfo	 %08x\n", ntohl((int)fi->blockinfo));

	printf("  nr	imageblocks  imageoffs\n");
	for (i = 0; i < UBI_BLOCK_IDENT_MAX; i++)
		printf("    [%2d]   %08x   %08x\n", i,
		       ntohl(fi->imageblocks[i]),
		       ntohl(fi->imageoffs[i]));

	for (i = 0; i < UBI_TIMESTAMPS; i++) {
		if (!ntohl(fi->times[i]))
			continue;
		printf("time[%3d] = %08x %.3f sec\n", i, ntohl(fi->times[i]),
		       (double)ntohl(fi->times[i]) / 500000000.0);
	}

	printf("crc32_table\n");
	hexdump((char *)&fi->crc32_table, sizeof(fi->crc32_table));
	printf("\npage_buf\n");
	hexdump((char *)&fi->page_buf, sizeof(fi->page_buf));

	printf("\n");

}

static void
print_ubi_block_info(struct ubi_scan_info *fi,
		     struct ubi_vid_hdr *bi, int nr)
{
	int i;
	int unknown = 0;

	printf("\nBINFO\n");

	for (i = 0; i < nr; i++) {
		if ((int)ubi32_to_cpu(bi[i].magic) != UBI_VID_HDR_MAGIC) {
			printf("block=%d %08x\n",
			       i, i * ntohl(fi->blocksize));
#if 0
			printf(".");
			if ((unknown & 0x3f) == 0x3f)
				printf("\n");
			unknown++;
#else
			hexdump((char *)&bi[i],
				sizeof(struct ubi_vid_hdr));
#endif
		} else {
			if (unknown)
				printf("\n");
			printf("block=%d %08x\n"
			       "    leb_ver=0x%x data_size=%d "
			       "lnum=%d used_ebs=0x%x\n"
			       "    data_crc=%08x hdr_crc=%08x\n",
			       i, i * ntohl(fi->blocksize),
			       ubi32_to_cpu(bi[i].leb_ver),
			       ubi32_to_cpu(bi[i].data_size),
			       ubi32_to_cpu(bi[i].lnum),
			       ubi32_to_cpu(bi[i].used_ebs),
			       ubi32_to_cpu(bi[i].data_crc),
			       ubi32_to_cpu(bi[i].hdr_crc));
			hexdump((char *)&bi[i],
				sizeof(struct ubi_vid_hdr));
			unknown = 0;
		}
	}
}

static int do_read(unsigned int memtop, char *buf, int buf_len __unused)
{
	unsigned long finfo_addr;
	unsigned long binfo_addr;
	unsigned long images_addr;
	unsigned long nrblocks;
	unsigned long bi_size;
	unsigned long images_size;
	struct ubi_scan_info *fi;
	struct ubi_vid_hdr *bi;
	char *images;
	unsigned long memaddr = memtop - MEMSIZE;

	print_status_help();

	/* Read and print FINFO */
	finfo_addr = MEMSIZE - CFG_IPLSIZE * 1024;

	printf("read info at addr %08lx\n", finfo_addr);
	fi = (struct ubi_scan_info *)(buf + finfo_addr);

	binfo_addr  = ntohl((unsigned long)fi->blockinfo) - memaddr;
	images_addr = ntohl((unsigned long)fi->images) - memaddr;
	nrblocks    = ntohl(fi->nrblocks);

	printf("BINFO %08lx\n", binfo_addr);

	bi_size = nrblocks * sizeof(struct ubi_vid_hdr);
	images_size = nrblocks * sizeof(unsigned int);

	printf("FINFO\n");
	print_ubi_scan_info(fi);
	/* hexdump((char *)fi, sizeof(*fi)); */

	/* Read and print BINFO */
	bi = (struct ubi_vid_hdr *)(buf + binfo_addr);
	print_ubi_block_info(fi, bi, nrblocks);

	/* Read and print IMAGES */
	images = buf + images_addr;
	printf("\nIMAGES\n");
	hexdump(images, images_size);

	return 0;
}

int main(int argc, char *argv[])
{
	char buf[MEMSIZE];
	FILE *fp;
	int rc;

	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &g_args);

	if (!g_args.arg1) {
		fprintf(stderr, "Please specify a file "
			"name for memory dump!\n");
		exit(EXIT_FAILURE);
	}

	memset(buf, 0xAB, sizeof(buf));

	fp = fopen(g_args.arg1, "r");
	if (!fp)
		exit(EXIT_FAILURE);
	rc = fread(buf, 1, sizeof(buf), fp);
	if (rc != sizeof(buf))
		exit(EXIT_FAILURE);
	fclose(fp);
	do_read(g_args.memtop, buf, sizeof(buf));

	exit(EXIT_SUCCESS);
}
