ALL_SOURCES=*.[ch] hashtable/*.[ch]

TARGETS = mkfs.ubifs

LDLIBS_mkfs.ubifs = -lz -llzo2 -lm -luuid

include ../common.mk

$(BUILDDIR)/mkfs.ubifs: $(addprefix $(BUILDDIR)/,\
	crc16.o crc32.o lpt.o compr.o devtable.o \
	hashtable/hashtable.o hashtable/hashtable_itr.o)

clean::
	rm -f $(BUILDDIR)/hashtable/*.o cscope.*

cscope:
	@echo $(ALL_SOURCES) > cscope.files
	@cscope -bR
	@rm cscope.files

install:: ${TARGETS}
	mkdir -p ${DESTDIR}/${SBINDIR}
	install -m 0755 ${TARGETS} ${DESTDIR}/${SBINDIR}/
