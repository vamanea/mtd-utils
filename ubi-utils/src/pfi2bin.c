/*
 * Copyright (c) International Business Machines Corp., 2006
 * Copyright (C) 2008 Nokia Corporation
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
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <mtd/ubi-header.h>
#include <libubigen.h>
#include <libpfi.h>
#include "common.h"
#include "list.h"

#define PROGRAM_VERSION "1.5"
#define PROGRAM_NAME    "pfi2bin"

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

/**
 * pfi2vol_info - convert PFI UBI volume information to libubigen.
 * @pfi: PFI UBI volume information
 * @n: PFI volume index to convert
 * @vi: libubigen volume information
 */
static void pfi2vol_info(const struct pfi_ubi *pfi, int n,
			 struct ubigen_vol_info *vi,
			 const struct ubigen_info *ui)
{
	vi->id = pfi->ids[n];
	vi->bytes = pfi->size;
	vi->alignment = pfi->alignment;
	vi->data_pad = ui->leb_size % vi->alignment;
	vi->usable_leb_size = ui->leb_size - vi->data_pad;
	vi->type = pfi->vol_type;
	vi->name = pfi->names[n];
	vi->name_len = strlen(vi->name);
	if (vi->name_len > UBI_VOL_NAME_MAX) {
		errmsg("too long name, cut to %d symbols: \"%s\"",
		       UBI_VOL_NAME_MAX, vi->name);
		vi->name_len = UBI_VOL_NAME_MAX;
	}

	vi->used_ebs = (vi->bytes + vi->usable_leb_size - 1) / vi->usable_leb_size;
	vi->compat = 0;
}

static int create_flash_image(void)
{
	int i, err, vtbl_size = args.peb_size;
	struct ubigen_info ui;
	struct list_entry *ubi_list = list_empty(), *ptr;
	struct ubi_vtbl_record *vtbl;
	struct pfi_ubi *pfi;

	vtbl = ubigen_create_empty_vtbl(&vtbl_size);
	if (!vtbl) {
		errmsg("cannot initialize volume table");
		return -1;
	}

	ubigen_info_init(&ui, args.peb_size, args.min_io_size,
			 args.subpage_size, args.vid_hdr_offs, args.ubi_ver,
			 args.ec);

	err = read_pfi_headers(&ubi_list, args.fp_in);
	if (err != 0) {
		errmsg("cannot read PFI headers, error %d", err);
		goto error;
	}

	/* Add all volumes to the volume table */
	list_for_each(pfi, ptr, ubi_list)
		for (i = 0; i < pfi->ids_size; i++) {
			struct ubigen_vol_info vi;

			pfi2vol_info(pfi, i, &vi, &ui);
			ubigen_add_volume(&ui, &vi, vtbl);
		}

	err = ubigen_write_layout_vol(&ui, vtbl, args.fp_out);
	if (err) {
		errmsg("cannot create layout volume");
		goto error;
	}

	/* Write all volumes */
	list_for_each(pfi, ptr, ubi_list)
		for (i = 0; i < pfi->ids_size; i++) {
			struct ubigen_vol_info vi;

			pfi2vol_info(pfi, i, &vi, &ui);
			err = fseek(args.fp_in, pfi->data_offs, SEEK_SET);
			if (err == -1) {
				errmsg("cannot seek input file");
				perror("fseek");
				goto error;
			}

			err = ubigen_write_volume(&ui, &vi, pfi->data_size,
						  args.fp_in, args.fp_out);
			if (err) {
				errmsg("cannot write volume %d", vi.id);
				goto error;
			}
		}

	if (args.fp_out != stdout) {
		i = ftell(args.fp_out);
		if (i == -1) {
			errmsg("cannot seek output file");
			perror("ftell");
			goto error;
		}

		printf("physical eraseblocks written: %d (", i / ui.peb_size);
		ubiutils_print_bytes(i, 0);
		printf(")\n");
	}

error:
	free(vtbl);
	ubi_list = remove_all((free_func_t)&free_pfi_ubi, ubi_list);
	return err;
}

int main(int argc, char * const argv[])
{
	int err;

	err = parse_opt(argc, argv);
	if (err)
		return -1;

	err = create_flash_image();
	if (err)
		remove(args.f_out);

	return err;
}
