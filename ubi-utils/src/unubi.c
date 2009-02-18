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
 */

/*
 * Authors: Drake Dowsett, dowsett@de.ibm.com
 *          Frank Haverkamp, haver@vnet.ibm.com
 *
 * 1.2 Removed argp because we want to use uClibc.
 * 1.3 Minor cleanups.
 * 1.4 Meanwhile Drake had done a lot of changes, syncing those.
 * 1.5 Bugfixes, simplifications
 */

/*
 * unubi  reads  an  image  file containing blocks of UBI headers and data
 * (such as produced from nand2bin) and rebuilds the volumes within.   The
 * default  operation  (when  no  flags are given) is to rebuild all valid
 * volumes found in the image. unubi  can  also  read  straight  from  the
 * onboard MTD device (ex. /dev/mtdblock/NAND).
 */

/* TODO: consideration for dynamic vs. static volumes */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mtd/ubi-media.h>
#include <mtd_swab.h>

#include "crc32.h"
#include "unubi_analyze.h"

#define EXEC		"unubi"
#define CONTACT		"haver@vnet.ibm.com"
#define VERSION		"1.5"

static char doc[] = "\nVersion: " VERSION "\n";
static int debug = 0;

static const char *optionsstr =
"Extract volumes and/or analysis information from an UBI data file.\n"
"When no parameters are flagged or given, the default operation is\n"
"to rebuild all valid complete UBI volumes found within the image.\n"
"\n"
" OPERATIONS\n"
"  -a, --analyze              Analyze image and create gnuplot graphs\n"
"  -i, --info-table           Extract volume information tables\n"
"  -r, --rebuild=<volume-id>  Extract and rebuild volume\n"
"\n"
" OPTIONS\n"
"  -b, --blocksize=<block-size>   Specify size of eraseblocks in image in bytes\n"
"                             (default 128KiB)\n"
"  -d, --dir=<output-dir>     Specify output directory\n"
"  -D, --debug                Enable debug output\n"
"  -s, --headersize=<header-size>   Specify size reserved for metadata in eraseblock\n"
	"                             in bytes (default 2048 Byte)\n"
 /* the -s option might be insufficient when using different vid
    offset than what we used when writing this tool ... Better would
    probably be --vid-hdr-offset or alike */
"\n"
" ADVANCED\n"
"  -e, --eb-split             Generate individual eraseblock images (all\n"
"                             eraseblocks)\n"
"  -v, --vol-split            Generate individual eraseblock images (valid\n"
"                             eraseblocks only)\n"
"  -V, --vol-split!           Raw split by eraseblock (valid eraseblocks only)\n"
"\n"
"  -?, --help                 Give this help list\n"
"      --usage                Give a short usage message\n"
"      --version              Print program version\n"
"\n";

static const char *usage =
"Usage: unubi [-aievV?] [-r <volume-id>] [-b <block-size>] [-d <output-dir>]\n"
"            [-s <header-size>] [--analyze] [--info-table]\n"
"            [--rebuild=<volume-id>] [--blocksize=<block-size>]\n"
"            [--dir=<output-dir>] [--headersize=<header-size>] [--eb-split]\n"
"            [--vol-split] [--vol-split!] [--help] [--usage] [--version]\n"
"            image-file\n";

#define ERR_MSG(fmt...)							\
	fprintf(stderr, EXEC ": " fmt)

#define SPLIT_DATA	1
#define SPLIT_RAW	2

#define DIR_FMT		"unubi_%s"
#define KIB		1024
#define MIB		(KIB * KIB)
#define MAXPATH		KIB

/* filenames */
#define FN_INVAL	"%s/eb%04u%s"			/* invalid eraseblock */
#define FN_NSURE	"%s/eb%04u_%03u_%03u_%03x%s"	/* unsure eraseblock */
#define FN_VALID	"%s/eb%04u_%03u_%03u_%03x%s"	/* valid eraseblock */
#define FN_VOLSP	"%s/vol%03u_%03u_%03u_%04zu"	/* split volume */
#define FN_VOLWH	"%s/volume%03u"			/* whole volume */
#define FN_VITBL	"%s/vol_info_table%zu"		/* vol info table */

static uint32_t crc32_table[256];

