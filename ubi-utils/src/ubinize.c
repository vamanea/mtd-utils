/*
 * Copyright (C) 2008 Nokia Corporation
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
 * Generate UBI images.
 *
 * Authors: Artem Bityutskiy
 *          Oliver Lohmann
 */

#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include <mtd/ubi-header.h>
#include <libubigen.h>
#include <libiniparser.h>
#include "common.h"

#define PROGRAM_VERSION "1.5"
#define PROGRAM_NAME    "ubinize"

static const char *doc = PROGRAM_NAME " version " PROGRAM_VERSION
" - a tool to generate UBI images. An UBI image may contain one or more UBI "
"volumes which have to be defined in the input configuration ini-file. The "
"ini file defines all the UBI volumes - their characteristics and the and the "
"contents, but it does not define the characteristics of the flash the UBI "
"image is generated for. Instead, the flash characteristics are defined via "
"the comman-line options. "
"Note, if not sure about some of the command-line parameters, do not specify "
"them and let the utility to use default values.";

static const char *optionsstr =
"-o, --output=<file name>     output file name (default is stdout)\n"
"-p, --peb-size=<bytes>       size of the physical eraseblock of the flash\n"
"                             this UBI image is created for in bytes,\n"
"                             kilobytes (KiB), or megabytes (MiB)\n"
"                             (mandatory parameter)\n"
"-m, --min-io-size=<bytes>    minimum input/output unit size of the flash\n"
"                             in bytes\n"
"-s, --sub-page-size=<bytes>  minimum input/output unit used for UBI\n"
"                             headers, e.g. sub-page size in case of NAND\n"
"                             flash (equivalent to the minimum input/output\n"
"                             unit size by default)\n"
"-O, --vid-hdr-offset=<num>   offset if the VID header from start of the\n"
"                             physical eraseblock (default is the second\n"
"                             minimum I/O unit or sub-page, if it was\n"
"                             specified)\n"
"-e, --erase-counter=<num>    the erase counter value to put to EC headers\n"
"                             (default is 0)\n"
"-x, --ubi-ver=<num>          UBI version number to put to EC headers\n"
"                             (default is 1)\n"
"-v  --verbose                be verbose\n"
"-h, --help                   print help message\n"
"-V, --version                print program version";

static const char *usage =
"Usage: " PROGRAM_NAME " [-o filename] [-h] [-V] [--output=<filename>] [--help]\n"
"\t\t[--version] inifile\n"
"Example: " PROGRAM_NAME "-o fs.raw cfg.ini";

