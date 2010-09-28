
# -*- sh -*-

CPPFLAGS += -I./include $(ZLIBCPPFLAGS) $(LZOCPPFLAGS)

ifeq ($(WITHOUT_XATTR), 1)
  CPPFLAGS += -DWITHOUT_XATTR
endif

SUBDIRS = lib ubi-utils mkfs.ubifs

TARGETS = ftl_format flash_erase nanddump doc_loadbios \
	ftl_check mkfs.jffs2 flash_lock flash_unlock flash_info \
	flash_otp_info flash_otp_dump mtd_debug flashcp nandwrite nandtest \
	jffs2dump \
	nftldump nftl_format docfdisk \
	rfddump rfdformat \
	serve_image recv_image \
	sumtool #jffs2reader
SCRIPTS = flash_eraseall

SYMLINKS =

LDLIBS = -L$(BUILDDIR)/lib -lmtd
LDDEPS = $(BUILDDIR)/lib/libmtd.a

include common.mk

# mkfs.ubifs needs -lubi which is in ubi-utils/
subdirs_mkfs.ubifs_all: subdirs_ubi-utils_all

clean::
	-rm -f $(SYMLINKS)
ifneq ($(BUILDDIR)/.git,)
ifneq ($(BUILDDIR),.)
ifneq ($(BUILDDIR),$(CURDIR))
	rm -rf $(BUILDDIR)
endif
endif
endif

$(SYMLINKS):
	ln -sf ../fs/jffs2/$@ $@

$(BUILDDIR)/mkfs.jffs2: $(addprefix $(BUILDDIR)/,\
	compr_rtime.o mkfs.jffs2.o compr_zlib.o compr_lzo.o \
	compr.o rbtree.o)
LDFLAGS_mkfs.jffs2 = $(ZLIBLDFLAGS) $(LZOLDFLAGS)
LDLIBS_mkfs.jffs2  = -lz -llzo2

$(BUILDDIR)/jffs2reader: $(BUILDDIR)/jffs2reader.o
LDFLAGS_jffs2reader = $(ZLIBLDFLAGS) $(LZOLDFLAGS)
LDLIBS_jffs2reader  = -lz -llzo2

$(BUILDDIR)/lib/libmtd.a: subdirs_lib_all ;

install:: ${TARGETS} ${SCRIPTS}
	mkdir -p ${DESTDIR}/${SBINDIR}
	install -m 0755 ${TARGETS} ${SCRIPTS} ${DESTDIR}/${SBINDIR}/
	mkdir -p ${DESTDIR}/${MANDIR}/man1
	gzip -9c mkfs.jffs2.1 > ${DESTDIR}/${MANDIR}/man1/mkfs.jffs2.1.gz
