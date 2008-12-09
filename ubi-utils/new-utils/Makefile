#
# Makefile for ubi-utils
#

KERNELHDR := ../../include
DESTDIR := /usr/local
SBINDIR=/usr/sbin
MANDIR=/usr/man
INCLUDEDIR=/usr/include

CC := $(CROSS)gcc
AR := $(CROSS)ar
RANLIB := $(CROSS)ranlib
CFLAGS ?= -O2
CFLAGS += -Werror -Wall
CPPFLAGS += -Iinclude -Isrc -I$(KERNELHDR)
LDFLAGS += -L.

LIBS = libubi libmtd libubigen libiniparser libscan
UTILS = ubiupdatevol ubimkvol ubirmvol ubicrc32 ubinfo ubiattach \
        ubidetach ubinize ubiformat

vpath %.c src

all: $(UTILS)

# The below cancels existing implicite rule to make programs from .c files,
# in order to force make using our rule defined below
%: %.c

# The below is the rule to get an .o file from a .c file
%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -c -o $@

# And the below is the rule to get final executable from its .o and common.o
%: libubi.a %.o common.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(filter %.o, $^) -lubi -o $@

ubicrc32: ubicrc32.o crc32.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

ubinize: ubinize.o common.o crc32.o libiniparser.a libubigen.a
	$(CC) $(CFLAGS) $(LDFLAGS) $(filter %.o, $^) -liniparser -lubigen -o $@

ubiformat: ubiformat.o common.o crc32.o libmtd.a libscan.a libubi.a libubigen.a
	$(CC) $(CFLAGS) $(LDFLAGS) $(filter %.o, $^) -lmtd -lscan -lubi -lubigen -o $@

libubi.a: libubi.o
	$(AR) crv $@ $^
	$(RANLIB) $@

libmtd.a: libmtd.o
	$(AR) crv $@ $^
	$(RANLIB) $@

libubigen.a: libubigen.o
	$(AR) crv $@ $^
	$(RANLIB) $@

libiniparser.a: libiniparser.o dictionary.o
	$(AR) crv $@ $^
	$(RANLIB) $@

libscan.a: libscan.o crc32.o
	$(AR) crv $@ $^
	$(RANLIB) $@

clean:
	rm -rf *.o $(addsuffix .a, $(LIBS)) $(UTILS) .*.c.dep

install: ${UTILS}
	mkdir -p ${DESTDIR}/${SBINDIR}
	install -m 0755 ${UTILS} ${DESTDIR}/${SBINDIR}/

uninstall:
	for file in ${UTILS}; do \
		$(RM) ${DESTDIR}/${SBINDIR}/$$file; \
	done