/* struct args:
 *	bsize		int, blocksize of image blocks
 *	hsize		int, eraseblock header size
 *	analyze		flag, when non-zero produce analysis
 *	eb_split	flag, when non-zero output eb####
 *			note: SPLIT_DATA vs. SPLIT_RAW
 *	vol_split	flag, when non-zero output vol###_####
 *			note: SPLIT_DATA vs. SPLIT_RAW
 *	odir_path	string, directory to place volumes in
 *	img_path	string, file to read as ubi image
 *	vols		int array of size UBI_MAX_VOLUMES, where a 1 can be
 *			written for each --rebuild flag in the index specified
 *			then the array can be counted and collapsed using
 *			count_set() and collapse()
 */
struct args {
	int analyze;
	int itable;
	uint32_t *vols;

	size_t vid_hdr_offset;
	size_t data_offset;
	size_t bsize;		/* FIXME replace by vid_hdr/data offs? */
	size_t hsize;

	char *odir_path;
	int eb_split;
	int vol_split;
	char *img_path;

	char **options;
};

struct option long_options[] = {
	{ .name = "rebuild", .has_arg = 1, .flag = NULL, .val = 'r' },
	{ .name = "dir", .has_arg = 1, .flag = NULL, .val = 'd' },
	{ .name = "analyze", .has_arg = 0, .flag = NULL, .val = 'a' },
	{ .name = "blocksize", .has_arg = 1, .flag = NULL, .val = 'b' },
	{ .name = "eb-split", .has_arg = 0, .flag = NULL, .val = 'e' },
	{ .name = "vol-split", .has_arg = 0, .flag = NULL, .val = 'v' },
	{ .name = "vol-split!", .has_arg = 0, .flag = NULL, .val = 'e' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = '?' },
	{ .name = "usage", .has_arg = 0, .flag = NULL, .val = 0 },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'J' },
	{ NULL, 0, NULL, 0}
};

/**
 * parses out a numerical value from a string of numbers followed by:
 *	k, K, kib, KiB for kibibyte
 *	m, M, mib, MiB for mebibyte
 **/
static uint32_t
str_to_num(char *str)
{
	char *s;
	ulong num;

	s = str;
	num = strtoul(s, &s, 0);

	if (*s != '\0') {
		if ((strcmp(s, "KiB") == 0) || (strcmp(s, "K") == 0) ||
		    (strcmp(s, "kib") == 0) || (strcmp(s, "k") == 0))
			num *= KIB;
		else if ((strcmp(s, "MiB") == 0) || (strcmp(s, "M") == 0) ||
		    (strcmp(s, "mib") == 0) || (strcmp(s, "m") == 0))
			num *= MIB;
		else
			ERR_MSG("couldn't parse '%s', assuming %lu\n",
				s, num);
	}
	return num;
}

static int
parse_opt(int argc, char **argv, struct args *args)
{
	uint32_t i;

	while (1) {
		int key;

		key = getopt_long(argc, argv, "ab:s:d:Deir:vV?J",
				  long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'a': /* --analyze */
			args->analyze = 1;
			break;
		case 'b': /* --block-size=<block-size> */
			args->bsize = str_to_num(optarg);
			break;
		case 's': /* --header-size=<header-size> */
			args->hsize = str_to_num(optarg);
			break;
		case 'd': /* --dir=<output-dir> */
			args->odir_path = optarg;
			break;
		case 'D': /* --debug */
			/* I wanted to use -v but that was already
			   used ... */
			debug = 1;
			break;
		case 'e': /* --eb-split */
			args->eb_split = SPLIT_RAW;
			break;
		case 'i': /* --info-table */
			args->itable = 1;
			break;
		case 'r': /* --rebuild=<volume-id> */
			i = str_to_num(optarg);
			if (i < UBI_MAX_VOLUMES)
				args->vols[str_to_num(optarg)] = 1;
			else {
				ERR_MSG("volume-id out of bounds\n");
				return -1;
			}
			break;
		case 'v': /* --vol-split */
			if (args->vol_split != SPLIT_RAW)
				args->vol_split = SPLIT_DATA;
			break;
		case 'V': /* --vol-split! */
			args->vol_split = SPLIT_RAW;
			break;
		case '?': /* help */
			fprintf(stderr,	"Usage: unubi [OPTION...] "
				"image-file\n%s%s\nReport bugs to %s\n",
				doc, optionsstr, CONTACT);
			exit(0);
			break;
		case 'J':
			fprintf(stderr, "%s\n", VERSION);
			exit(0);
			break;
		default:
			fprintf(stderr, "%s", usage);
			exit(-1);
		}
	}

	/* FIXME I suppose hsize should be replaced! */
	args->vid_hdr_offset = args->hsize - UBI_VID_HDR_SIZE;
	args->data_offset = args->hsize;

	if (optind < argc)
		args->img_path = argv[optind++];
	return 0;
}


