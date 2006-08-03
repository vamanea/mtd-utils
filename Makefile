
# -*- sh -*-

# $Id: Makefile,v 1.60 2005/11/07 11:15:09 gleixner Exp $

SBINDIR=/usr/sbin
MANDIR=/usr/man
INCLUDEDIR=/usr/include
#CROSS=arm-linux-
CC := $(CROSS)gcc
CFLAGS := -I./include -O2 -Wall

ifeq ($(origin CROSS),undefined)
  BUILDDIR := .
else
# Remove the trailing slash to make the directory name
  BUILDDIR := $(CROSS:-=)
endif

RAWTARGETS = ftl_format flash_erase flash_eraseall nanddump doc_loadbios \
	mkfs.jffs ftl_check mkfs.jffs2 flash_lock flash_unlock flash_info \
	flash_otp_info flash_otp_dump mtd_debug flashcp nandwrite \
	jffs2dump \
	nftldump nftl_format docfdisk \
	rfddump rfdformat \
	sumtool #jffs2reader

TARGETS = $(foreach target,$(RAWTARGETS),$(BUILDDIR)/$(target))

SYMLINKS =

%: %.o
	$(CC) $(CFLAGS) $(LDFLAGS) -g -o $@ $^

$(BUILDDIR)/%.o: %.c
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -g -c -o $@ $< -g -Wp,-MD,$(BUILDDIR)/.$(<F).dep

.SUFFIXES:

all: $(TARGETS)

IGNORE=${wildcard $(BUILDDIR)/.*.c.dep}
-include ${IGNORE}

clean:
	rm -f $(BUILDDIR)/*.o $(TARGETS) $(BUILDDIR)/.*.c.dep $(SYMLINKS)
	if [ "$(BUILDDIR)x" != ".x" ]; then rm -rf $(BUILDDIR); fi

$(SYMLINKS):
	ln -sf ../fs/jffs2/$@ $@

$(BUILDDIR)/mkfs.jffs2: $(BUILDDIR)/crc32.o \
			$(BUILDDIR)/compr_rtime.o \
			$(BUILDDIR)/mkfs.jffs2.o \
			$(BUILDDIR)/compr_zlib.o \
			$(BUILDDIR)/compr.o
	$(CC) $(LDFLAGS) -o $@ $^ -lz

$(BUILDDIR)/flash_eraseall: $(BUILDDIR)/crc32.o $(BUILDDIR)/flash_eraseall.o
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILDDIR)/jffs2reader: $(BUILDDIR)/jffs2reader.o
	$(CC) $(LDFLAGS) -o $@ $^ -lz

$(BUILDDIR)/jffs2dump: $(BUILDDIR)/jffs2dump.o $(BUILDDIR)/crc32.o
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILDDIR)/sumtool: $(BUILDDIR)/sumtool.o $(BUILDDIR)/crc32.o
	$(CC) $(LDFLAGS) -o $@ $^

install: ${TARGETS}
	mkdir -p ${DESTDIR}/${SBINDIR}
	install -m0755 ${TARGETS} ${DESTDIR}/${SBINDIR}/
	mkdir -p ${DESTDIR}/${MANDIR}/man1
	gzip -c mkfs.jffs2.1 > ${DESTDIR}/${MANDIR}/man1/mkfs.jffs2.1.gz
