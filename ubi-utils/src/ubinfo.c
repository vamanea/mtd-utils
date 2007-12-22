/*
 * Copyright (C) 2007 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Author: Artem Bityutskiy
 */

/*
 * An utility to get UBI information.
 */
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libubi.h>
#include "common.h"

#define PROGRAM_VERSION "1.0"
#define PROGRAM_NAME    "ubinfo"

/* The variables below is set by command line arguments */
struct args {
	int devn;
	int vol_id;
	int all;
	char node[MAX_NODE_LEN + 2];
	int node_given;
};

static struct args myargs = {
	.vol_id = -1,
	.devn = -1,
	.all = 0,
	.node_given = 0,
};

static const char *doc = "Version " PROGRAM_VERSION "\n"
	PROGRAM_NAME " - a tool to UBI information.";

static const char *optionsstr =
"-d, --devn=<UBI device number>  UBI device number to get information about\n"
"-n, --vol_id=<volume ID>        ID of UBI volume to print information about\n"
"-a, --all                       print information about all devices and volumes,\n"
"                                or about all volumes if the UBI device was\n"
"                                specified\n"
"-h, --help                      print help message\n"
"-V, --version                   print program version";

static const char *usage =
"Usage 1: " PROGRAM_NAME " [-d <UBI device number>] [-n <volume ID>] [-a] [-h] [-V] [--vol_id=<volume ID>]\n"
"\t\t[--devn <UBI device number>] [--all] [--help] [--version]\n"
"Usage 2: " PROGRAM_NAME " <UBI device node file name> [-a] [-h] [-V] [--all] [--help] [--version]\n"
"Usage 3: " PROGRAM_NAME " <UBI volume node file name> [-h] [-V] [--help] [--version]\n\n"
"Example 1: " PROGRAM_NAME " - (no arguments) print general UBI information\n"
"Example 2: " PROGRAM_NAME " -d 1 - print information about UBI device number 1\n"
"Example 3: " PROGRAM_NAME " /dev/ubi0 -a - print information about all volumes of UBI\n"
"           device /dev/ubi0\n"
"Example 4: " PROGRAM_NAME " /dev/ubi1_0 - print information about UBI volume /dev/ubi1_0\n"
"Example 5: " PROGRAM_NAME " -a - print all information\n";

static const struct option long_options[] = {
	{ .name = "devn",      .has_arg = 1, .flag = NULL, .val = 'd' },
	{ .name = "vol_id",    .has_arg = 1, .flag = NULL, .val = 'n' },
	{ .name = "all",       .has_arg = 0, .flag = NULL, .val = 'a' },
	{ .name = "help",      .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",   .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0},
};

static int parse_opt(int argc, char * const argv[], struct args *args)
{
	char *endp;

	while (1) {
		int key;

		key = getopt_long(argc, argv, "an:d:hV", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'a':
			args->all = 1;
			break;

		case 'n':
			args->vol_id = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || args->vol_id < 0) {
				errmsg("bad volume ID: " "\"%s\"", optarg);
				return -1;
			}
			break;

		case 'd':
			args->devn = strtoul(optarg, &endp, 0);
			if (*endp != '\0' || endp == optarg || args->devn < 0) {
				errmsg("bad UBI device number: \"%s\"", optarg);
				return -1;
			}

			break;

		case 'h':
			fprintf(stderr, "%s\n\n", doc);
			fprintf(stderr, "%s\n\n", usage);
			fprintf(stderr, "%s\n", optionsstr);
			exit(0);

		case 'V':
			fprintf(stderr, "%s\n", PROGRAM_VERSION);
			exit(0);

		case ':':
			errmsg("parameter is missing");
			return -1;

		default:
			fprintf(stderr, "Use -h for help\n");
			exit(-1);
		}
	}

	return 0;
}

