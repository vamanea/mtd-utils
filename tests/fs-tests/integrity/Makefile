
ifeq ($(origin CC),default)
CC = gcc
endif

COMMON_HEADERS_DIR := ../../../include

CFLAGS := $(CFLAGS) -Wall -g -O2 -I$(COMMON_HEADERS_DIR)

LDFLAGS := $(LDFLAGS)

TARGETS = integck

all: $(TARGETS)

# Disable optimizations to make it possible to use gdb comfortably
# Use -rdynamic to have stack backtraces
debug:
	gcc $(CFLAGS) -O0 -D INTEGCK_DEBUG -rdynamic integck.c -o integck

clean:
	rm -f *.o $(TARGETS)
