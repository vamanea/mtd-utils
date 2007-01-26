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
 * An utility to update UBI volumes.
 *
 * Author: Frank Haverkamp
 *         Joshua W. Boyer
 *
 * 1.0 Reworked the userinterface to use argp.
 * 1.1 Removed argp parsing because we want to use uClib.
 * 1.2 Minor cleanups
 */

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

#include <config.h>
#include <libubi.h>

#define PROGRAM_VERSION "1.2"

#define MAXPATH		1024
#define BUFSIZE		128 * 1024
#define MIN(x,y)	((x)<(y)?(x):(y))

struct args {
	int devn;
	int vol_id;
	int truncate;
	int broken_update;
	int bufsize;

	/* special stuff needed to get additional arguments */
	char *arg1;
	char **options;		/* [STRING...] */
};

static struct args myargs = {
	.devn = -1,
	.vol_id = -1,
	.truncate = 0,
	.broken_update = 0,
	.bufsize = BUFSIZE,
	.arg1 = NULL,
	.options = NULL,
};

static int verbose = 0;

static char doc[] = "\nVersion: " PROGRAM_VERSION "\n\t"
	BUILD_OS" "BUILD_CPU" at "__DATE__" "__TIME__"\n"
	"\nWrite to UBI Volume.\n";

static const char *optionsstr =
"  -B, --broken-update        broken update, this is for testing\n"
"  -d, --devn=<devn>          UBI device\n"
"  -n, --vol_id=<volume id>   UBI volume id\n"
"  -t, --truncate             truncate volume\n"
"  -?, --help                 Give this help list\n"
"      --usage                Give a short usage message\n"
"  -V, --version              Print program version\n";

static const char *usage =
"Usage: ubiupdatevol [-Bt?V] [-d <devn>] [-n <volume id>] [--broken-update]\n"
"            [--devn=<devn>] [--vol_id=<volume id>] [--truncate] [--help]\n"
"            [--usage] [--version] <image file>\n";

struct option long_options[] = {
	{ .name = "broken-update", .has_arg = 0, .flag = NULL, .val = 'B' },
	{ .name = "devn", .has_arg = 1, .flag = NULL, .val = 'd' },
	{ .name = "vol_id", .has_arg = 1, .flag = NULL, .val = 'n' },
	{ .name = "truncate", .has_arg = 0, .flag = NULL, .val = 't' },
	{ .name = "help", .has_arg = 0, .flag = NULL, .val = '?' },
	{ .name = "usage", .has_arg = 0, .flag = NULL, .val = 0 },
	{ .name = "version", .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0}
};

/*
 * @brief Parse the arguments passed into the test case.
 */
