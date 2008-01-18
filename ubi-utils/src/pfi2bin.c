/*
 * Copyright (c) International Business Machines Corp., 2006
 * Copyright (C) 2007 Nokia Corporation
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
 *
 * Convert a PFI file (partial flash image) into a plain binary file.
 * This tool can be used to prepare the data to be burned into flash
 * chips in a manufacturing step where the flashes are written before
 * being soldered onto the hardware. For NAND images one more step may be
 * needed to add the right OOB data to the binary image.
 *
 * Authors: Oliver Lohmann
 *          Artem Bityutskiy
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
#include "common.h"
#include "list.h"
#include "reader.h"
#include "peb.h"
#include "crc32.h"

#define PROGRAM_VERSION "1.5"
#define PROGRAM_NAME    "pfi2bin"

#define ERR_BUF_SIZE 1024

static uint32_t crc32_table[256];
static char err_buf[ERR_BUF_SIZE];

static const char *doc = PROGRAM_NAME " version " PROGRAM_VERSION
" - a tool to convert PFI files into raw flash images. Note, if not\n"
"sure about some of the parameters, do not specify them and let the utility to\n"
"use default values.";

static const char *optionsstr =
"-o, --output=<file name>     output file name (default is stdout)\n"
"-p, --peb-size=<bytes>       size of the physical eraseblock of the flash this\n"
"                             UBI image is created for in bytes, kilobytes (KiB),\n"
"                             or megabytes (MiB) (mandatory parameter)\n"
"-m, --min-io-size=<bytes>    minimum input/output unit size of the flash in bytes\n"
"-s, --sub-page-size=<bytes>  minimum input/output unit used for UBI headers, e.g.\n"
"                             sub-page size in case of NAND flash (equivalent to\n"
"                             the minimum input/output unit size by default)\n"
"-O, --vid-hdr-offset=<num>   offset if the VID header from start of the physical\n"
"                             eraseblock (default is the second minimum I/O unit\n"
"                             or sub-page, if it was specified)\n"
"-e, --erase-counter=<num>    the erase counter value to put to EC headers\n"
"                             (default is 0)\n"
"-x, --ubi-ver=<num>          UBI version number to put to EC headers\n"
"                             (default is 1)\n"
"-h, --help                   print help message\n"
"-V, --version                print program version";

static const char *usage =
"Usage: " PROGRAM_NAME "[-o filename] [-h] [-V] [--output=<filename>] [--help] [--version] pfifile\n"
"Example:" PROGRAM_NAME "-o fs.raw fs.pfi";

struct option long_options[] = {
	{ .name = "output",         .has_arg = 1, .flag = NULL, .val = 'o' },
	{ .name = "peb-size",       .has_arg = 1, .flag = NULL, .val = 'p' },
	{ .name = "min-io-size",    .has_arg = 1, .flag = NULL, .val = 'm' },
	{ .name = "sub-page-size",  .has_arg = 1, .flag = NULL, .val = 's' },
	{ .name = "vid-hdr-offset", .has_arg = 1, .flag = NULL, .val = 'O' },
	{ .name = "erase-counter",  .has_arg = 1, .flag = NULL, .val = 'e' },
	{ .name = "ubi-ver",        .has_arg = 1, .flag = NULL, .val = 'x' },
	{ .name = "help",           .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",        .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

struct args {
	const char *f_in;
	const char *f_out;
	FILE *fp_in;
	FILE *fp_out;
	int peb_size;
	int min_io_size;
	int subpage_size;
	int vid_hdr_offs;
	int ec;
	int ubi_ver;
};

static struct args args = {
	.f_out        = NULL,
	.peb_size     = -1,
	.min_io_size  = -1,
	.subpage_size = -1,
	.vid_hdr_offs = 0,
	.ec           = 0,
	.ubi_ver      = 1,
};

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key;
		char *endp;

		key = getopt_long(argc, argv, "o:p:m:s:O:e:x:hV", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'o':
			args.fp_out = fopen(optarg, "wb");
			if (!args.fp_out) {
				errmsg("cannot open file \"%s\"", optarg);
				return -1;
			}
			args.f_out = optarg;
			break;

		case 'p':
			args.peb_size = strtoull(optarg, &endp, 0);
			if (endp == optarg || args.peb_size <= 0) {
				errmsg("bad physical eraseblock size: \"%s\"", optarg);
				return -1;
			}
			if (*endp != '\0') {
				int mult = ubiutils_get_multiplier(endp);

				if (mult == -1) {
					errmsg("bad size specifier: \"%s\" - "
					       "should be 'KiB', 'MiB' or 'GiB'", endp);
					return -1;
				}
				args.peb_size *= mult;
			}
			break;

		case 'm':
			args.min_io_size = strtoull(optarg, &endp, 0);
			if (endp == optarg || args.min_io_size <= 0) {
				errmsg("bad min. I/O unit size: \"%s\"", optarg);
				return -1;
			}
			if (*endp != '\0') {
				int mult = ubiutils_get_multiplier(endp);

				if (mult == -1) {
					errmsg("bad size specifier: \"%s\" - "
					       "should be 'KiB', 'MiB' or 'GiB'", endp);
					return -1;
				}
				args.min_io_size *= mult;
			}
			break;

		case 's':
			args.subpage_size = strtoull(optarg, &endp, 0);
			if (endp == optarg || args.subpage_size <= 0) {
				errmsg("bad sub-page size: \"%s\"", optarg);
				return -1;
			}
			if (*endp != '\0') {
				int mult = ubiutils_get_multiplier(endp);

				if (mult == -1) {
					errmsg("bad size specifier: \"%s\" - "
					       "should be 'KiB', 'MiB' or 'GiB'", endp);
					return -1;
				}
				args.subpage_size *= mult;
			}
			break;

		case 'O':
			args.vid_hdr_offs = strtoul(optarg, &endp, 0);
			if (endp == optarg || args.vid_hdr_offs < 0) {
				errmsg("bad VID header offset: \"%s\"", optarg);
				return -1;
			}
			break;

		case 'e':
			args.ec = strtoul(optarg, &endp, 0);
			if (endp == optarg || args.ec < 0) {
				errmsg("bad erase counter value: \"%s\"", optarg);
				return -1;
			}
			break;

		case 'x':
			args.ubi_ver = strtoul(optarg, &endp, 0);
			if (endp == optarg || args.ubi_ver < 0) {
				errmsg("bad UBI version: \"%s\"", optarg);
				return -1;
			}
			break;

		case 'h':
			fprintf(stderr, "%s\n\n", doc);
			fprintf(stderr, "%s\n\n", usage);
			fprintf(stderr, "%s\n", optionsstr);
			exit(EXIT_SUCCESS);

		case 'V':
			fprintf(stderr, "%s\n", PROGRAM_VERSION);
			exit(EXIT_SUCCESS);

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}

	if (optind == argc) {
		errmsg("input PFI file was not specified (use -h for help)");
		return -1;
	}

	if (optind != argc - 1) {
		errmsg("more then one input PFI file was specified (use -h for help)");
		return -1;
	}

	if (args.peb_size < 0) {
		errmsg("physical eraseblock size was not specified (use -h for help)");
		return -1;
	}

	if (args.min_io_size < 0) {
		errmsg("min. I/O unit size was not specified (use -h for help)");
		return -1;
	}

	if (args.subpage_size < 0)
		args.subpage_size = args.min_io_size;

	args.f_in = argv[optind++];
	args.fp_in = fopen(args.f_in, "rb");
	if (!args.fp_in) {
		errmsg("cannot open file \"%s\"", args.f_in);
		return -1;
	}

	if (!args.f_out) {
		args.f_out = "stdout";
		args.fp_out = stdout;
	}

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
memorize_raw_eb(pfi_raw_t pfi_raw, list_t *raw_pebs)
{
	int err = 0;
	int i, read, to_read, eb_num, bytes_left;
	list_t pebs = *raw_pebs;
	peb_t	peb  = NULL;

	long old_file_pos = ftell(args.fp_in);
	for (i = 0; i < (int)pfi_raw->starts_size; i++) {
		bytes_left = pfi_raw->data_size;
		err = fseek(args.fp_in, old_file_pos, SEEK_SET);
		if (err != 0)
			goto err;

		eb_num = byte_to_blk(pfi_raw->starts[i], args.peb_size);
		while (bytes_left) {
			to_read = MIN(bytes_left, args.peb_size);
			err = peb_new(eb_num++, args.peb_size, &peb);
			if (err != 0)
				goto err;
			read = fread(peb->data, 1, to_read, args.fp_in);
			if (read != to_read) {
				err = -EIO;
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
	return err;
}

static int
convert_ubi_volume(pfi_ubi_t ubi, list_t raw_pebs,
		struct ubi_vtbl_record *vol_tab,
		size_t *ebs_written)
{
	int err = 0;
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

	err = peb_new(0, 0, &cmp_peb);
	if (err != 0)
		goto err;

	long old_file_pos = ftell(args.fp_in);
	for (i = 0; i < ubi->ids_size; i++) {
		err = fseek(args.fp_in, old_file_pos, SEEK_SET);
		if (err != 0)
			goto err;
		err = ubigen_create(&u, ubi->ids[i], vol_type,
				   args.peb_size, args.ec,
				   ubi->alignment, args.ubi_ver,
				   args.vid_hdr_offs, 0, ubi->data_size,
				   args.fp_in, args.fp_out);
		if (err != 0)
			goto err;

		err = ubigen_get_leb_total(u, &leb_total);
		if (err != 0)
			goto err;

		j = 0;
		while(j < leb_total) {
			cmp_peb->num = *ebs_written;
			raw_peb = is_in((cmp_func_t)peb_cmp, cmp_peb,
					raw_pebs);
			if (raw_peb) {
				err = peb_write(args.fp_out, raw_peb);
			}
			else {
				err = ubigen_write_leb(u, NO_ERROR);
				j++;
			}
			if (err != 0)
				goto err;
			(*ebs_written)++;
		}
		/* memorize volume table entry */
		err = ubigen_set_lvol_rec(u, ubi->size,
				ubi->names[i],
				(void*) &vol_tab[ubi->ids[i]]);
		if (err != 0)
			goto err;
		ubigen_destroy(&u);
	}

	peb_free(&cmp_peb);
	return 0;

