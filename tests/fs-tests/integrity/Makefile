
ifeq ($(origin CC),default)
CC = gcc
endif

COMMON_HEADERS_DIR := ../../../include

CFLAGS := $(CFLAGS) -Wall -g -O2 -I../lib -I$(COMMON_HEADERS_DIR)

LDFLAGS := $(LDFLAGS)

TARGETS = integck

all: $(TARGETS)

$(TARGETS): ../lib/tests.o

../lib/tests.o: ../lib/tests.h

clean:
	rm -f *.o $(TARGETS)

tests: all
	./integck
