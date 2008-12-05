DESTDIR := /usr/local
SBINDIR=/usr/sbin
ALL_SOURCES=*.[ch] hashtable/*.[ch]
CFLAGS += -Wall
LDLIBS += -lz -llzo2 -lm -luuid
TARGETS = mkfs.ubifs

all: $(TARGETS)

mkfs.ubifs: crc16.o crc32.o lpt.o compr.o hashtable/hashtable.o \
            hashtable/hashtable_itr.o devtable.o

clean:
	rm -f *.o hashtable/*.o $(TARGETS) cscope.*

cscope:
	@echo $(ALL_SOURCES) > cscope.files
	@cscope -bR
	@rm cscope.files

install: ${TARGETS}
	mkdir -p ${DESTDIR}/${SBINDIR}
	install -m 0755 ${TARGETS} ${DESTDIR}/${SBINDIR}/
