AUTOMAKE_OPTIONS = foreign
SUBDIRS=lib inc  


# -----------------------------------------------------------------------------
#  Scripts (Perl/Bash) which shall be installed.
#
bin_SCRIPTS		= $(top_srcdir)/src/mkpfi/mkpfi

# -----------------------------------------------------------------------------
#  C programs which shall be build and installed.
#  
INCLUDES=-I$(top_srcdir)/inc -I$(top_srcdir)/../../kernel/include

bin_PROGRAMS		= bin/pfi2bin \
			  bin/bin2nand \
			  bin/ubicrc32 \
			  bin/mkbootenv \
			  bin/ubimirror \
			  bin/ubimkvol \
			  bin/ubirmvol \
			  bin/ubiwritevol \
			  bin/pfiflash \
			  bin/pddcustomize \
			  bin/ubiinfo \
			  bin/nand2bin \
			  bin/unubi

# -----------------------------------------------------------------------------
#  C programs which shall be build and NOT installed. (FLD dependency...)
#  
noinst_PROGRAMS	=    	  bin/ubigen

bin_ubigen_SOURCES 	= $(top_srcdir)/src/ubigen/ubigen_main.c
bin_ubigen_LDADD   	= $(top_builddir)/lib/libubigen.la \
			  $(top_builddir)/lib/libcrc32.la 

bin_pfiflash_SOURCES 	= $(top_srcdir)/src/pfiflash/pfiflash.c 
bin_pfiflash_LDADD   	= $(top_builddir)/lib/libpfiflash.la \
			  $(top_builddir)/lib/liberror.la 
bin_pfiflash_LDFLAGS	= -static

bin_pddcustomize_SOURCES= $(top_srcdir)/src/pddcustomize/pddcustomize.c 
bin_pddcustomize_LDADD	= $(top_builddir)/lib/libbootenv.la \
			  $(top_builddir)/lib/liberror.la \
			  $(top_builddir)/lib/libubi.la \
			  $(top_builddir)/lib/libubimirror.la
bin_pddcustomize_LDFLAGS= -static

bin_pfi2bin_SOURCES 	= $(top_srcdir)/src/pfi2bin/pfi2bin.c
bin_pfi2bin_LDADD   	= $(top_builddir)/lib/libubigen.la \
			  $(top_builddir)/lib/liberror.la \
			  $(top_builddir)/lib/liblist.la \
			  $(top_builddir)/lib/libreader.la 

bin_bin2nand_SOURCES 	= $(top_srcdir)/src/bin2nand/bin2nand.c \
			  $(top_srcdir)/src/bin2nand/nandecc.c 
bin_bin2nand_LDADD   	= $(top_builddir)/lib/liberror.la 


bin_ubicrc32_SOURCES 	= $(top_srcdir)/src/ubicrc32/ubicrc32.c 
bin_ubicrc32_LDADD   	= $(top_builddir)/lib/libcrc32.la 

bin_mkbootenv_SOURCES 	= $(top_srcdir)/src/mkbootenv/mkbootenv.c 
bin_mkbootenv_LDADD   	= $(top_builddir)/lib/libbootenv.la \
			  $(top_builddir)/lib/liberror.la 


bin_ubimirror_SOURCES	= $(top_srcdir)/src/ubimirror/ubimirror.c 
bin_ubimirror_LDADD   	= $(top_builddir)/lib/liberror.la \
			  $(top_builddir)/lib/libubimirror.la 
bin_ubimirror_LDFLAGS= -static

bin_ubimkvol_SOURCES	= $(top_srcdir)/src/ubimkvol/ubimkvol.c 
bin_ubimkvol_LDADD   	= $(top_builddir)/lib/libubi.la 
bin_ubimkvol_LDFLAGS	= -static

bin_ubirmvol_SOURCES	= $(top_srcdir)/src/ubirmvol/ubirmvol.c 
bin_ubirmvol_LDADD   	= $(top_builddir)/lib/libubi.la
bin_ubirmvol_LDFLAGS	= -static

bin_ubiwritevol_SOURCES	= $(top_srcdir)/src/ubiwritevol/ubiwritevol.c 
bin_ubiwritevol_LDADD   = $(top_builddir)/lib/libubi.la 
bin_ubiwritevol_LDFLAGS	= -static

bin_ubiinfo_SOURCES	= $(top_srcdir)/src/ubiinfo/ubiinfo.c

bin_nand2bin_SOURCES	= $(top_srcdir)/src/nand2bin/nand2bin.c \
			  $(top_srcdir)/src/bin2nand/nandecc.c \
			  $(top_srcdir)/src/nand2bin/nandcorr.c

bin_unubi_SOURCES	= $(top_srcdir)/src/unubi/unubi.c
bin_unubi_LDADD   	= $(top_builddir)/lib/libcrc32.la 

clean-local:
	rm -rf bin/ 
