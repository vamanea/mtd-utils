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
 * Authors: Frank Haverkamp, haver@vnet.ibm.com
 *	    Drake Dowsett, dowsett@de.ibm.com
 *
 * 1.2 Removed argp because we want to use uClibc.
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
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mtd/ubi-header.h>

#include "crc32.h"
#include "eb_chain.h"
#include "unubi_analyze.h"

#define EXEC		"unubi"
#define CONTACT		"haver@vnet.ibm.com"
#define VERSION		"1.0"

extern char *optarg;
extern int optind;

static char doc[] = "\nVersion: " VERSION "\n\t"
	BUILD_OS" "BUILD_CPU" at "__DATE__" "__TIME__"\n"
	"\nAnalyze raw flash containing UBI data.\n";

static const char *optionsstr =
" OPTIONS\n"
"  -r, --rebuild=<volume-id>  Extract and rebuild volume\n"
"\n"
"  -d, --dir=<output-dir>     Specify output directory\n"
"\n"
"  -a, --analyze              Analyze image\n"
"\n"
"  -b, --blocksize=<block-size>   Specify size of eraseblocks in image in bytes\n"
"                             (default 128KiB)\n"
"\n"
"  -e, --eb-split             Generate individual eraseblock images (all\n"
"                             eraseblocks)\n"
"  -v, --vol-split            Generate individual eraseblock images (valid\n"
"                             eraseblocks only)\n"
"  -V, --vol-split!           Raw split by eraseblock (valid eraseblocks only)\n"
"\n"
"  -?, --help                 Give this help list\n"
"      --usage                Give a short usage message\n"
"      --version              Print program version\n";

static const char *usage =
"Usage: unubi [-aevV?] [-r <volume-id>] [-d <output-dir>] [-b <block-size>]\n"
"            [--rebuild=<volume-id>] [--dir=<output-dir>] [--analyze]\n"
"            [--blocksize=<block-size>] [--eb-split] [--vol-split]\n"
"            [--vol-split!] [--help] [--usage] [--version] image-file\n";

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
#define FN_VOLSP	"%s/vol%03u_%03u_%03u_%04u"	/* split volume */
#define FN_VOLWH	"%s/volume%03u"			/* whole volume */

static uint32_t crc32_table[256];

/* struct args:
 *	bsize		int, blocksize of image blocks
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
	uint32_t bsize;
	int analyze;
	int eb_split;
	int vol_split;
	char *odir_path;
	char *img_path;
	uint32_t *vols;

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
		if ((strcmp(s, "MiB") == 0) || (strcmp(s, "M") == 0) ||
		    (strcmp(s, "mib") == 0) || (strcmp(s, "m") == 0))
			num *= MIB;
		else
			ERR_MSG("couldn't parse '%s', assuming %lu\n",
				s, num);
	}
	return num;
}


/**
 * parses the arguments passed into the program
 * get the input argument from argp_parse, which we know is a
 * pointer to our arguments structure;
 **/