static int translate_dev(libubi_t libubi, const char *node)
{
	int err;

	if (strlen(node) > MAX_NODE_LEN) {
		errmsg("too long device node name: \"%s\" (%d characters), max. is %d",
		       node, strlen(node), MAX_NODE_LEN);
		return -1;
	}

	err = ubi_node_type(libubi, node);
	if (err == -1) {
		if (errno) {
			errmsg("unrecognized device node \"%s\"", node);
			perror("ubi_node_type");
			return -1;
		}
		errmsg("\"%s\" does not correspond to any UBI device or volume",
		       node);
		return -1;
	}

	if (err == 1) {
		struct ubi_dev_info dev_info;

		err = ubi_get_dev_info(libubi, node, &dev_info);
		if (err) {
			errmsg("cannot get information about UBI device \"%s\"",
			       node);
			perror("ubi_get_dev_info");
			return -1;
		}

		myargs.devn = dev_info.dev_num;
	} else {
		struct ubi_vol_info vol_info;

		err = ubi_get_vol_info(libubi, node, &vol_info);
		if (err) {
			errmsg("cannot get information about UBI volume \"%s\"",
			       node);
			perror("ubi_get_vol_info");
			return -1;
		}

		if (myargs.vol_id != -1) {
			errmsg("both volume character device node (\"%s\") and "
			       "volume ID (%d) are specify, use only one of them"
			       "(use -h for help)", node, myargs.vol_id);
			return -1;
		}

		myargs.devn = vol_info.dev_num;
		myargs.vol_id = vol_info.vol_id;
	}

	return 0;
}

static int print_vol_info(libubi_t libubi, int dev_num, int vol_id)
{
	int err;
	struct ubi_vol_info vol_info;

	err = ubi_get_vol_info1(libubi, dev_num, vol_id, &vol_info);
	if (err) {
		errmsg("cannot get information about UBI volume %d on ubi%d",
		       vol_id, dev_num);
		perror("ubi_get_vol_info1");
		return -1;
	}

	printf("Volume ID:   %d (on ubi%d)\n", vol_info.vol_id, vol_info.dev_num);
	printf("Type:        %s\n",
	       vol_info.type == UBI_DYNAMIC_VOLUME ?  "dynamic" : "static");
	printf("Alignment:   %d\n", vol_info.alignment);

	printf("Size:        %d LEBs (", vol_info.rsvd_ebs);
	ubiutils_print_bytes(vol_info.rsvd_bytes, 0);
	printf(")\n");

	if (vol_info.type == UBI_STATIC_VOLUME) {
		printf("Data bytes:  ");
		ubiutils_print_bytes(vol_info.data_bytes, 1);
	}
	printf("State:       %s\n", vol_info.corrupted ? "corrupted" : "OK");
	printf("Name:        %s\n", vol_info.name);
	printf("Character device major/minor: %d:%d\n",
	       vol_info.major, vol_info.minor);

	return 0;
}

static int print_dev_info(libubi_t libubi, int dev_num, int all)
{
	int i, err, first = 1;
	struct ubi_dev_info dev_info;
	struct ubi_vol_info vol_info;

	err = ubi_get_dev_info1(libubi, dev_num, &dev_info);
	if (err) {
		errmsg("cannot get information about UBI device %d", dev_num);
		perror("ubi_get_dev_info1");
		return -1;
	}

	printf("ubi%d:\n", dev_info.dev_num);
	printf("Volumes count:                           %d\n", dev_info.vol_count);
	printf("Logical eraseblock size:                 %d\n", dev_info.eb_size);

	printf("Total amount of logical eraseblocks:     %d (", dev_info.total_ebs);
	ubiutils_print_bytes(dev_info.total_bytes, 0);
	printf(")\n");

	printf("Amount of available logical eraseblocks: %d", dev_info.avail_ebs);
	ubiutils_print_bytes(dev_info.avail_bytes, 0);
	printf(")\n");

	printf("Maximum count of volumes                 %d\n", dev_info.max_vol_count);
	printf("Count of bad physical eraseblocks:       %d\n", dev_info.bad_count);
	printf("Count of reserved physical eraseblocks:  %d\n", dev_info.bad_rsvd);
	printf("Current maximum erase counter value:     %lld\n", dev_info.max_ec);
	printf("Minimum input/output unit size:          %d bytes\n", dev_info.min_io_size);
	printf("Character device major/minor:            %d:%d\n",
	       dev_info.major, dev_info.minor);

	if (dev_info.vol_count == 0)
		return 0;

	printf("Present volumes:                         ");
	for (i = dev_info.lowest_vol_num;
	     i <= dev_info.highest_vol_num; i++) {
		err = ubi_get_vol_info1(libubi, dev_info.dev_num, i, &vol_info);
		if (err == -1) {
			if (errno == ENOENT)
				continue;

			errmsg("libubi failed to probe volume %d on ubi%d",
			       i, dev_info.dev_num);
			perror("ubi_get_vol_info1");
			return -1;
		}

		if (!first)
			printf(", %d", i);
		else {
			printf("%d", i);
			first = 0;
		}
	}
	printf("\n");

	if (!all)
		return 0;

	first = 1;
	printf("\n");

	for (i = dev_info.lowest_vol_num;
	     i <= dev_info.highest_vol_num; i++) {
		if(!first)
			printf("-----------------------------------\n");
		err = ubi_get_vol_info1(libubi, dev_info.dev_num, i, &vol_info);
		if (err == -1) {
			if (errno == ENOENT)
				continue;

			errmsg("libubi failed to probe volume %d on ubi%d",
			       i, dev_info.dev_num);
			perror("ubi_get_vol_info1");
			return -1;
		}
		first = 0;

		err = print_vol_info(libubi, dev_info.dev_num, i);
		if (err)
			return err;
	}

	return 0;
}