struct option long_options[] = {
	{ .name = "output",         .has_arg = 1, .flag = NULL, .val = 'o' },
	{ .name = "peb-size",       .has_arg = 1, .flag = NULL, .val = 'p' },
	{ .name = "min-io-size",    .has_arg = 1, .flag = NULL, .val = 'm' },
	{ .name = "sub-page-size",  .has_arg = 1, .flag = NULL, .val = 's' },
	{ .name = "vid-hdr-offset", .has_arg = 1, .flag = NULL, .val = 'O' },
	{ .name = "erase-counter",  .has_arg = 1, .flag = NULL, .val = 'e' },
	{ .name = "ubi-ver",        .has_arg = 1, .flag = NULL, .val = 'x' },
	{ .name = "verbose",        .has_arg = 1, .flag = NULL, .val = 'v' },
	{ .name = "help",           .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",        .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

struct args {
	const char *f_in;
	const char *f_out;
	FILE *fp_out;
	int peb_size;
	int min_io_size;
	int subpage_size;
	int vid_hdr_offs;
	int ec;
	int ubi_ver;
	int verbose;
	dictionary *dict;
};

static struct args args = {
	.f_out        = NULL,
	.peb_size     = -1,
	.min_io_size  = -1,
	.subpage_size = -1,
	.vid_hdr_offs = 0,
	.ec           = 0,
	.ubi_ver      = 1,
	.verbose      = 0,
};

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key;
		char *endp;

		key = getopt_long(argc, argv, "o:p:m:s:O:e:x:vhV", long_options, NULL);
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

		case 'v':
			args.verbose = 1;
			break;

		case 'h':
			ubiutils_print_text(stderr, doc, 80);
			fprintf(stderr, "\n\n%s\n\n", usage);
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

	args.f_in = argv[optind];

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

	if (!args.f_out) {
		args.f_out = "stdout";
		args.fp_out = stdout;
	}

	return 0;
}

int read_section(const char *sname, struct ubigen_vol_info *vi,
		 const char **img)
{
	char buf[256];
	const char *p;

	*img = NULL;

	if (strlen(sname) > 128) {
		errmsg("too long section name \"%s\"", sname);
		return -1;
	}

	/* Make sure mode is UBI, otherwise ignore this section */
	sprintf(buf, "%s:mode", sname);
	p = iniparser_getstring(args.dict, buf, NULL);
	if (!p) {
		errmsg("\"mode\" key not found in section \"%s\"", sname);
		errmsg("the \"mode\" key is mandatory and has to be "
		       "\"mode=ubi\" if the section describes an UBI volume");
		return -1;
	}

	/* If mode is not UBI, skip this section */
	if (strcmp(p, "ubi")) {
		verbose(args.verbose, "skip non-ubi section \"%s\"", sname);
		return 1;
	}

	verbose(args.verbose, "mode=ubi, keep parsing");

	/* Fetch the name of the volume image file */
	sprintf(buf, "%s:image", sname);
	p = iniparser_getstring(args.dict, buf, NULL);
	if (p)
		*img = p;

	/* Fetch volume id */
	sprintf(buf, "%s:vol_id", sname);
	vi->id = iniparser_getint(args.dict, buf, -1);
	if (vi->id == -1) {
		errmsg("\"vol_id\" key not found in section  \"%s\"", sname);
		return -1;
	}

	if (vi->id < 0) {
		errmsg("negative volume ID %d", vi->id);
		return -1;
	}

	if (vi->id >= UBI_MAX_VOLUMES) {
		errmsg("too highe volume ID %d, max. is %d",
		       vi->id, UBI_MAX_VOLUMES);
		return -1;
	}

	verbose(args.verbose, "volume ID: %d", vi->id);

	/* Fetch volume size */
	sprintf(buf, "%s:vol_size", sname);
	p = iniparser_getstring(args.dict, buf, NULL);
	if (p) {
		char *endp;

		vi->bytes = strtoull((char *)p, &endp, 0);
		if (endp == p || vi->bytes <= 0) {
			errmsg("bad \"vol_size\" key: \"%s\"", p);
			return -1;
		}

		if (*endp != '\0') {
			int mult = ubiutils_get_multiplier(endp);

			if (mult == -1) {
				errmsg("bad size specifier: \"%s\" - "
				       "should be 'KiB', 'MiB' or 'GiB'", endp);
				return -1;
			}
			vi->bytes *= mult;
		}

		verbose(args.verbose, "volume size: %lld bytes", vi->bytes);
	} else {
		struct stat st;

		if (!*img) {
			errmsg("neither image file (\"image=\") nor volume size"
			       " (\"vol_size=\") specified");
			return -1;
		}

		if (stat(*img, &st)) {
			errmsg("cannot stat \"%s\"", *img);
			perror("stat");
			return -1;
		}

		vi->bytes = st.st_size;

		if (vi->bytes == 0) {
			errmsg("file \"%s\" referred from section \"%s\" is empty",
			       *img, sname);
			return -1;
		}

		printf(PROGRAM_NAME ": volume size was not specified in"
		       "section \"%s\", assume ", sname);
		ubiutils_print_bytes(vi->bytes, 1);
		printf("\n");
	}

	/* Fetch volume type */
	sprintf(buf, "%s:vol_type", sname);
	p = iniparser_getstring(args.dict, buf, NULL);
	if (!p) {
		normsg(": volume type was not specified in "
		       "section \"%s\", assume \"dynamic\"\n", sname);
		vi->type = UBI_VID_DYNAMIC;
	} else {
		if (!strcmp(p, "static"))
			vi->type = UBI_VID_STATIC;
		else if (!strcmp(p, "dynamic"))
			vi->type = UBI_VID_DYNAMIC;
		else {
			errmsg("invalid volume type \"%s\"", p);
			return -1;
		}
	}

	verbose(args.verbose, "volume type: %s",
		vi->type == UBI_VID_DYNAMIC ? "dynamic" : "static");

	/* Fetch volume name */
	sprintf(buf, "%s:vol_name", sname);
	p = iniparser_getstring(args.dict, buf, NULL);
	if (!p) {
		errmsg("\"vol_name\" key not found in section  \"%s\"", sname);
		return -1;
	}

	vi->name = p;
	vi->name_len = strlen(p);
	if (vi->name_len > UBI_VOL_NAME_MAX) {
		errmsg("too long volume name in section \"%s\", max. is "
		       "%d characters", vi->name, UBI_VOL_NAME_MAX);
		return -1;
	}

	verbose(args.verbose, "volume name: %s", p);

	/* Fetch volume alignment */
	sprintf(buf, "%s:vol_alignment", sname);
	vi->alignment = iniparser_getint(args.dict, buf, -1);
	if (vi->alignment == -1) {
		normsg("volume alignment was not specified in section "
		       "\"%s\", assume 1", sname);
			vi->alignment = 1;
	} else if (vi->id < 0) {
		errmsg("negative volume alignement %d", vi->alignment);
		return -1;
	}

	verbose(args.verbose, "volume alignment: %d", vi->alignment);

	return 0;
}

static void init_vol_info(const struct ubigen_info *ui,
			  struct ubigen_vol_info *vi)
{
	vi->data_pad = ui->leb_size % vi->alignment;
	vi->usable_leb_size = ui->leb_size - vi->data_pad;
	vi->used_ebs = (vi->bytes + vi->usable_leb_size - 1) / vi->usable_leb_size;
	vi->compat = 0;
}

int main(int argc, char * const argv[])
{
	int err = -1, sects, i, volumes;
	struct ubigen_info ui;
	struct ubi_vtbl_record *vtbl;

	err = parse_opt(argc, argv);
	if (err)
		return -1;

	ubigen_info_init(&ui, args.peb_size, args.min_io_size,
			 args.subpage_size, args.vid_hdr_offs,
			 args.ubi_ver, args.ec);

	verbose(args.verbose, "LEB size:    %d", ui.leb_size);
	verbose(args.verbose, "PEB size:    %d", ui.peb_size);
	verbose(args.verbose, "min_io_size: %d", ui.min_io_size);
	verbose(args.verbose, "VID offset:  %d", ui.vid_hdr_offs);

	vtbl = ubigen_create_empty_vtbl(&ui);
	if (!vtbl)
		goto out;

	args.dict = iniparser_load(args.f_in);
	if (!args.dict) {
		errmsg("cannot load the input ini file \"%s\"", args.f_in);
		goto out_vtbl;
	}

	verbose(args.verbose, "loaded the ini-file \"%s\"", args.f_in);

	/* Each section describes one volume */
	sects = iniparser_getnsec(args.dict);
	if (sects == -1) {
		errmsg("ini-file parsing error (iniparser_getnsec)");
		goto out_dict;
	}

	verbose(args.verbose, "count of sections: %d", sects);
	if (sects == 0) {
		errmsg("no sections found the ini-file \"%s\"", args.f_in);
		goto out_dict;
	}

	/*
	 * Skip 2 PEBs at the beginning of the file for the volume table which
	 * will be written later.
	 */
	if (fseek(args.fp_out, ui.peb_size * 2, SEEK_SET) == -1) {
		errmsg("cannot seek file \"%s\"", args.f_out);
		goto out_dict;
	}

	for (i = 0; i < sects; i++) {
		const char *sname = iniparser_getsecname(args.dict, i);
		struct ubigen_vol_info vi;
		const char *img = NULL;
		struct stat st;
		FILE *f;

		if (!sname) {
			errmsg("ini-file parsing error (iniparser_getsecname)");
			goto out_dict;
		}

		if (args.verbose)
			printf("\n");
		verbose(args.verbose, "parsing section \"%s\"", sname);

		err = read_section(sname, &vi, &img);
		if (err == -1)
			goto out_dict;
		if (!err)
			volumes += 1;
		init_vol_info(&ui, &vi);

		verbose(args.verbose, "adding volume %d", vi.id);

		err = ubigen_add_volume(&ui, &vi, vtbl);
		if (err) {
			errmsg("cannot add volume for section \"%s\"", sname);
			goto out_dict;
		}

		if (!img)
			continue;

		if (stat(img, &st)) {
			errmsg("cannot stat \"%s\"", img);
			perror("stat");
			goto out_dict;
		}

		f = fopen(img, "r");
		if (!f) {
			errmsg("cannot open \"%s\"", img);
			perror("fopen");
			goto out_dict;
		}

		verbose(args.verbose, "writing volume %d", vi.id);
		verbose(args.verbose, "image file:  %s", img);

		err = ubigen_write_volume(&ui, &vi, st.st_size, f, args.fp_out);
		fclose(f);
		if (err) {
			errmsg("cannot write volume for section \"%s\"", sname);
			goto out_dict;
		}

		if (args.verbose)
			printf("\n");
	}

	verbose(args.verbose, "writing layout volume");

	err = ubigen_write_layout_vol(&ui, vtbl, args.fp_out);
	if (err) {
		errmsg("cannot write layout volume");
		goto out_dict;
	}

	verbose(args.verbose, "done");

	iniparser_freedict(args.dict);
	free(vtbl);
	fclose(args.fp_out);
	return 0;

out_dict:
	iniparser_freedict(args.dict);
out_vtbl:
	free(vtbl);
out:
	fclose(args.fp_out);
	remove(args.f_out);
	return err;
}

#if 0
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
#endif
