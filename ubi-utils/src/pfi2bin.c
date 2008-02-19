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
 * Convert a PFI file (partial flash image) into a plain binary file.
 * This tool can be used to prepare the data to be burned into flash
 * chips in a manufacturing step where the flashes are written before
 * being soldered onto the hardware. For NAND images another step is
 * required to add the right OOB data to the binary image.
 *
 * 1.3 Removed argp because we want to use uClibc.
 * 1.4 Minor cleanups
 */

#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <ubigen.h>
#include <mtd/ubi-header.h>
#include <mtd_swab.h>

#include "config.h"
#include "list.h"
#include "error.h"
#include "reader.h"
#include "peb.h"
#include "crc32.h"

#define PROGRAM_VERSION "1.4"

#define MAX_FNAME 255
#define DEFAULT_ERASE_COUNT  0 /* Hmmm.... Perhaps */
#define ERR_BUF_SIZE 1024

#define MIN(a,b) ((a) < (b) ? (a) : (b))

static uint32_t crc32_table[256];
static char err_buf[ERR_BUF_SIZE];

/*
 * Data used to buffer raw blocks which have to be
 * located at a specific point inside the generated RAW file
 */

typedef enum action_t {
	ACT_NOTHING   = 0x00000000,
	ACT_RAW	   = 0x00000001,
} action_t;

static const char copyright [] __attribute__((unused)) =
	"(c) Copyright IBM Corp 2006\n";

static char doc[] = "\nVersion: " PROGRAM_VERSION "\n"
	"pfi2bin - a tool to convert PFI files into binary images.\n";

static const char *optionsstr =
" Common settings:\n"
"  -c, --copyright\n"
"  -v, --verbose              Print more information.\n"
"\n"
" Input:\n"
"  -j, --platform=pdd-file    PDD information which contains the card settings.\n"
"\n"
" Output:\n"
"  -o, --output=filename      Outputfile, default: stdout.\n"
"\n"
"  -?, --help                 Give this help list\n"
"      --usage                Give a short usage message\n"
"  -V, --version              Print program version\n";

static const char *usage =
"Usage: pfi2bin [-cv?V] [-j pdd-file] [-o filename] [--copyright]\n"
"            [--verbose] [--platform=pdd-file] [--output=filename] [--help]\n"
"            [--usage] [--version] pfifile\n";

