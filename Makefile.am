## Process this file with automake to produce Makefile.in

sbin_PROGRAMS = ftl_format erase eraseall nftldump nanddump doc_loadbios \
               nftl_format mkfs.jffs ftl_check nandtest nandwrite mkfs.jffs2 \
	       lock unlock einfo mtd_debug fcp

CFLAGS = -O2 -Wall
INCLUDES = -I@CONFIG_KERNELDIR@/include -I@CONFIG_KERNELDIR@/fs/jffs2
SYMLINKS = crc32.h crc32.c compr_rtime.c compr_rubin.c compr_zlib.c compr.c pushpull.c pushpull.h histo_mips.h compr_rubin.h
CLEANFILES = $(SYMLINKS)

mkfs_jffs2_SOURCES = mkfs.jffs2.c $(SYMLINKS)

$(SYMLINKS):
	ln -sf @CONFIG_KERNELDIR@/fs/jffs2/$@ $@

compr.o: compr.c
	$(COMPILE) $(CFLAGS) $(INCLUDES) -Dprintk=printf -DKERN_NOTICE= -DKERN_WARNING= -c -o $@ $<