static int
parse_opt(int argc, char **argv, struct args *args)
{
	while (1) {
		int key;

		key = getopt_long(argc, argv, "Bd:n:t?V", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
			case 'v': /* --verbose=<level> */
				verbose = strtoul(optarg, (char **)NULL, 0);
				break;

			case 'n': /* --vol_id=<volume id> */
				args->vol_id = strtol(optarg, (char **)NULL, 0);
				break;

			case 'd': /* --devn=<device number> */
				args->devn = strtol(optarg, (char **)NULL, 0);
				break;

			case 'b': /* --bufsize=<bufsize> */
				args->bufsize = strtol(optarg, (char **)NULL, 0);
				if (args->bufsize <= 0)
					args->bufsize = BUFSIZE;
				break;

			case 't': /* --truncate */
				args->truncate = 1;
				break;

			case 'B': /* --broken-update */
				args->broken_update = 1;
				break;

			case '?': /* help */
				fprintf(stderr,	"Usage: "
					"ubiupdatevol [OPTION...] <image file>\n%s%s"
					"\nReport bugs to %s\n",
					doc, optionsstr, PACKAGE_BUGREPORT);
				exit(EXIT_SUCCESS);
				break;

			case 'V':
				fprintf(stderr, "%s\n", PROGRAM_VERSION);
				exit(0);
				break;

			default:
				fprintf(stderr, "%s", usage);
				exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) {
		/* only one additional argument required */
		args->arg1 = argv[optind++];
	}
	return 0;
}

/**
 * @bytes bytes must be always 0, if not 0 this is a testcase for a
 * broken volume update where data is promissed to be written, but for
 * some reason nothing is written. The volume is unusable after this.
 */
static int
ubi_truncate_volume(struct args *args, int64_t bytes)
{
	int rc, ofd;
	char path[MAXPATH];
	int old_errno;

	snprintf(path, MAXPATH-1, "/dev/ubi%d_%d", args->devn, args->vol_id);
	path[MAXPATH-1] = '\0';

	ofd = open(path, O_RDWR);
	if (ofd < 0) {
		fprintf(stderr, "Cannot open volume %s\n", path);
		exit(EXIT_FAILURE);
	}
	rc = ioctl(ofd, UBI_IOCVOLUP, &bytes);
	old_errno = errno;
	if (rc < 0) {
		perror("UBI volume update ioctl");
		fprintf(stderr, "    rc=%d errno=%d\n", rc, old_errno);
		exit(EXIT_FAILURE);
	}
	close(ofd);
	return 0;
}

static ssize_t ubi_write(int fd, const void *buf, size_t count)
{
	int rc;
	int len = count;

	while (len) {
		rc = write(fd, buf, len);
		if (rc == -1) {
			if (errno == EINTR)
				continue; /* try again */
			perror("write error");
			return rc;
		}

		len -= rc;
		buf += rc;
	}
	return count;
}

static int
ubi_update_volume(struct args *args)
{
	int rc, ofd;
	FILE *ifp = NULL;
	struct stat st;
	int size = 0;
	char *fname = args->arg1;
	char path[MAXPATH];
	char *buf;
	int64_t bytes = 0;
	int old_errno;

	buf = malloc(args->bufsize);
	if (!buf) {
		perror("Out of memory");
		exit(EXIT_FAILURE);
	}

	if (fname == NULL) {
		fprintf(stderr, "Please specify an existing image file.\n");
		exit(EXIT_FAILURE);
	}

	rc = stat(fname, &st);
	if (rc < 0) {
		fprintf(stderr, "Cannot stat input file %s\n", fname);
		exit(EXIT_FAILURE);
	}
	bytes = size = st.st_size;

	ifp = fopen(fname, "r");
	if (!ifp)
		exit(EXIT_FAILURE);

	snprintf(path, MAXPATH-1, "/dev/ubi%d_%d", args->devn, args->vol_id);
	path[MAXPATH-1] = '\0';

	ofd = open(path, O_RDWR);
	if (ofd < 0) {
		fprintf(stderr, "Cannot open UBI volume %s\n", path);
		exit(EXIT_FAILURE);
	}

	rc = ioctl(ofd, UBI_IOCVOLUP, &bytes);
	old_errno = errno;
	if (rc < 0) {
		perror("UBI volume update ioctl");
		fprintf(stderr, "    rc=%d errno=%d\n", rc, old_errno);
		exit(EXIT_FAILURE);
	}

	while (size > 0) {
		ssize_t tocopy = MIN(args->bufsize, size);

		rc = fread(buf, tocopy, 1, ifp);
		if (rc != 1) {
			perror("Could not read everything.");
			exit(EXIT_FAILURE);
		}

		rc = ubi_write(ofd, buf, tocopy);
		old_errno = errno;
		if (rc != tocopy) {
			perror("Could not write to device");
			fprintf(stderr, "    rc=%d errno=%d\n", rc, old_errno);
			exit(EXIT_FAILURE);
		}
		size -= tocopy;
	}

	free(buf);
	fclose(ifp);
	rc = close(ofd);
	if (rc != 0) {
		perror("UBI volume close failed");
		exit(EXIT_FAILURE);
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	int rc;

	parse_opt(argc, argv, &myargs);

	if (myargs.truncate) {
		rc = ubi_truncate_volume(&myargs, 0LL);
		if (rc < 0)
			exit(EXIT_FAILURE);
		exit(EXIT_SUCCESS);
	}
	if (myargs.broken_update) {
		rc = ubi_truncate_volume(&myargs, 1LL);
		if (rc < 0)
			exit(EXIT_FAILURE);
		exit(EXIT_SUCCESS);
	}
	rc = ubi_update_volume(&myargs);
	if (rc < 0)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
