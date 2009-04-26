#
# Makefile for ubi-utils
#

KERNELHDR := ../include

SUBDIRS = old-utils

# CFLAGS += -Werror
CPPFLAGS += -Iinclude -Isrc -I$(KERNELHDR)

LIBS = libubi libmtd libubigen libiniparser libscan
TARGETS = ubiupdatevol ubimkvol ubirmvol ubicrc32 ubinfo ubiattach \
          ubidetach ubinize ubiformat ubirename mtdinfo

VPATH = src

include ../common.mk

# And the below is the rule to get final executable from its .o and common.o
$(TARGETS): $(addprefix $(BUILDDIR)/,\
	libubi.a common.o)
#	$(CC) $(CFLAGS) $(filter %.o, $^) -L. -lubi -o $@

$(BUILDDIR)/ubicrc32: $(addprefix $(BUILDDIR)/,\
	ubicrc32.o crc32.o)
#	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/ubinize: $(addprefix $(BUILDDIR)/,\
	ubinize.o common.o crc32.o libiniparser.a libubigen.a)
#	$(CC) $(CFLAGS) $(filter %.o, $^) -L. -liniparser -lubigen -o $@

$(BUILDDIR)/mtdinfo: $(addprefix $(BUILDDIR)/,\
	libmtd.a libubigen.a crc32.o common.o)
#	$(CC) $(CFLAGS) $(filter %.o, $^) -L. -lmtd -lubigen -o $@

$(BUILDDIR)/ubiformat: $(addprefix $(BUILDDIR)/,\
	ubiformat.o common.o crc32.o libmtd.a libscan.a libubi.a libubigen.a)
#	$(CC) $(CFLAGS) $(filter %.o, $^) -L. -lmtd -lscan -lubi -lubigen -o $@

$(BUILDDIR)/libubi.a: $(BUILDDIR)/libubi.o

$(BUILDDIR)/libmtd.a: $(BUILDDIR)/libmtd.o $(BUILDDIR)/libmtd_legacy.o

$(BUILDDIR)/libubigen.a: $(BUILDDIR)/libubigen.o

$(BUILDDIR)/libiniparser.a: $(addprefix $(BUILDDIR)/,\
	libiniparser.o dictionary.o)

$(BUILDDIR)/libscan.a: $(addprefix $(BUILDDIR)/,\
	libscan.o crc32.o)

clean::
	rm -f $(addsuffix .a, $(LIBS))

install::
	mkdir -p ${DESTDIR}/${SBINDIR}
	install -m 0755 ${TARGETS} ${DESTDIR}/${SBINDIR}/

uninstall:
	for file in ${TARGETS}; do \
		$(RM) ${DESTDIR}/${SBINDIR}/$$file; \
	done