err:
	peb_free(&cmp_peb);
	ubigen_destroy(&u);
	return err;
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
write_ubi_volume_table(list_t raw_pebs,
		struct ubi_vtbl_record *vol_tab, size_t vol_tab_size,
		size_t *ebs_written)
{
	int err = 0;
	ubi_info_t u;
	peb_t raw_peb;
	peb_t cmp_peb;
	size_t leb_size, leb_total, j = 0;
	uint8_t *ptr = NULL;
	FILE* fp_leb = NULL;
	int vt_slots;
	size_t vol_tab_size_limit;

	err = peb_new(0, 0, &cmp_peb);
	if (err != 0)
		goto err;

	/* @FIXME: Artem creates one volume with 2 LEBs.
	 * IMO 2 volumes would be more convenient. In order
	 * to get 2 reserved LEBs from ubigen, I have to
	 * introduce this stupid mechanism. Until no final
	 * decision of the VTAB structure is made... Good enough.
	 */
	err = ubigen_create(&u, UBI_LAYOUT_VOL_ID, UBI_VID_DYNAMIC,
			   args.peb_size, args.ec,
			   1, args.ubi_ver,
			   args.vid_hdr_offs, UBI_COMPAT_REJECT,
			   vol_tab_size, stdin, args.fp_out);
			   /* @FIXME stdin for fp_in is a hack */
	if (err != 0)
		goto err;
	err = ubigen_get_leb_size(u, &leb_size);
	if (err != 0)
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

	err = ubigen_create(&u, UBI_LAYOUT_VOL_ID, UBI_VID_DYNAMIC,
			   args.peb_size, args.ec,
			   1, args.ubi_ver, args.vid_hdr_offs,
			   UBI_COMPAT_REJECT, leb_size * UBI_LAYOUT_VOLUME_EBS,
			   fp_leb, args.fp_out);
	if (err != 0)
		goto err;
	err = ubigen_get_leb_total(u, &leb_total);
	if (err != 0)
		goto err;

	long old_file_pos = ftell(fp_leb);
	while(j < leb_total) {
		err = fseek(fp_leb, old_file_pos, SEEK_SET);
		if (err != 0)
			goto err;

		cmp_peb->num = *ebs_written;
		raw_peb = is_in((cmp_func_t)peb_cmp, cmp_peb,
				raw_pebs);
		if (raw_peb) {
			err = peb_write(args.fp_out, raw_peb);
		}
		else {
			err = ubigen_write_leb(u, NO_ERROR);
			j++;
		}

		if (err != 0)
			goto err;
		(*ebs_written)++;
	}

err:
	free(ptr);
	peb_free(&cmp_peb);
	ubigen_destroy(&u);
	fclose(fp_leb);
	return err;
}