/**
 * counts the number of indicies which are flagged in full_array;
 * full_array is an array of flags (1/0);
 **/
static size_t
count_set(uint32_t *full_array, size_t full_len)
{
	size_t count, i;

	if (full_array == NULL)
		return 0;

	for (i = 0, count = 0; i < full_len; i++)
		if (full_array[i] != 0)
			count++;

	return count;
}


/**
 * generates coll_array from full_array;
 * full_array is an array of flags (1/0);
 * coll_array is an array of the indicies in full_array which are flagged (1);
 **/
static size_t
collapse(uint32_t *full_array, size_t full_len,
	 uint32_t *coll_array, size_t coll_len)
{
	size_t i, j;

	if ((full_array == NULL) || (coll_array == NULL))
		return 0;

	for (i = 0, j = 0; (i < full_len) && (j < coll_len); i++)
		if (full_array[i] != 0) {
			coll_array[j] = i;
			j++;
		}

	return j;
}

/**
 * data_crc: save the FILE* position, calculate the crc over a span,
 *	reset the position
 * returns non-zero when EOF encountered
 **/
static int
data_crc(FILE* fpin, size_t length, uint32_t *ret_crc)
{
	int rc;
	size_t i;
	char buf[length];
	uint32_t crc;
	fpos_t start;

	rc = fgetpos(fpin, &start);
	if (rc < 0)
		return -1;

	for (i = 0; i < length; i++) {
		int c = fgetc(fpin);
		if (c == EOF) {
			ERR_MSG("unexpected EOF\n");
			return -1;
		}
		buf[i] = (char)c;
	}

	rc = fsetpos(fpin, &start);
	if (rc < 0)
		return -1;

	crc = clc_crc32(crc32_table, UBI_CRC32_INIT, buf, length);
	*ret_crc = crc;
	return 0;
}


/**
 * reads data of size len from fpin and writes it to path
 **/
static int
extract_data(FILE* fpin, size_t len, const char *path)
{
	int rc;
	size_t i;
	FILE* fpout;

	rc = 0;
	fpout = NULL;

	fpout = fopen(path, "wb");
	if (fpout == NULL) {
		ERR_MSG("couldn't open file for writing: %s\n", path);
		rc = -1;
		goto err;
	}

	for (i = 0; i < len; i++) {
		int c = fgetc(fpin);
		if (c == EOF) {
			ERR_MSG("unexpected EOF while writing: %s\n", path);
			rc = -2;
			goto err;
		}
		c = fputc(c, fpout);
		if (c == EOF) {
			ERR_MSG("couldn't write: %s\n", path);
			rc = -3;
			goto err;
		}
	}

 err:
	if (fpout != NULL)
		fclose(fpout);
	return rc;
}


/**
 * extract volume information table from block. saves and reloads fpin
 * position
 * returns -1 when a fpos set or get fails, otherwise <= -2 on other
 * failure and 0 on success
 **/