struct option long_options[] = {
	{ .name = "copyright", .has_arg = 0, .flag = NULL, .val = 'c' },
	{ .name = "verbose", .has_arg = 0, .flag = NULL, .val = 'v' },
	{ .name = "platform", .has_arg = 1, .flag = NULL, .val = 'j' },
	{ .name = "output", .has_arg = 1, .flag = NULL, .val = 'o' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = '?' },
	{ .name = "usage", .has_arg = 0, .flag = NULL, .val = 0 },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

typedef struct io {
	FILE* fp_pdd;	/* a FilePointer to the PDD data */
	FILE* fp_pfi;	/* a FilePointer to the PFI input stream */
	FILE* fp_out;	/* a FilePointer to the output stream */
} *io_t;

typedef struct myargs {
	/* common settings */
	action_t action;
	int verbose;
	const char *f_in_pfi;
	const char *f_in_pdd;
	const char *f_out;

	/* special stuff needed to get additional arguments */
	char *arg1;
	char **options;			/* [STRING...] */
} myargs;

static int
parse_opt(int argc, char **argv, myargs *args)
{
	while (1) {
		int key;

		key = getopt_long(argc, argv, "cvj:o:?V", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
			/* common settings */
			case 'v': /* --verbose=<level> */
				args->verbose = 1;
				break;

			case 'c': /* --copyright */
				fprintf(stderr, "%s\n", copyright);
				exit(0);
				break;

			case 'j': /* --platform */
				args->f_in_pdd = optarg;
				break;

			case 'o': /* --output */
				args->f_out = optarg;
				break;

			case '?': /* help */
				printf("pfi2bin [OPTION...] pfifile\n");
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
		args->f_in_pfi = argv[optind++];

	return 0;
}


static size_t
byte_to_blk(size_t byte, size_t blk_size)
{
	return	(byte % blk_size) == 0
		? byte / blk_size
		: byte / blk_size + 1;
}




/**
 * @precondition  IO: File stream points to first byte of RAW data.
 * @postcondition IO: File stream points to first byte of next
 *		      or EOF.
 */
static int
memorize_raw_eb(pfi_raw_t pfi_raw, pdd_data_t pdd, list_t *raw_pebs,
		io_t io)
{
	int rc = 0;
	uint32_t i;

	size_t read, to_read, eb_num;
	size_t bytes_left;
	list_t pebs = *raw_pebs;
	peb_t	peb  = NULL;

	long old_file_pos = ftell(io->fp_pfi);
	for (i = 0; i < pfi_raw->starts_size; i++) {
		bytes_left = pfi_raw->data_size;
		rc = fseek(io->fp_pfi, old_file_pos, SEEK_SET);
		if (rc != 0)
			goto err;

		eb_num = byte_to_blk(pfi_raw->starts[i], pdd->eb_size);
		while (bytes_left) {
			to_read = MIN(bytes_left, pdd->eb_size);
			rc = peb_new(eb_num++, pdd->eb_size, &peb);
			if (rc != 0)
				goto err;
			read = fread(peb->data, 1, to_read, io->fp_pfi);
			if (read != to_read) {
				rc = -EIO;
				goto err;
			}
			pebs = append_elem(peb, pebs);
			bytes_left -= read;
		}

	}
	*raw_pebs = pebs;
	return 0;
err:
	pebs = remove_all((free_func_t)&peb_free, pebs);
	return rc;
}

static int
convert_ubi_volume(pfi_ubi_t ubi, pdd_data_t pdd, list_t raw_pebs,
		struct ubi_vtbl_record *vol_tab,
		size_t *ebs_written, io_t io)
{
	int rc = 0;
	uint32_t i, j;
	peb_t raw_peb;
	peb_t cmp_peb;
	ubi_info_t u;
	size_t leb_total = 0;
	uint8_t vol_type;

	switch (ubi->type) {
	case pfi_ubi_static:
		vol_type = UBI_VID_STATIC; break;
	case pfi_ubi_dynamic:
		vol_type = UBI_VID_DYNAMIC; break;
	default:
		vol_type = UBI_VID_DYNAMIC;
	}

	rc = peb_new(0, 0, &cmp_peb);
	if (rc != 0)
		goto err;

	long old_file_pos = ftell(io->fp_pfi);
	for (i = 0; i < ubi->ids_size; i++) {
		rc = fseek(io->fp_pfi, old_file_pos, SEEK_SET);
		if (rc != 0)
			goto err;
		rc = ubigen_create(&u, ubi->ids[i], vol_type,
				   pdd->eb_size, DEFAULT_ERASE_COUNT,
				   ubi->alignment, UBI_VERSION,
				   pdd->vid_hdr_offset, 0, ubi->data_size,
				   io->fp_pfi, io->fp_out);
		if (rc != 0)
			goto err;

		rc = ubigen_get_leb_total(u, &leb_total);
		if (rc != 0)
			goto err;

		j = 0;
		while(j < leb_total) {
			cmp_peb->num = *ebs_written;
			raw_peb = is_in((cmp_func_t)peb_cmp, cmp_peb,
					raw_pebs);
			if (raw_peb) {
				rc = peb_write(io->fp_out, raw_peb);
			}
			else {
				rc = ubigen_write_leb(u, NO_ERROR);
				j++;
			}
			if (rc != 0)
				goto err;
			(*ebs_written)++;
		}
		/* memorize volume table entry */
		rc = ubigen_set_lvol_rec(u, ubi->size,
				ubi->names[i],
				(void*) &vol_tab[ubi->ids[i]]);
		if (rc != 0)
			goto err;
		ubigen_destroy(&u);
	}

	peb_free(&cmp_peb);
	return 0;

err:
	peb_free(&cmp_peb);
	ubigen_destroy(&u);
	return rc;
}


static FILE*
my_fmemopen (void *buf, size_t size, const char *opentype)
{
    FILE* f;
    size_t ret;

    assert(strcmp(opentype, "r") == 0);

    f = tmpfile();
    ret = fwrite(buf, 1, size, f);
    rewind(f);

    return f;
}

/**
 * @brief		Builds a UBI volume table from a volume entry list.
 * @return 0		On success.
 *	   else		Error.
 */
static int
write_ubi_volume_table(pdd_data_t pdd, list_t raw_pebs,
		struct ubi_vtbl_record *vol_tab, size_t vol_tab_size,
		size_t *ebs_written, io_t io)
{
	int rc = 0;
	ubi_info_t u;
	peb_t raw_peb;
	peb_t cmp_peb;
	size_t leb_size, leb_total, j = 0;
	uint8_t *ptr = NULL;
	FILE* fp_leb = NULL;
	int vt_slots;
	size_t vol_tab_size_limit;

	rc = peb_new(0, 0, &cmp_peb);
	if (rc != 0)
		goto err;

	/* @FIXME: Artem creates one volume with 2 LEBs.
	 * IMO 2 volumes would be more convenient. In order
	 * to get 2 reserved LEBs from ubigen, I have to
	 * introduce this stupid mechanism. Until no final
	 * decision of the VTAB structure is made... Good enough.
	 */
	rc = ubigen_create(&u, UBI_LAYOUT_VOLUME_ID, UBI_VID_DYNAMIC,
			   pdd->eb_size, DEFAULT_ERASE_COUNT,
			   1, UBI_VERSION,
			   pdd->vid_hdr_offset, UBI_COMPAT_REJECT,
			   vol_tab_size, stdin, io->fp_out);
			   /* @FIXME stdin for fp_in is a hack */
	if (rc != 0)
		goto err;
	rc = ubigen_get_leb_size(u, &leb_size);
	if (rc != 0)
		goto err;
	ubigen_destroy(&u);

	/*
	 * The number of supported volumes is restricted by the eraseblock size
	 * and by the UBI_MAX_VOLUMES constant.
	 */
	vt_slots = leb_size / UBI_VTBL_RECORD_SIZE;
	if (vt_slots > UBI_MAX_VOLUMES)
		vt_slots = UBI_MAX_VOLUMES;
	vol_tab_size_limit = vt_slots * UBI_VTBL_RECORD_SIZE;

	ptr = (uint8_t*) malloc(leb_size * sizeof(uint8_t));
	if (ptr == NULL)
		goto err;

	memset(ptr, 0xff, leb_size);
	memcpy(ptr, vol_tab, vol_tab_size_limit);
	fp_leb = my_fmemopen(ptr, leb_size, "r");

	rc = ubigen_create(&u, UBI_LAYOUT_VOLUME_ID, UBI_VID_DYNAMIC,
			   pdd->eb_size, DEFAULT_ERASE_COUNT,
			   1, UBI_VERSION, pdd->vid_hdr_offset,
			   UBI_COMPAT_REJECT, leb_size * UBI_LAYOUT_VOLUME_EBS,
			   fp_leb, io->fp_out);
	if (rc != 0)
		goto err;
	rc = ubigen_get_leb_total(u, &leb_total);
	if (rc != 0)
		goto err;

	long old_file_pos = ftell(fp_leb);
	while(j < leb_total) {
		rc = fseek(fp_leb, old_file_pos, SEEK_SET);
		if (rc != 0)
			goto err;

		cmp_peb->num = *ebs_written;
		raw_peb = is_in((cmp_func_t)peb_cmp, cmp_peb,
				raw_pebs);
		if (raw_peb) {
			rc = peb_write(io->fp_out, raw_peb);
		}
		else {
			rc = ubigen_write_leb(u, NO_ERROR);
			j++;
		}

		if (rc != 0)
			goto err;
		(*ebs_written)++;
	}

err:
	free(ptr);
	peb_free(&cmp_peb);
	ubigen_destroy(&u);
	fclose(fp_leb);
	return rc;
}

static int
write_remaining_raw_ebs(pdd_data_t pdd, list_t raw_blocks, size_t *ebs_written,
			FILE* fp_out)
{
	int rc = 0;
	uint32_t j, delta;
	list_t ptr;
	peb_t empty_eb, peb;

	/* create an empty 0xff EB (for padding) */
	rc = peb_new(0, pdd->eb_size, &empty_eb);

	foreach(peb, ptr, raw_blocks) {
		if (peb->num < *ebs_written) {
			continue; /* omit blocks which
				     are already passed */
		}

		if (peb->num < *ebs_written) {
			err_msg("eb_num: %d\n", peb->num);
			err_msg("Bug: This should never happen. %d %s",
				__LINE__, __FILE__);
			goto err;
		}

		delta = peb->num - *ebs_written;
		if (((delta + *ebs_written) * pdd->eb_size) > pdd->flash_size) {
			err_msg("RAW block outside of flash_size.");
			goto err;
		}
		for (j = 0; j < delta; j++) {
			rc = peb_write(fp_out, empty_eb);
			if (rc != 0)
				goto err;
			(*ebs_written)++;
		}
		rc = peb_write(fp_out, peb);
		if (rc != 0)
			goto err;
		(*ebs_written)++;
	}

err:
	peb_free(&empty_eb);
	return rc;
}

static int
init_vol_tab(struct ubi_vtbl_record **vol_tab, size_t *vol_tab_size)
{
	uint32_t crc;
	size_t i;
	struct ubi_vtbl_record* res = NULL;

	*vol_tab_size = UBI_MAX_VOLUMES * UBI_VTBL_RECORD_SIZE;

	res = (struct ubi_vtbl_record*) calloc(1, *vol_tab_size);
	if (vol_tab == NULL) {
		return -ENOMEM;
	}

	for (i = 0; i < UBI_MAX_VOLUMES; i++) {
		crc = clc_crc32(crc32_table, UBI_CRC32_INIT,
			&(res[i]), UBI_VTBL_RECORD_SIZE_CRC);
		res[i].crc = cpu_to_be32(crc);
	}

	*vol_tab = res;
	return 0;
}

static int
create_raw(io_t io)
{
	int rc = 0;
	size_t ebs_written = 0; /* eraseblocks written already... */
	size_t vol_tab_size;
	list_t ptr;

	list_t pfi_raws = mk_empty(); /* list of raw sections from a pfi */
	list_t pfi_ubis = mk_empty(); /* list of ubi sections from a pfi */
	list_t raw_pebs	 = mk_empty(); /* list of raw eraseblocks */

	struct ubi_vtbl_record *vol_tab = NULL;
	pdd_data_t pdd = NULL;

	rc = init_vol_tab (&vol_tab, &vol_tab_size);
	if (rc != 0) {
		err_msg("Cannot initialize volume table.");
		goto err;
	}

	rc = read_pdd_data(io->fp_pdd, &pdd,
			err_buf, ERR_BUF_SIZE);
	if (rc != 0) {
		err_msg("Cannot read necessary pdd_data: %s rc: %d",
				err_buf, rc);
		goto err;
	}

	rc = read_pfi_headers(&pfi_raws, &pfi_ubis, io->fp_pfi,
			err_buf, ERR_BUF_SIZE);
	if (rc != 0) {
		err_msg("Cannot read pfi header: %s rc: %d",
				err_buf, rc);
		goto err;
	}

	pfi_raw_t pfi_raw;
	foreach(pfi_raw, ptr, pfi_raws) {
		rc = memorize_raw_eb(pfi_raw, pdd, &raw_pebs,
			io);
		if (rc != 0) {
			err_msg("Cannot create raw_block in mem. rc: %d\n",
				rc);
			goto err;
		}
	}

	pfi_ubi_t pfi_ubi;
	foreach(pfi_ubi, ptr, pfi_ubis) {
		rc = convert_ubi_volume(pfi_ubi, pdd, raw_pebs,
					vol_tab, &ebs_written, io);
		if (rc != 0) {
			err_msg("Cannot convert UBI volume. rc: %d\n", rc);
			goto err;
		}
	}

	rc = write_ubi_volume_table(pdd, raw_pebs, vol_tab, vol_tab_size,
			&ebs_written, io);
	if (rc != 0) {
		err_msg("Cannot write UBI volume table. rc: %d\n", rc);
		goto err;
	}

	rc  = write_remaining_raw_ebs(pdd, raw_pebs, &ebs_written, io->fp_out);
	if (rc != 0)
		goto err;

	if (io->fp_out != stdout)
		info_msg("Physical eraseblocks written: %8d\n", ebs_written);
err:
	free(vol_tab);
	pfi_raws = remove_all((free_func_t)&free_pfi_raw, pfi_raws);
	pfi_ubis = remove_all((free_func_t)&free_pfi_ubi, pfi_ubis);
	raw_pebs = remove_all((free_func_t)&peb_free, raw_pebs);
	free_pdd_data(&pdd);
	return rc;
}


/* ------------------------------------------------------------------------- */
static void
open_io_handle(myargs *args, io_t io)
{
	/* set PDD input */
	io->fp_pdd = fopen(args->f_in_pdd, "r");
	if (io->fp_pdd == NULL) {
		err_sys("Cannot open: %s", args->f_in_pdd);
	}

	/* set PFI input */
	io->fp_pfi = fopen(args->f_in_pfi, "r");
	if (io->fp_pfi == NULL) {
		err_sys("Cannot open PFI input file: %s", args->f_in_pfi);
	}

	/* set output prefix */
	if (strcmp(args->f_out,"") == 0)
		io->fp_out = stdout;
	else {
		io->fp_out = fopen(args->f_out, "wb");
		if (io->fp_out == NULL) {
			err_sys("Cannot open output file: %s", args->f_out);
		}
	}
}

static void
close_io_handle(io_t io)
{
	if (fclose(io->fp_pdd) != 0) {
		err_sys("Cannot close PDD file.");
	}
	if (fclose(io->fp_pfi) != 0) {
		err_sys("Cannot close PFI file.");
	}
	if (io->fp_out != stdout) {
		if (fclose(io->fp_out) != 0) {
			err_sys("Cannot close output file.");
		}
	}

	io->fp_pdd = NULL;
	io->fp_pfi = NULL;
	io->fp_out = NULL;
}

int
main(int argc, char *argv[])
{
	int rc = 0;

	ubigen_init();
	init_crc32_table(crc32_table);

	struct io io = {NULL, NULL, NULL};
	myargs args = {
		.action = ACT_RAW,
		.verbose = 0,

		.f_in_pfi = "",
		.f_in_pdd = "",
		.f_out = "",

		/* arguments */
		.arg1 = NULL,
		.options = NULL,
	};

	/* parse arguments */
	parse_opt(argc, argv, &args);

	if (strcmp(args.f_in_pfi, "") == 0) {
		err_quit("No PFI input file specified!");
	}

	if (strcmp(args.f_in_pdd, "") == 0) {
		err_quit("No PDD input file specified!");
	}

	open_io_handle(&args, &io);

	info_msg("[ Creating RAW...");
	rc = create_raw(&io);
	if (rc != 0) {
		err_msg("Creating RAW failed.");
		goto err;
	}

err:
	close_io_handle(&io);
	if (rc != 0) {
		remove(args.f_out);
	}

	return rc;
}
