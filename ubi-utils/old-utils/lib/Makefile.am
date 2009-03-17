AUTOMAKE_OPTIONS = foreign
INCLUDES=-I$(top_srcdir)/inc -I$(top_srcdir)/../../kernel/include

# -----------------------------------------------------------------------------
# all export libs which shall be generated
lib_LTLIBRARIES		= libubi.la \
			  libpfiflash.la

# -----------------------------------------------------------------------------
# all convinence libs which shall be generated
noinst_LTLIBRARIES	= libcrc32.la \
		     	  libubigen.la \
		     	  liberror.la \
		     	  liblist.la \
		     	  libbootenv.la \
		     	  libpfi.la \
		     	  libpeb.la \
			  libreader.la \
			  libubimirror.la

# -----------------------------------------------------------------------------
# exported libs
libpfiflash_la_SOURCES	= $(top_srcdir)/src/libpfiflash/pfiflash.c
libpfiflash_la_LDFLAGS  = -no-undefined -version-info 1:0:0
libpfiflash_la_LIBADD	= libreader.la \
			  libubimirror.la \
			  libubi.la

libubi_la_SOURCES	= $(top_srcdir)/src/libubi/libubi.c \
			  $(top_srcdir)/src/libubi/libubi_sysfs.c
libubi_la_LDFLAGS	= -no-undefined -version-info 1:0:0

# -----------------------------------------------------------------------------
# complex convinence libs, beware for double includes.
libreader_la_SOURCES    = $(top_srcdir)/src/libreader/reader.c 
libreader_la_LIBADD	= libpfi.la \
		    	  liblist.la  \
			  libpeb.la \
			  libbootenv.la

libubigen_la_SOURCES	= $(top_srcdir)/src/libubigen/ubigen.c
libubigen_la_LIBADD	= libcrc32.la

libbootenv_la_SOURCES 	= $(top_srcdir)/src/libbootenv/bootenv.c \
			  $(top_srcdir)/src/libbootenv/hashmap.c 
libbootenv_la_LIBADD	= libcrc32.la

libubimirror_la_SOURCES	= $(top_srcdir)/src/libubimirror/ubimirror.c 
libubimirror_la_LIBADD  = libubi.la


# -----------------------------------------------------------------------------
# simple convinence libs
libcrc32_la_SOURCES	= $(top_srcdir)/src/libcrc32/crc32.c
liberror_la_SOURCES	= $(top_srcdir)/src/liberror/error.c
liblist_la_SOURCES	= $(top_srcdir)/src/liblist/list.c
libpeb_la_SOURCES	= $(top_srcdir)/src/libpeb/peb.c
libpfi_la_SOURCES	= $(top_srcdir)/src/libpfi/pfi.c 