static int
extract_itable(FILE *fpin, struct eb_info *cur, size_t bsize, size_t num,
	       const char *path)
{
	char filename[MAXPATH + 1];
	int rc;
	size_t i, max;
	fpos_t temp;
	FILE* fpout = NULL;
	struct ubi_vtbl_record rec;

	if (fpin == NULL || cur == NULL || path == NULL)
		return -2;

	/* remember position */
	rc = fgetpos(fpin, &temp);
	if (rc < 0)
		return -1;

	/* jump to top of eraseblock, skip to data section */
	fsetpos(fpin, &cur->eb_top);
	if (rc < 0)
		return -1;
	fseek(fpin, be32_to_cpu(cur->ec.data_offset), SEEK_CUR);

	/* prepare output file */
	if (be32_to_cpu(cur->vid.vol_id) != UBI_LAYOUT_VOLUME_ID)
		return -2;
	memset(filename, 0, MAXPATH + 1);
	snprintf(filename, MAXPATH, FN_VITBL, path, num);
	fpout = fopen(filename, "w");
	if (fpout == NULL)
		return -2;

	/* loop through entries */
	fprintf(fpout,
		"index\trpebs\talign\ttype\tcrc\t\tname\n");
	max = bsize - be32_to_cpu(cur->ec.data_offset);
	for (i = 0; i < (max / sizeof(rec)); i++) {
		int blank = 1;
		char *ptr, *base;
		char name[UBI_VOL_NAME_MAX + 1];
		const char *type = "unknown\0";
		uint32_t crc;

		/* read record */
		rc = fread(&rec, 1, sizeof(rec), fpin);
		if (rc == 0)
			break;
		if (rc != sizeof(rec)) {
			ERR_MSG("reading volume information "
				"table record failed\n");
			rc = -3;
			goto exit;
		}

		/* check crc */
		crc = clc_crc32(crc32_table, UBI_CRC32_INIT, &rec,
				UBI_VTBL_RECORD_SIZE_CRC);
		if (crc != be32_to_cpu(rec.crc))
			continue;

		/* check for empty */
		base = (char *)&rec;
		ptr = base;
		while (blank &&
		       ((unsigned)(ptr - base) < UBI_VTBL_RECORD_SIZE_CRC)) {
			if (*ptr != 0)
				blank = 0;
			ptr++;
		}

		if (blank)
			continue;

		/* prep type string */
		if (rec.vol_type == UBI_VID_DYNAMIC)
			type = "dynamic\0";
		else if (rec.vol_type == UBI_VID_STATIC)
			type = "static\0";

		/* prep name string */
		rec.name[be16_to_cpu(rec.name_len)] = '\0';
		sprintf(name, "%s", rec.name);

		/* print record line to fpout */
		fprintf(fpout, "%zu\t%u\t%u\t%s\t0x%08x\t%s\n",
			i,
			be32_to_cpu(rec.reserved_pebs),
			be32_to_cpu(rec.alignment),
			type,
			be32_to_cpu(rec.crc),
			name);
	}

 exit:
	/* reset position */
	if (fsetpos(fpin, &temp) < 0)
		rc = -1;

	if (fpout != NULL)
		fclose(fpout);

	return rc;
}


/**
 * using eb chain, tries to rebuild the data of volume at vol_id, or for all
 * the known volumes, if vol_id is NULL;
 **/
static int
rebuild_volume(FILE * fpin, uint32_t *vol_id, struct eb_info **head,
	       const char *path, size_t block_size, size_t header_size)
{
	char filename[MAXPATH];
	int rc;
	uint32_t vol, num, data_size;
	FILE* fpout;
	struct eb_info *cur;

	rc = 0;

	if ((fpin == NULL) || (head == NULL) || (*head == NULL))
		return 0;

	/* when vol_id is null, then do all  */
	if (vol_id == NULL) {
		cur = *head;
		vol = be32_to_cpu(cur->vid.vol_id);
	} else {
		vol = *vol_id;
		eb_chain_position(head, vol, NULL, &cur);
		if (cur == NULL) {
			if (debug)
				ERR_MSG("no valid volume %d was found\n", vol);
			return -1;
		}
	}

	num = 0;
	snprintf(filename, MAXPATH, FN_VOLWH, path, vol);
	fpout = fopen(filename, "wb");
	if (fpout == NULL) {
		ERR_MSG("couldn't open file for writing: %s\n", filename);
		return -1;
	}

	while (cur != NULL) {
		size_t i;

		if (be32_to_cpu(cur->vid.vol_id) != vol) {
			/* close out file */
			fclose(fpout);

			/* only stay around if that was the only volume */
			if (vol_id != NULL)
				goto out;

			/* begin with next */
			vol = be32_to_cpu(cur->vid.vol_id);
			num = 0;
			snprintf(filename, MAXPATH, FN_VOLWH, path, vol);
			fpout = fopen(filename, "wb");
			if (fpout == NULL) {
				ERR_MSG("couldn't open file for writing: %s\n",
					filename);
				return -1;
			}
		}

		while (num < be32_to_cpu(cur->vid.lnum)) {
			/* FIXME haver: I hope an empty block is
			   written out so that the binary has no holes
			   ... */
			if (debug)
				ERR_MSG("missing valid block %d for volume %d\n",
					num, vol);
			num++;
		}

		rc = fsetpos(fpin, &(cur->eb_top));
		if (rc < 0)
			goto out;
		fseek(fpin, be32_to_cpu(cur->ec.data_offset), SEEK_CUR);

		if (cur->vid.vol_type == UBI_VID_DYNAMIC)
			/* FIXME It might be that alignment has influence */
			data_size = block_size - header_size;
		else
			data_size = be32_to_cpu(cur->vid.data_size);

		for (i = 0; i < data_size; i++) {
			int c = fgetc(fpin);
			if (c == EOF) {
				ERR_MSG("unexpected EOF while writing: %s\n",
					filename);
				rc = -2;
				goto out;
			}
			c = fputc(c, fpout);
			if (c == EOF) {
				ERR_MSG("couldn't write: %s\n", filename);
				rc = -3;
				goto out;
			}
		}

		cur = cur->next;
		num++;
	}

 out:
	if (vol_id == NULL)
		fclose(fpout);
	return rc;
}