static int print_general_info(libubi_t libubi, int all)
{
	int i, err, first = 1;
	struct ubi_info ubi_info;
	struct ubi_dev_info dev_info;

	err = ubi_get_info(libubi, &ubi_info);
	if (err) {
		errmsg("cannot get UBI information");
		perror("ubi_get_info");
		return -1;
	}

	printf("UBI version:                    %d\n", ubi_info.version);
	printf("Count of UBI devices:           %d\n", ubi_info.dev_count);
	printf("UBI control device major/minor: %d:%d\n",
	       ubi_info.ctrl_major, ubi_info.ctrl_minor);

	if (ubi_info.dev_count == 0)
		return 0;

	printf("Present UBI devices:            ");
	for (i = ubi_info.lowest_dev_num;
	     i <= ubi_info.highest_dev_num; i++) {
		err = ubi_get_dev_info1(libubi, i, &dev_info);
		if (err == -1) {
			if (errno == ENOENT)
				continue;

			errmsg("libubi failed to probe UBI device %d", i);
			perror("ubi_get_dev_info1");
			return -1;
		}

		if (!first)
			printf(", ubi%d", i);
		else {
			printf("ubi%d", i);
			first = 0;
		}
	}
	printf("\n");

	if (!all)
		return 0;

	first = 1;
	printf("\n");

	for (i = ubi_info.lowest_dev_num;
	     i <= ubi_info.highest_dev_num; i++) {
		if(!first)
			printf("\n===================================\n\n");
		err = ubi_get_dev_info1(libubi, i, &dev_info);
		if (err == -1) {
			if (errno == ENOENT)
				continue;

			errmsg("libubi failed to probe UBI device %d", i);
			perror("ubi_get_dev_info1");
			return -1;
		}
		first = 0;

		err = print_dev_info(libubi, i, all);
		if (err)
			return err;
	}
	return 0;
}

int main(int argc, char * const argv[])
{
	int err;
	libubi_t libubi;

	if (argc > 1 && argv[1][0] != '-') {
		strncpy(myargs.node, argv[1], MAX_NODE_LEN + 1);
		myargs.node_given = 1;
	}

	if (argc > 2 && argv[2][0] != '-') {
		errmsg("incorrect arguments, use -h for help");
		return -1;
	}

	err = parse_opt(argc, argv, &myargs);
	if (err)
		return -1;

	if (argc > 1 && !myargs.node_given && myargs.devn != -1) {
		errmsg("specify either device number or node file (use -h for help)");
		return -1;
	}

	libubi = libubi_open();
	if (libubi == NULL) {
		errmsg("cannot open libubi");
		perror("libubi_open");
		return -1;
	}

	if (myargs.node_given) {
		/*
		 * A character device was specified, translate this into UBI
		 * device number and volume ID.
		 */
		err = translate_dev(libubi, myargs.node);
		if (err)
			goto out_libubi;
	}

	if (myargs.vol_id != -1 && myargs.devn == -1) {
		errmsg("volume ID is specified, but UBI device number is not "
		       "(use -h for help)\n");
		goto out_libubi;
	}

	if (myargs.devn != -1 && myargs.vol_id != -1) {
		print_vol_info(libubi, myargs.devn, myargs.vol_id);
		goto out;
	}

	if (myargs.devn == -1 && myargs.vol_id == -1)
		err = print_general_info(libubi, myargs.all);
	else if (myargs.devn != -1 && myargs.vol_id == -1)
		err = print_dev_info(libubi, myargs.devn, myargs.all);

	if (err)
		goto out_libubi;

out:
	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}
