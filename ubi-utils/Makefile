#
# Makefile for ubi-utils
#

OPTFLAGS := -O2 -Wall
KERNELHDR := ../include
DESTDIR := /usr/local
SBINDIR=/usr/sbin
MANDIR=/usr/man
INCLUDEDIR=/usr/include

CC := $(CROSS)gcc
CFLAGS := -Iinclude -Isrc -I$(KERNELHDR) $(OPTFLAGS) -Werror -Wall

LIBS = libubi libubigen libiniparser
UTILS = ubiupdate ubimkvol ubirmvol ubicrc32 ubinfo ubiattach ubidetach ubinize

vpath %.c src

all: $(UTILS)

# The below cancels existing implicite rule to make programs from .c files,
# in order to force make using our rule defined below
%: %.c

# The below is the rule to get an .o file from a .c file
%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@

# And the below is the rule to get final executable from its .o and common.o
%: libubi %.o common.o
	$(CC) $(CFLAGS) $(filter %.o, $^) -L. -lubi -o $@

ubicrc32: ubicrc32.o crc32.o
	$(CC) $(CFLAGS) -o $@ $^

ubinize: ubinize.o common.o crc32.o libiniparser libubigen
	$(CC) $(CFLAGS) $(filter %.o, $^) -L. -liniparser -lubigen -o $@

libubi: libubi.o
	$(AR) crv $@.a $^
	ranlib $@.a

libubigen: libubigen.o
	$(AR) crv $@.a $^
	ranlib $@.a

libiniparser: libiniparser.o dictionary.o
	$(AR) crv $@.a $^
	ranlib $@.a

clean:
	rm -rf *.o $(addsuffix .a, $(LIBS)) $(UTILS) .*.c.dep

install: ${UTILS}
	mkdir -p ${DESTDIR}/${SBINDIR}
	install -m0755 ${UTILS} ${DESTDIR}/${SBINDIR}/

uninstall:
	for file in ${UTILS}; do \
		$(RM) ${DESTDIR}/${SBINDIR}/$$file; \
	done
