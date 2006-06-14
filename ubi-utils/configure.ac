# Don't remove this check.
AC_PREREQ(2.59)

# AC_INIT: Package, Version, Bugs
AC_INIT([flashutils],[0.1],[arnez@de.ibm.com])
AC_CONFIG_HEADERS([inc/config.h:inc/config-h.in])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([1.8 foreign])
AM_MAINTAINER_MODE

# Check debug options
AS_HELP_STRING
AC_ARG_ENABLE([debug],
	      AS_HELP_STRING([--enable-debug],
			     [build with debug information [default=yes]]),,enable_debug="yes")

# Check for programs.
AM_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for header files.
AC_HEADER_STDC
# FIXME: Use AC_CHECK_HEADERS for UBI stuff.
# AC_CHECK_HEADERS([errno.h mtd/ubi-user.h])
AC_CHECK_HEADERS([errno.h])

# Set build flags
if test "x$enable_debug" = "xyes"; then
	CFLAGS="$CFLAGS -g -DDEBUG "
fi

AC_DEFINE_UNQUOTED(HOST_OS, "${host}", [Host OS])
AC_DEFINE_UNQUOTED(HOST_CPU, "${host_os}", [Host CPU])
AC_DEFINE_UNQUOTED(BUILD_OS, "${build_os}", [Build OS])
AC_DEFINE_UNQUOTED(BUILD_CPU, "${build_cpu}", [Build CPU])

# Additional Config
AC_C_BIGENDIAN

# CFLAGS
CFLAGS="-std=gnu99 -Wundef -Wall $CFLAGS"

# Init output.
AC_CONFIG_FILES([
	   Makefile
	   lib/Makefile
	   inc/Makefile
])

AC_OUTPUT
