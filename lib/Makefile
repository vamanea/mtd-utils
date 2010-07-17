#
# Makefile for libmtd
#

SUBDIRS =

# CFLAGS += -Werror
CPPFLAGS += -I../include
LIBS = libmtd
TARGETS = libmtd.a

include ../common.mk

$(BUILDDIR)/libmtd.a: $(addprefix $(BUILDDIR)/,\
       libmtd.o libmtd_legacy.o crc32.o fec.o)

clean::
	rm -f $(addsuffix .a, $(LIBS))

install::

uninstall:
