#
# Makefile for ubi-utils
#

KERNELHDR := ../../include

CFLAGS ?= -O2 -g -Werror
CPPFLAGS += -I./inc -I./src -I$(KERNELHDR) \
	-std=gnu99 -DPACKAGE_VERSION=\"1.0\"

PERLPROGS = mkpfi ubicrc32.pl

TARGETS = pfiflash pddcustomize ubimirror bin2nand nand2bin ubigen \
	mkbootenv unubi pfi2bin

vpath   %.c ./src

include ../../common.mk

$(BUILDDIR)/pddcustomize: $(addprefix $(BUILDDIR)/,\
	pddcustomize.o error.o libubimirror.o bootenv.o hashmap.o \
	libubi.o crc32.o)

$(BUILDDIR)/pfiflash: $(addprefix $(BUILDDIR)/,\
	pfiflash.o libpfiflash.o list.o reader.o error.o libubimirror.o \
	bootenv.o hashmap.o pfi.o libubi.o crc32.o)

$(BUILDDIR)/ubimirror: $(addprefix $(BUILDDIR)/,\
	ubimirror.o error.o libubimirror.o bootenv.o hashmap.o \
	libubi.o crc32.o)

$(BUILDDIR)/nand2bin: $(addprefix $(BUILDDIR)/,\
	nand2bin.o nandecc.o nandcorr.o)

$(BUILDDIR)/bin2nand: $(addprefix $(BUILDDIR)/,\
	bin2nand.o error.o nandecc.o)

$(BUILDDIR)/ubigen: $(addprefix $(BUILDDIR)/,\
	ubigen.o libubigen.o crc32.o)

$(BUILDDIR)/mkbootenv: $(addprefix $(BUILDDIR)/,\
	mkbootenv.o bootenv.o hashmap.o error.o crc32.o)

$(BUILDDIR)/unubi: $(addprefix $(BUILDDIR)/,\
	unubi.o crc32.o unubi_analyze.o eb_chain.o)

$(BUILDDIR)/pfi2bin: $(addprefix $(BUILDDIR)/,\
	pfi2bin.o peb.o error.o list.o crc32.o libubigen.o bootenv.o \
	hashmap.o reader.o pfi.o)

install::
	mkdir -p ${DESTDIR}/${SBINDIR}
	install -m 0755 ${TARGETS} ${DESTDIR}/${SBINDIR}/
	(cd perl && install ${PERLPROGS} ${DESTDIR}/${SBINDIR}/)

uninstall:
	for file in ${TARGETS} ${PERLPROGS}; do \
		$(RM) ${DESTDIR}/${SBINDIR}/$$file; \
	done