/**
 * traverses FILE* trying to load complete, valid and accurate header data
 * into the eb chain;
 **/
static int
unubi_volumes(FILE* fpin, uint32_t *vols, size_t vc, struct args *a)
{
	char filename[MAXPATH + 1];
	char reason[MAXPATH + 1];
	int rc;
	size_t i, count, itable_num;
	/* relations:
	 * cur ~ head
	 * next ~ first */
	struct eb_info *head, *cur, *first, *next;
	struct eb_info **next_ptr;

	rc = 0;
	count = 0;
	itable_num = 0;
	head = NULL;
	first = NULL;
	next = NULL;
	cur = malloc(sizeof(*cur));
	if (cur == NULL) {
		ERR_MSG("out of memory\n");
		rc = -ENOMEM;
		goto err;
	}
	memset(cur, 0, sizeof(*cur));

	fgetpos(fpin, &(cur->eb_top));
	while (1) {
		const char *raw_path;
		uint32_t crc;

		cur->phys_addr = ftell(fpin);
		cur->phys_block = cur->phys_addr / a->bsize;
		cur->data_crc_ok = 0;
		cur->ec_crc_ok   = 0;
		cur->vid_crc_ok  = 0;

		memset(filename, 0, MAXPATH + 1);
		memset(reason, 0, MAXPATH + 1);

		/* in case of an incomplete ec header */
		raw_path = FN_INVAL;

		/* read erasecounter header */
		rc = fread(&cur->ec, 1, sizeof(cur->ec), fpin);
		if (rc == 0)
			goto out; /* EOF */
		if (rc != sizeof(cur->ec)) {
			ERR_MSG("reading ec-hdr failed\n");
			rc = -1;
			goto err;
		}

		/* check erasecounter header magic */
		if (be32_to_cpu(cur->ec.magic) != UBI_EC_HDR_MAGIC) {
			snprintf(reason, MAXPATH, ".invalid.ec_magic");
			goto invalid;
		}

		/* check erasecounter header crc */
		crc = clc_crc32(crc32_table, UBI_CRC32_INIT, &(cur->ec),
				UBI_EC_HDR_SIZE_CRC);
		if (be32_to_cpu(cur->ec.hdr_crc) != crc) {
			snprintf(reason, MAXPATH, ".invalid.ec_hdr_crc");
			goto invalid;
		}

		/* read volume id header */
		rc = fsetpos(fpin, &(cur->eb_top));
		if (rc != 0)
			goto err;
		fseek(fpin, be32_to_cpu(cur->ec.vid_hdr_offset), SEEK_CUR);
		rc = fread(&cur->vid, 1, sizeof(cur->vid), fpin);
		if (rc == 0)
			goto out; /* EOF */
		if (rc != sizeof(cur->vid)) {
			ERR_MSG("reading vid-hdr failed\n");
			rc = -1;
			goto err;
		}

		/* if the magic number is 0xFFFFFFFF, then it's very likely
		 * that the volume is empty */
		if (be32_to_cpu(cur->vid.magic) == 0xffffffff) {
			snprintf(reason, MAXPATH, ".empty");
			goto invalid;
		}

		/* vol_id should be in bounds */
		if ((be32_to_cpu(cur->vid.vol_id) >= UBI_MAX_VOLUMES) &&
		    (be32_to_cpu(cur->vid.vol_id) <
		     UBI_INTERNAL_VOL_START)) {
			snprintf(reason, MAXPATH, ".invalid");
			goto invalid;
		} else
			raw_path = FN_NSURE;

		/* check volume id header magic */
		if (be32_to_cpu(cur->vid.magic) != UBI_VID_HDR_MAGIC) {
			snprintf(reason, MAXPATH, ".invalid.vid_magic");
			goto invalid;
		}
		cur->ec_crc_ok = 1;

		/* check volume id header crc */
		crc = clc_crc32(crc32_table, UBI_CRC32_INIT, &(cur->vid),
				UBI_VID_HDR_SIZE_CRC);
		if (be32_to_cpu(cur->vid.hdr_crc) != crc) {
			snprintf(reason, MAXPATH, ".invalid.vid_hdr_crc");
			goto invalid;
		}
		cur->vid_crc_ok = 1;

		/* check data crc, but only for a static volume */
		if (cur->vid.vol_type == UBI_VID_STATIC) {
			rc = data_crc(fpin, be32_to_cpu(cur->vid.data_size),
				      &crc);
			if (rc < 0)
				goto err;
			if (be32_to_cpu(cur->vid.data_crc) != crc) {
				snprintf(reason, MAXPATH, ".invalid.data_crc");
				goto invalid;
			}
			cur->data_crc_ok = 1;
		}

		/* enlist this vol, it's valid */
		raw_path = FN_VALID;
		cur->linear = count;
		rc = eb_chain_insert(&head, cur);
		if (rc < 0) {
			if (rc == -ENOMEM) {
				ERR_MSG("out of memory\n");
				goto err;
			}
			ERR_MSG("unknown and unexpected error, please contact "
				CONTACT "\n");
			goto err;
		}

		/* extract info-table */
		if (a->itable &&
		    (be32_to_cpu(cur->vid.vol_id) == UBI_LAYOUT_VOLUME_ID)) {
			extract_itable(fpin, cur, a->bsize,
				       itable_num, a->odir_path);
			itable_num++;
		}

		/* split volumes */
		if (a->vol_split) {
			size_t size = 0;

			rc = fsetpos(fpin, &(cur->eb_top));
			if (rc != 0)
				goto err;

			/*
			 * FIXME For dynamic UBI volumes we must write
			 * the maximum available data. The
			 * vid.data_size field is not used in this
			 * case. The dynamic volume user is
			 * responsible for the content.
			 */
			if (a->vol_split == SPLIT_DATA) {
				/* Write only data section */
				if (cur->vid.vol_type == UBI_VID_DYNAMIC) {
					/* FIXME Formular is not
					   always right ... */
					size = a->bsize - a->hsize;
				} else
					size = be32_to_cpu(cur->vid.data_size);

				fseek(fpin,
				      be32_to_cpu(cur->ec.data_offset),
				      SEEK_CUR);
			}
			else if (a->vol_split == SPLIT_RAW)
				/* write entire eraseblock */
				size = a->bsize;

			snprintf(filename, MAXPATH, FN_VOLSP,
				 a->odir_path,
				 be32_to_cpu(cur->vid.vol_id),
				 be32_to_cpu(cur->vid.lnum),
				 be32_to_cpu(cur->vid.leb_ver), count);
			rc = extract_data(fpin, size, filename);
			if (rc < 0)
				goto err;
		}

 invalid:
		/* split eraseblocks */
		if (a->eb_split) {
			/* jump to top of block */
			rc = fsetpos(fpin, &(cur->eb_top));
			if (rc != 0)
				goto err;

			if (strcmp(raw_path, FN_INVAL) == 0)
				snprintf(filename, MAXPATH, raw_path,
					 a->odir_path, count, reason);
			else
				snprintf(filename, MAXPATH, raw_path,
					 a->odir_path,
					 count,
					 be32_to_cpu(cur->vid.vol_id),
					 be32_to_cpu(cur->vid.lnum),
					 be32_to_cpu(cur->vid.leb_ver),
					 reason);

			rc = extract_data(fpin, a->bsize, filename);
			if (rc < 0)
				goto err;
		}

		/* append to simple linked list */
		if (first == NULL)
			next_ptr = &first;
		else
			next_ptr = &next->next;

		*next_ptr = malloc(sizeof(**next_ptr));
		if (*next_ptr == NULL) {
			ERR_MSG("out of memory\n");
			rc = -ENOMEM;
			goto err;
		}
		memset(*next_ptr, 0, sizeof(**next_ptr));

		next = *next_ptr;
		memcpy(next, cur, sizeof(*next));
		next->next = NULL;

		count++;
		rc = fsetpos(fpin, &(cur->eb_top));
		if (rc != 0)
			goto err;
		fseek(fpin, a->bsize, SEEK_CUR);
		memset(cur, 0, sizeof(*cur));

		fgetpos(fpin, &(cur->eb_top));
	}

 out:
	for (i = 0; i < vc; i++) {
		rc = rebuild_volume(fpin, &vols[i], &head, a->odir_path,
			       a->bsize, a->hsize);
		if (rc < 0)
			goto err;
	}

	/* if there were no volumes specified, rebuild them all,
	 * UNLESS eb_ or vol_ split or analyze was specified */
	if ((vc == 0) && (!a->eb_split) && (!a->vol_split) &&
	    (!a->analyze) && (!a->itable)) {
		rc = rebuild_volume(fpin, NULL, &head, a->odir_path, a->bsize,
				    a->hsize);
		if (rc < 0)
			goto err;
	}

 err:
	free(cur);

	if (a->analyze) {
		char fname[PATH_MAX + 1];
		FILE *fp;

		unubi_analyze(&head, first, a->odir_path);

		/* prepare output files */
		memset(fname, 0, PATH_MAX + 1);
		snprintf(fname, PATH_MAX, "%s/%s", a->odir_path, FN_EH_STAT);
		fp = fopen(fname, "w");
		if (fp != NULL) {
			eb_chain_print(fp, head);
			fclose(fp);
		}
	}
	eb_chain_destroy(&head);
	eb_chain_destroy(&first);

	return rc;
}


