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
CFLAGS := -I./inc -I./src -I$(KERNELHDR) $(OPTFLAGS) -Werror \
	-Wwrite-strings -W -std=gnu99 -DPACKAGE_VERSION=\"1.0\"

PERLPROGS = mkpfi
TARGETS = ubiupdate ubimkvol ubirmvol ubicrc32 ubinfo ubiattach ubidetach \
	  unubi pfi2bin

vpath   %.c ./src

%: %.o
	$(CC) $(LDFLAGS) -g -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -g -c -o $@ $< -g -Wp,-MD,.$(shell basename $<).dep

all: $(TARGETS) libubi.a

IGNORE=${wildcard .*.c.dep}
-include ${IGNORE}

clean:
	rm -rf *.o $(TARGETS) .*.c.dep libubi.a

libubi.a: libubi.o
	ar cr $@ $^

ubidetach: ubidetach.o common.o libubi.o
	$(CC) $(LDFLAGS) -o $@ $^

ubiattach: ubiattach.o common.o libubi.o
	$(CC) $(LDFLAGS) -o $@ $^

ubinfo: ubinfo.o common.o libubi.o
	$(CC) $(LDFLAGS) -o $@ $^

ubiupdate: ubiupdate.o common.o libubi.o
	$(CC) $(LDFLAGS) -o $@ $^

ubimkvol: ubimkvol.o common.o libubi.o
	$(CC) $(LDFLAGS) -o $@ $^

ubirmvol: ubirmvol.o common.o libubi.o
	$(CC) $(LDFLAGS) -o $@ $^

ubicrc32: ubicrc32.o crc32.o
	$(CC) $(LDFLAGS) -o $@ $^

unubi: unubi.o crc32.o unubi_analyze.o eb_chain.o
	$(CC) $(LDFLAGS) -o $@ $^

pfi2bin: pfi2bin.o common.o peb.o list.o crc32.o libubigen.o bootenv.o \
		hashmap.o reader.o pfi.o
	$(CC) $(LDFLAGS) -o $@ $^

install: ${TARGETS}
	mkdir -p ${DESTDIR}/${SBINDIR}
	install -m0755 ${TARGETS} ${DESTDIR}/${SBINDIR}/
	(cd perl && install ${PERLPROGS} ${DESTDIR}/${SBINDIR}/)

uninstall:
	for file in ${TARGETS} ${PERLPROGS}; do \
		$(RM) ${DESTDIR}/${SBINDIR}/$$file; \
	done
