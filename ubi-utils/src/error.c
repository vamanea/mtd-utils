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

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <string.h>
#include "error.h"

#define MAXLINE 4096

static FILE *logfp = NULL;

static void err_doit(int, int, const char *, va_list);

int
read_procfile(FILE *fp_out, const char *procfile)
{
	FILE *fp;

	fp = fopen(procfile, "r");
	if (!fp)
		return -ENOENT;

	while(!feof(fp)) {
		int c = fgetc(fp);

		if (c == EOF)
			return 0;

		if (putc(c, fp_out) == EOF)
			return -EIO;

		if (ferror(fp))
			return -EIO;
	}
	return fclose(fp);
}

void
error_initlog(const char *logfile)
{
	logfp = fopen(logfile, "a+");
	read_procfile(logfp, "/proc/cpuinfo");
}

void
info_msg(const char *fmt, ...)
{
	FILE* fpout;
	char buf[MAXLINE + 1];
	va_list	ap;
	int n;

	fpout = stdout;

	va_start(ap, fmt);
	vsnprintf(buf, MAXLINE, fmt, ap);
	n = strlen(buf);
	strcat(buf, "\n");

	fputs(buf, fpout);
	fflush(fpout);
	if (fpout != stdout)
		fclose(fpout);

	va_end(ap);
	return;
}

void
__err_ret(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(1, LOG_INFO, fmt, ap);
	va_end(ap);
	return;
}

void
__err_sys(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(1, LOG_ERR, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}


void
__err_msg(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	err_doit(0, LOG_INFO, fmt, ap);
	va_end(ap);

	return;
}

void
__err_quit(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(0, LOG_ERR, fmt, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void
__err_dump(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(1, LOG_ERR, fmt, ap);
	va_end(ap);
	abort();		/* dump core and terminate */
	exit(EXIT_FAILURE);	/* shouldn't get here */
}


static void
err_doit(int errnoflag, int level __attribute__((unused)),
	 const char *fmt, va_list ap)
{
	FILE* fpout;
	int errno_save, n;
	char buf[MAXLINE + 1];
	fpout = stderr;

	errno_save = errno; /* value caller might want printed */

	vsnprintf(buf, MAXLINE, fmt, ap); /* safe */

	n = strlen(buf);

	if (errnoflag)
		snprintf(buf + n, MAXLINE - n, ": %s", strerror(errno_save));
	strcat(buf, "\n");

	if (logfp) {
		fputs(buf, logfp);
		fflush(logfp);
	}

	fputs(buf, fpout);
	fflush(fpout);
	if (fpout != stderr)
		fclose(fpout);

	return;
}
