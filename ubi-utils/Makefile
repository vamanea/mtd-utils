#
# Makefile for ubi-utils
#

HOST_OS_NAME := $(shell uname -s)
HOST_VERSION_NAME := $(shell uname -r)
BUILD_CPU := $(shell uname -m)
BUILD_OS := $(shell uname -o)

OPTFLAGS := -O2 -g -Wall
KERNELHDR := ../include
DESTDIR := /usr/local
SBINDIR := bin

CC := $(CROSS)gcc
CFLAGS := -I./inc -I./src -I$(KERNELHDR) $(OPTFLAGS) -Werror \
	-Wwrite-strings -W -std=gnu99 \
        -DHOST_OS_NAME=\"$(HOST_OS_NAME)\" \
        -DHOST_VERSION_NAME=\"$(HOST_VERSION_NAME)\" \
	-DBUILD_CPU=\"$(BUILD_CPU)\" -DBUILD_OS=\"$(BUILD_OS)\" \
	-DPACKAGE_VERSION=\"1.0\"

PERLPROGS = mkpfi ubicrc32.pl
TARGETS = ubiupdatevol ubimkvol ubirmvol pfiflash pddcustomize ubimirror \
	bin2nand nand2bin ubigen mkbootenv unubi pfi2bin ubicrc32

vpath   %.c ./src

%: %.o
	$(CC) $(LDFLAGS) -g -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -g -c -o $@ $< -g -Wp,-MD,.$(shell basename $<).dep

all: $(TARGETS)

IGNORE=${wildcard .*.c.dep}
-include ${IGNORE}

clean:
	rm -rf *.o $(TARGETS) .*.c.dep

ubiupdatevol: ubiupdatevol.o error.o libubi.o
	$(CC) $(LDFLAGS) -o $@ $^

ubimkvol: ubimkvol.o error.o libubi.o
	$(CC) $(LDFLAGS) -o $@ $^

ubirmvol: ubirmvol.o error.o libubi.o
	$(CC) $(LDFLAGS) -o $@ $^

pddcustomize: pddcustomize.o error.o libubimirror.o bootenv.o hashmap.o \
		libubiold.o libubiold_sysfs.o crc32.o
	$(CC) $(LDFLAGS) -o $@ $^

pfiflash: pfiflash.o libpfiflash.o list.o reader.o error.o libubimirror.o \
		bootenv.o hashmap.o pfi.o libubiold.o libubiold_sysfs.o crc32.o
	$(CC) $(LDFLAGS) -o $@ $^

ubimirror: ubimirror.o error.o libubimirror.o bootenv.o hashmap.o \
		libubiold.o libubiold_sysfs.o crc32.o
	$(CC) $(LDFLAGS) -o $@ $^

nand2bin: nand2bin.o nandecc.o nandcorr.o
	$(CC) $(LDFLAGS) -o $@ $^

bin2nand: bin2nand.o error.o nandecc.o
	$(CC) $(LDFLAGS) -o $@ $^

ubigen: ubigen.o libubigen.o crc32.o
	$(CC) $(LDFLAGS) -o $@ $^

mkbootenv: mkbootenv.o bootenv.o hashmap.o error.o crc32.o
	$(CC) $(LDFLAGS) -o $@ $^

unubi: unubi.o crc32.o unubi_analyze.o eb_chain.o
	$(CC) $(LDFLAGS) -o $@ $^

pfi2bin: pfi2bin.o peb.o error.o list.o crc32.o libubigen.o bootenv.o \
		hashmap.o reader.o pfi.o
	$(CC) $(LDFLAGS) -o $@ $^

ubicrc32: ubicrc32.o crc32.o
	$(CC) $(LDFLAGS) -o $@ $^

install: ${TARGETS}
	mkdir -p ${DESTDIR}/${SBINDIR}
	install -m0755 ${TARGETS} ${DESTDIR}/${SBINDIR}/
	(cd perl && install ${PERLPROGS} ${DESTDIR}/${SBINDIR}/)

uninstall:
	for file in ${TARGETS} ${PERLPROGS}; do \
		$(RM) ${DESTDIR}/${SBINDIR}/$$file; \
	done