static int
parse_opt(int argc, char **argv, struct args *args)
{
	uint32_t i;

	while (1) {
		int key;

		key = getopt_long(argc, argv, "r:d:ab:evV?", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
			case 'a':
				args->analyze = 1;
				break;
			case 'b':
				args->bsize = str_to_num(optarg);
				break;
			case 'd':
				args->odir_path = optarg;
				break;
			case 'e':
				args->eb_split = SPLIT_RAW;
				break;
			case 'r':
				i = str_to_num(optarg);
				if (i < UBI_MAX_VOLUMES)
					args->vols[str_to_num(optarg)] = 1;
				else {
					ERR_MSG("volume-id out of bounds\n");
					return -1;
				}
				break;
			case 'v':
				if (args->vol_split != SPLIT_RAW)
					args->vol_split = SPLIT_DATA;
				break;
			case 'V':
				args->vol_split = SPLIT_RAW;
				break;
			case '?': /* help */
				fprintf(stderr, "Usage: unubi [OPTION...] image-file\n");
				fprintf(stderr, "%s", doc);
				fprintf(stderr, "%s", optionsstr);
				fprintf(stderr, "\nReport bugs to %s\n", CONTACT);
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
 * header_crc: calculate the crc of EITHER a eb_hdr or vid_hdr
 * one of the first to args MUST be NULL, the other is the header
 *	to caculate the crc on
 * always returns 0
 **/
static int
header_crc(struct ubi_ec_hdr *ebh, struct ubi_vid_hdr *vidh, uint32_t *ret_crc)
{
	uint32_t crc = UBI_CRC32_INIT;

	if (ret_crc == NULL)
		return 0;

	if ((ebh != NULL) && (vidh == NULL))
		crc = clc_crc32(crc32_table, crc, ebh, UBI_EC_HDR_SIZE_CRC);
	else if ((ebh == NULL) && (vidh != NULL))
		crc = clc_crc32(crc32_table, crc, vidh, UBI_VID_HDR_SIZE_CRC);
	else
		return 0;

	*ret_crc = crc;
	return 0;
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
 * using eb chain, tries to rebuild the data of volume at vol_id, or for all
 * the known volumes, if vol_id is NULL;
 **/
static int
rebuild_volume(FILE* fpin, uint32_t *vol_id, eb_info_t *head, const char* path)
{
	char filename[MAXPATH];
	int rc;
	uint32_t vol, num;
	FILE* fpout;
	eb_info_t cur;

	rc = 0;

	if ((fpin == NULL) || (head == NULL) || (*head == NULL))
		return 0;

	/* when vol_id is null, then do all  */
	if (vol_id == NULL) {
		cur = *head;
		vol = ubi32_to_cpu(cur->inner.vol_id);
	}
	else {
		vol = *vol_id;
		eb_chain_position(head, vol, NULL, &cur);
		if (cur == NULL) {
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

		if (ubi32_to_cpu(cur->inner.vol_id) != vol) {
			/* close out file */
			fclose(fpout);

			/* only stay around if that was the only volume */
			if (vol_id != NULL)
				goto out;

			/* begin with next */
			vol = ubi32_to_cpu(cur->inner.vol_id);
			num = 0;
			snprintf(filename, MAXPATH, FN_VOLWH, path, vol);
			fpout = fopen(filename, "wb");
			if (fpout == NULL) {
				ERR_MSG("couldn't open file for writing: %s\n",
					filename);
				return -1;
			}
		}

		if (ubi32_to_cpu(cur->inner.lnum) != num) {
			ERR_MSG("missing valid block %d for volume %d\n",
				num, vol);
		}

		rc = fsetpos(fpin, &(cur->eb_top));
		if (rc < 0)
			goto out;
		fseek(fpin, ubi32_to_cpu(cur->outer.data_offset), SEEK_CUR);

		for (i = 0; i < ubi32_to_cpu(cur->inner.data_size); i++) {
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
	size_t i, count;
	/* relations:
	 * cur ~ head
	 * next ~ first */
	eb_info_t head, cur, first, next;
	eb_info_t *next_ptr;

	rc = 0;
	count = 0;
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

		memset(filename, 0, MAXPATH + 1);
		memset(reason, 0, MAXPATH + 1);

		/* in case of an incomplete ec header */
		raw_path = FN_INVAL;

		/* read erasecounter header */
		rc = fread(&cur->outer, 1, sizeof(cur->outer), fpin);
		if (rc == 0)
			goto out; /* EOF */
		if (rc != sizeof(cur->outer)) {
			ERR_MSG("reading ec-hdr failed rc=%d\n", rc);
			rc = -1;
			goto err;
		}

		/* check erasecounter header magic */
		if (ubi32_to_cpu(cur->outer.magic) != UBI_EC_HDR_MAGIC) {
			snprintf(reason, MAXPATH, ".invalid.ec_magic");
			goto invalid;
		}

		/* check erasecounter header crc */
		header_crc(&(cur->outer), NULL, &crc);

		if (ubi32_to_cpu(cur->outer.hdr_crc) != crc) {
			snprintf(reason, MAXPATH, ".invalid.ec_hdr_crc");
			goto invalid;
		}

		/* read volume id header */
		rc = fsetpos(fpin, &(cur->eb_top));
		if (rc != 0)
			goto err;
		fseek(fpin, ubi32_to_cpu(cur->outer.vid_hdr_offset), SEEK_CUR);

		/* read erasecounter header */
		rc = fread(&cur->inner, 1, sizeof(cur->inner), fpin);
		if (rc == 0)
			goto out; /* EOF */
		if (rc != sizeof(cur->inner)) {
			ERR_MSG("reading vid-hdr failed rc=%d\n", rc);
			rc = -1;
			goto err;
		}

		/* empty? */
		if (ubi32_to_cpu(cur->inner.magic) == 0xffffffff) {
			snprintf(reason, MAXPATH, ".empty");
			goto invalid;
		}

		/* vol_id should be in bounds */
		if ((ubi32_to_cpu(cur->inner.vol_id) >= UBI_MAX_VOLUMES) &&
		    (ubi32_to_cpu(cur->inner.vol_id) <
		     UBI_INTERNAL_VOL_START)) {
			snprintf(reason, MAXPATH, ".invalid");
			goto invalid;
		} else
			raw_path = FN_NSURE;

		/* check volume id header magic */
		if (ubi32_to_cpu(cur->inner.magic) != UBI_VID_HDR_MAGIC) {
			snprintf(reason, MAXPATH, ".invalid.vid_magic");
			goto invalid;
		}

		/* check volume id header crc */
		header_crc(NULL, &(cur->inner), &crc);
		if (ubi32_to_cpu(cur->inner.hdr_crc) != crc) {
			snprintf(reason, MAXPATH, ".invalid.vid_hdr_crc");
			goto invalid;
		}

		/* check data crc */
		rc = data_crc(fpin, ubi32_to_cpu(cur->inner.data_size), &crc);
		if (rc < 0)
			goto err;
		if (ubi32_to_cpu(cur->inner.data_crc) != crc) {
			snprintf(reason, MAXPATH, ".invalid.data_crc");
			goto invalid;
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

		if (a->vol_split) {
			size_t size = 0;

			rc = fsetpos(fpin, &(cur->eb_top));
			if (rc != 0)
				goto err;

			if (a->vol_split == SPLIT_DATA) {
				/* write only data section */
				size = ubi32_to_cpu(cur->inner.data_size);
				fseek(fpin,
				      ubi32_to_cpu(cur->outer.data_offset),
				      SEEK_CUR);
			}
			else if (a->vol_split == SPLIT_RAW)
				/* write entire eraseblock */
				size = a->bsize;

			snprintf(filename, MAXPATH, FN_VOLSP,
				 a->odir_path,
				 ubi32_to_cpu(cur->inner.vol_id),
				 ubi32_to_cpu(cur->inner.lnum),
				 ubi32_to_cpu(cur->inner.leb_ver), count);
			rc = extract_data(fpin, size, filename);
			if (rc < 0)
				goto err;
		}

 invalid:
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
					 ubi32_to_cpu(cur->inner.vol_id),
					 ubi32_to_cpu(cur->inner.lnum),
					 ubi32_to_cpu(cur->inner.leb_ver),
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
		rc = rebuild_volume(fpin, &vols[i], &head, a->odir_path);
		if (rc < 0)
			goto err;
	}

	/* if there were no volumes specified, rebuild them all,
	 * UNLESS eb_ or vol_ split or analyze was specified */
	if ((vc == 0) && (!a->eb_split) && (!a->vol_split) && (!a->analyze)) {
		rc = rebuild_volume(fpin, NULL, &head, a->odir_path);
		if (rc < 0)
			goto err;
	}

 err:
	free(cur);

	if (a->analyze)
		unubi_analyze(&head, first, a->odir_path);
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
		ERR_MSG("error encountered while working on image file: "
			"%s\n", a.img_path);
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
