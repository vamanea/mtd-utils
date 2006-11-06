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
#define MAXWIDTH 80

static FILE *logfp = NULL;

static void err_doit(int, int, const char *, va_list);

int
read_procfile(FILE *fp_out, const char *procfile)
{
	FILE *fp;

	if (!fp_out)
		return -ENXIO;

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
	if (!logfile)
		return;

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

/**
 * If a logfile is used we must not print on stderr and stdout
 * anymore. Since pfilfash might be used in a server context, it is
 * even dangerous to write to those descriptors.
 */
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
		return;		/* exit when logging completes */
	}

	if (fpout == stderr) {
		/* perform line wrap when outputting to stderr */
		int word_len, post_len, chars;
		char *buf_ptr;
		const char *frmt = "%*s%n %n";

		chars = 0;
		buf_ptr = buf;
		while (sscanf(buf_ptr, frmt, &word_len, &post_len) != EOF) {
			int i;
			char word[word_len + 1];
			char post[post_len + 1];

			strncpy(word, buf_ptr, word_len);
			word[word_len] = '\0';
			buf_ptr += word_len;
			post_len -= word_len;

			if (chars + word_len > MAXWIDTH) {
				fputc('\n', fpout);
				chars = 0;
			}
			fputs(word, fpout);
			chars += word_len;

			if (post_len > 0) {
				strncpy(post, buf_ptr, post_len);
				post[post_len] = '\0';
				buf_ptr += post_len;
			}
			for (i = 0; i < post_len; i++) {
				int inc = 1, chars_new;

				if (post[i] == '\t')
					inc = 8;
				if (post[i] == '\n') {
					inc = 0;
					chars_new = 0;
				} else
					chars_new = chars + inc;

				if (chars_new > MAXWIDTH) {
					fputc('\n', fpout);
					chars_new = inc;
				}
				fputc(post[i], fpout);
				chars = chars_new;
			}
		}
	}
	else
		fputs(buf, fpout);
	fflush(fpout);
	if (fpout != stderr)
		fclose(fpout);

	return;
}
