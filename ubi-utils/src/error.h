#ifndef __ERROR_H__
#define __ERROR_H__
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

void error_initlog(const char *logfile);
int read_procfile(FILE *fp_out, const char *procfile);

void __err_ret(const char *fmt, ...);
void __err_sys(const char *fmt, ...);
void __err_msg(const char *fmt, ...);
void __err_quit(const char *fmt, ...);
void __err_dump(const char *fmt, ...);

void info_msg(const char *fmt, ...);

#ifdef DEBUG
#define __loc_msg(str) do {					\
	__err_msg("[%s. FILE: %s FUNC: %s LINE: %d]\n",		\
		str, __FILE__, __FUNCTION__, __LINE__);		\
} while (0)
#else
#define __loc_msg(str)
#endif


#define err_dump(fmt, ...) do {					\
	__loc_msg("ErrDump");					\
	__err_dump(fmt, ##__VA_ARGS__);				\
} while (0)

#define err_quit(fmt, ...) do {					\
	__loc_msg("ErrQuit");					\
	__err_quit(fmt, ##__VA_ARGS__);				\
} while (0)


#define err_ret(fmt, ...) do {					\
	__loc_msg("ErrRet");					\
	__err_ret(fmt, ##__VA_ARGS__);				\
} while (0)

#define err_sys(fmt, ...) do {					\
	__loc_msg("ErrSys");					\
	__err_sys(fmt, ##__VA_ARGS__);				\
} while (0)

#define err_msg(fmt, ...) do {					\
	__loc_msg("ErrMsg");					\
	__err_msg(fmt, ##__VA_ARGS__);				\
} while (0)

#define log_msg(fmt, ...) do {					\
		/* __loc_msg("LogMsg");	*/			\
	__err_msg(fmt, ##__VA_ARGS__);				\
} while (0)

#ifdef DEBUG
#define dbg_msg(fmt, ...) do {					\
	__loc_msg("DbgMsg");					\
	__err_msg(fmt, ##__VA_ARGS__);				\
} while (0)
#else
#define dbg_msg(fmt, ...) do {} while (0)
#endif

#endif /* __ERROR_H__ */