/**
 * handles command line arguments, then calls unubi_volumes
 **/
int
main(int argc, char *argv[])
{
	int rc, free_a_odir;
	size_t vols_len;
	uint32_t *vols;
	FILE* fpin;
	struct args a;

	rc = 0;
	free_a_odir = 0;
	vols_len = 0;
	vols = NULL;
	fpin = NULL;
	init_crc32_table(crc32_table);

	/* setup struct args a */
	memset(&a, 0, sizeof(a));
	a.bsize = 128 * KIB;
	a.hsize = 2 * KIB;
	a.vols = malloc(sizeof(*a.vols) * UBI_MAX_VOLUMES);
	if (a.vols == NULL) {
		ERR_MSG("out of memory\n");
		rc = ENOMEM;
		goto err;
	}
	memset(a.vols, 0, sizeof(*a.vols) * UBI_MAX_VOLUMES);

	/* parse args and check for validity */
	parse_opt(argc, argv, &a);
	if (a.img_path == NULL) {
		ERR_MSG("no image file specified\n");
		rc = EINVAL;
		goto err;
	}
	else if (a.odir_path == NULL) {
		char *ptr;
		int len;

		ptr = strrchr(a.img_path, '/');
		if (ptr == NULL)
			ptr = a.img_path;
		else
			ptr++;

		len = strlen(DIR_FMT) + strlen(ptr);
		free_a_odir = 1;
		a.odir_path = malloc(sizeof(*a.odir_path) * len);
		if (a.odir_path == NULL) {
			ERR_MSG("out of memory\n");
			rc = ENOMEM;
			goto err;
		}
		snprintf(a.odir_path, len, DIR_FMT, ptr);
	}

	fpin = fopen(a.img_path, "rb");
	if (fpin == NULL) {
		ERR_MSG("couldn't open file for reading: "
			"%s\n", a.img_path);
		rc = EINVAL;
		goto err;
	}

	rc = mkdir(a.odir_path, 0777);
	if ((rc < 0) && (errno != EEXIST)) {
		ERR_MSG("couldn't create ouput directory: "
			"%s\n", a.odir_path);
		rc = -rc;
		goto err;
	}

	/* fill in vols array */
	vols_len = count_set(a.vols, UBI_MAX_VOLUMES);
	if (vols_len > 0) {
		vols = malloc(sizeof(*vols) * vols_len);
		if (vols == NULL) {
			ERR_MSG("out of memory\n");
			rc = ENOMEM;
			goto err;
		}
		collapse(a.vols, UBI_MAX_VOLUMES, vols, vols_len);
	}

	/* unubi volumes */
	rc = unubi_volumes(fpin, vols, vols_len, &a);
	if (rc < 0) {
		/* ERR_MSG("error encountered while working on image file: "
		   "%s\n", a.img_path); */
		rc = -rc;
		goto err;
	}

 err:
	free(a.vols);
	if (free_a_odir != 0)
		free(a.odir_path);
	if (fpin != NULL)
		fclose(fpin);
	if (vols_len > 0)
		free(vols);
	return rc;
}
