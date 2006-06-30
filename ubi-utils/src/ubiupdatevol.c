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
 *
 * 1.0 Reworked the userinterface to use argp.
 */

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

#include <config.h>
#include <libubi.h>

#define PROGRAM_VERSION "1.0"

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

static error_t parse_opt (int key, char *arg, struct argp_state *state);

static int verbose = 0;
const char *argp_program_version = PROGRAM_VERSION;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static char doc[] = "\nVersion: " PROGRAM_VERSION "\n\t"
	BUILD_OS" "BUILD_CPU" at "__DATE__" "__TIME__"\n"
	"\nWrite to UBI Volume.\n";

static struct argp_option options[] = {
	{ .name = "devn",
	  .key = 'd',
	  .arg = "<devn>",
	  .flags = 0,
	  .doc = "UBI device",
	  .group = OPTION_ARG_OPTIONAL },

	{ .name = "vol_id",
	  .key = 'n',
	  .arg = "<volume id>",
	  .flags = 0,
	  .doc = "UBI volume id",
	  .group = OPTION_ARG_OPTIONAL },

	{ .name = "truncate",
	  .key = 't',
	  .arg = NULL,
	  .flags = 0,
	  .doc = "truncate volume",
	  .group = OPTION_ARG_OPTIONAL },

	{ .name = "broken-update",
	  .key = 'B',
	  .arg = NULL,
	  .flags = 0,
	  .doc = "broken update, this is for testing",
	  .group = OPTION_ARG_OPTIONAL },

	{ .name = NULL, .key = 0, .arg = NULL, .flags = 0,
	  .doc = NULL, .group = 0 },
};

static struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.args_doc = 0,
	.doc =	doc,
	.children = NULL,
	.help_filter = NULL,
	.argp_domain = NULL,
};

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
	case 'v': /* --verbose=<level> */
		verbose = strtoul(arg, (char **)NULL, 0);
		break;

	case 'n': /* --vol_id=<volume id> */
		args->vol_id = strtol(arg, (char **)NULL, 0);
		break;

	case 'd': /* --devn=<device number> */
		args->devn = strtol(arg, (char **)NULL, 0);
		break;

	case 'b': /* --bufsize=<bufsize> */
		args->bufsize = strtol(arg, (char **)NULL, 0);
		if (args->bufsize <= 0)
			args->bufsize = BUFSIZE;
		break;

	case 't': /* --truncate */
		args->truncate = 1;
		break;

	case 'B': /* --broken-update */
		args->broken_update = 1;
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
		fprintf(stderr, "Please specify an existing file.\n");
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

	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, 0, &myargs);

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
