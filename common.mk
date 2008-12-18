CC := $(CROSS)gcc
AR := $(CROSS)ar
RANLIB := $(CROSS)ranlib
CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wwrite-strings -W

DESTDIR ?= /usr/local
PREFIX=/usr
EXEC_PREFIX=$(PREFIX)
SBINDIR=$(EXEC_PREFIX)/sbin
MANDIR=$(PREFIX)/share/man
INCLUDEDIR=$(PREFIX)/include

ifndef BUILDDIR
ifeq ($(origin CROSS),undefined)
  BUILDDIR := $(PWD)
else
# Remove the trailing slash to make the directory name
  BUILDDIR := $(PWD)/$(CROSS:-=)
endif
endif

override TARGETS := $(addprefix $(BUILDDIR)/,$(TARGETS))

SUBDIRS_ALL = $(patsubst %,subdirs_%_all,$(SUBDIRS))
SUBDIRS_CLEAN = $(patsubst %,subdirs_%_clean,$(SUBDIRS))
SUBDIRS_INSTALL = $(patsubst %,subdirs_%_install,$(SUBDIRS))

all:: $(TARGETS) $(SUBDIRS_ALL)

clean:: $(SUBDIRS_CLEAN)
	rm -f $(BUILDDIR)/*.o $(TARGETS) $(BUILDDIR)/.*.c.dep

install:: $(TARGETS) $(SUBDIRS_INSTALL)

$(BUILDDIR)/%: $(BUILDDIR)/%.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDFLAGS_$(notdir $@)) -g -o $@ $^ $(LDLIBS) $(LDLIBS_$(notdir $@))

$(BUILDDIR)/%.a:
	$(AR) crv $@ $^
	$(RANLIB) $@

$(BUILDDIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $< -g -Wp,-MD,$(BUILDDIR)/.$(<F).dep

subdirs_%:
	d=$(patsubst subdirs_%,%,$@); \
	t=`echo $$d | sed s:.*_::` d=`echo $$d | sed s:_.*::`; \
	$(MAKE) BUILDDIR=$(BUILDDIR)/$$d -C $$d $$t

.SUFFIXES:

IGNORE=${wildcard $(BUILDDIR)/.*.c.dep}
-include ${IGNORE}

PHONY += all clean install
.PHONY: $(PHONY)
