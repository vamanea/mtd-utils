UBIUTILS=../../ubi-utils
INCLUDE1=$(UBIUTILS)/inc
INCLUDE2=../../include
LIB=.

CC := $(CROSS)gcc

ALL_FILES=libubi io_update rmvol integ
ALL_FILES+=io_paral io_read io_basic mkvol_basic mkvol_bad mkvol_paral rsvol

CFLAGS += -Wall -I$(INCLUDE1) -I$(INCLUDE2) -L$(LIB) -ggdb

all: $(ALL_FILES)

libubi: $(UBIUTILS)/src/libubi.c  $(UBIUTILS)/inc/libubi.h  $(UBIUTILS)/src/libubi_int.h
	$(CC) $(CFLAGS) -DUDEV_SETTLE_HACK -c $(UBIUTILS)/src/libubi.c -o libubi.o
	ar cr libubi.a libubi.o

io_paral: io_paral.c common.c
	$(CC) $(CFLAGS) $^ -lubi -lpthread -o $@
io_update: io_update.c common.c
	$(CC) $(CFLAGS) $^ -lubi -o $@
io_read: io_read.c common.c
	$(CC) $(CFLAGS) $^ -lubi -o $@
io_basic: io_basic.c common.c
	$(CC) $(CFLAGS) $^ -lubi -o $@
mkvol_basic: mkvol_basic.c common.c
	$(CC) $(CFLAGS) $^ -lubi -o $@
mkvol_bad: mkvol_bad.c common.c
	$(CC) $(CFLAGS) $^ -lubi -o $@
mkvol_paral: mkvol_paral.c common.c
	$(CC) $(CFLAGS) $^ -lubi -lpthread -o $@
rsvol: rsvol.c common.c
	$(CC) $(CFLAGS) $^ -lubi -o $@
rmvol: rmvol.c common.c
	$(CC) $(CFLAGS) $^ -lubi -o $@
integ: integ.c
	$(CC) $(CFLAGS) $^ -lubi -o $@

clean:
	rm -rf $(ALL_FILES) $(addsuffix .o, $(ALL_FILES))
