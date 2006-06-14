#
# This makefile simplifies the build process for a toolchain user.
# A toolchain developer should prefer a manual build process which
# fits to his original needs.
#

X86_PREFIX?=/usr/local
x86_path=./build_x86
x86_status=$(x86_path)/config.status

PPC_PREFIX?=/opt/ppcnf/crossroot
ppc_path=./build_ppc
ppc_status=$(ppc_path)/config.status

all: x86 ppc

install: install_x86 install_ppc
uninstall: uninstall_x86 uninstall_ppc

install_x86: x86
	make -C $(x86_path) install

install_ppc: ppc 
	make -C $(ppc_path) install


uninstall_x86: x86
	make -C $(x86_path) uninstall

uninstall_ppc: ppc 
	make -C $(ppc_path) uninstall



x86: $(x86_status)
	make -C $(x86_path)

ppc:  $(ppc_status)
	make -C $(ppc_path)

$(x86_status): $(x86_path) Makefile.in
	cd $(x86_path) && ./config.status || ../configure \
				--prefix=$(X86_PREFIX)

$(ppc_status): $(ppc_path) Makefile.in
	cd $(ppc_path) && ./config.status || ../configure \
				--build=i686-pc-linux-gnu \
				--host=ppc-linux \
				--prefix=$(PPC_PREFIX) \
				--exec-prefix=$(PPC_PREFIX)

Makefile.in: Makefile.am
	./bootstrap

$(x86_path):
	mkdir -p $(x86_path)

$(ppc_path):
	mkdir -p $(ppc_path)

clean:
	rm -rf depcomp install-sh missing .deps \
		config.log config.status \
		inc/Makefile.in lib/Makefile.in
	find . -type f -name "*~" -print | xargs $(RM)
	rm -f Makefile.in
	rm -f aclocal.m4
	rm -rf autom4te.cache
	rm -f config.guess
	rm -f config.sub
	rm -f configure
	rm -f depcomp
	rm -f install-sh
	rm -f ltmain.sh
	rm -f missing
	rm -f lib/Makefile.in
	rm -f inc/Makefile.in
	rm -rf $(x86_path)
	rm -rf $(ppc_path)