static int
write_remaining_raw_ebs(list_t raw_blocks, size_t *ebs_written,
			FILE* fp_out)
{
	int err = 0;
	uint32_t j, delta;
	list_t ptr;
	peb_t empty_eb, peb;

	/* create an empty 0xff EB (for padding) */
	err = peb_new(0, args.peb_size, &empty_eb);

	foreach(peb, ptr, raw_blocks) {
		if (peb->num < *ebs_written) {
			continue; /* omit blocks which
				     are already passed */
		}

		if (peb->num < *ebs_written) {
			errmsg("eb_num: %d\n", peb->num);
			errmsg("Bug: This should never happen. %d %s",
				__LINE__, __FILE__);
			goto err;
		}

		delta = peb->num - *ebs_written;
		for (j = 0; j < delta; j++) {
			err = peb_write(fp_out, empty_eb);
			if (err != 0)
				goto err;
			(*ebs_written)++;
		}
		err = peb_write(fp_out, peb);
		if (err != 0)
			goto err;
		(*ebs_written)++;
	}

err:
	peb_free(&empty_eb);
	return err;
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
		res[i].crc = __cpu_to_be32(crc);
	}

	*vol_tab = res;
	return 0;
}

static int
create_raw(void)
{
	int err = 0;
	size_t ebs_written = 0; /* eraseblocks written already... */
	size_t vol_tab_size;
	list_t ptr;

	list_t pfi_raws = mk_empty(); /* list of raw sections from a pfi */
	list_t pfi_ubis = mk_empty(); /* list of ubi sections from a pfi */
	list_t raw_pebs	 = mk_empty(); /* list of raw eraseblocks */

	struct ubi_vtbl_record *vol_tab = NULL;

	err = init_vol_tab (&vol_tab, &vol_tab_size);
	if (err != 0) {
		errmsg("cannot initialize volume table");
		goto err;
	}

	err = read_pfi_headers(&pfi_raws, &pfi_ubis, args.fp_in,
			err_buf, ERR_BUF_SIZE);
	if (err != 0) {
		errmsg("cannot read pfi header: %s err: %d", err_buf, err);
		goto err;
	}

	pfi_raw_t pfi_raw;
	foreach(pfi_raw, ptr, pfi_raws) {
		err = memorize_raw_eb(pfi_raw, &raw_pebs);
		if (err != 0) {
			errmsg("cannot create raw_block in mem. err: %d\n", err);
			goto err;
		}
	}

	pfi_ubi_t pfi_ubi;
	foreach(pfi_ubi, ptr, pfi_ubis) {
		err = convert_ubi_volume(pfi_ubi, raw_pebs,
					vol_tab, &ebs_written);
		if (err != 0) {
			errmsg("cannot convert UBI volume. err: %d\n", err);
			goto err;
		}
	}

	err = write_ubi_volume_table(raw_pebs, vol_tab, vol_tab_size,
			&ebs_written);
	if (err != 0) {
		errmsg("cannot write UBI volume table. err: %d\n", err);
		goto err;
	}

	err  = write_remaining_raw_ebs(raw_pebs, &ebs_written, args.fp_out);
	if (err != 0)
		goto err;

	if (args.fp_out != stdout)
		printf("Physical eraseblocks written: %8d\n", ebs_written);
err:
	free(vol_tab);
	pfi_raws = remove_all((free_func_t)&free_pfi_raw, pfi_raws);
	pfi_ubis = remove_all((free_func_t)&free_pfi_ubi, pfi_ubis);
	raw_pebs = remove_all((free_func_t)&peb_free, raw_pebs);
	return err;
}

int main(int argc, char * const argv[])
{
	int err;

	err = parse_opt(argc, argv);
	if (err)
		return -1;

	ubigen_init();
	init_crc32_table(crc32_table);

	err = create_raw();
	if (err != 0) {
		errmsg("creating RAW failed");
		goto err;
	}

err:
	if (err != 0)
		remove(args.f_out);

	return err;
}
