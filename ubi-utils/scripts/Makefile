#
# Makefile
#
# Testcase for UBI pfi update.
#
# Author:	Frank Haverkamp <haverkam@de.ibm.com>
#

card		= test
mkpfi_cfg	= test.cfg

#
# Some default values you might want to overwrite. Try it if you need
# it and add more if needed. Note that no real sanity checking is done
# on those values. If you do it wrong your card has no valid PDD data.
#

PATH := $(PATH):/opt/ppc/usr/bin

dd		= dd
sed		= sed
bin2nand	= bin2nand
ubigen		= ubigen
mkpfi		= mkpfi
pfi2bin		= pfi2bin

vmlinux_bin	?= test_vmlinux.bin
rootfs_bin	?= test_rootfs.bin
spl_bin		?= test_u-boot.bin

compl		?= $(card)_complete
compl_pfi	?= $(compl).pfi

all: $(compl_pfi)

$(compl_pfi): $(vmlinux_bin) $(rootfs_bin) $(spl_bin)
	$(mkpfi) -c $(mkpfi_cfg)

#
# Default data
#
# If the binary data is not available in the current working directory
# we try to create symlinks to our test data.
#
$(vmlinux_bin) $(rootfs_bin) $(spl_bin):
	@echo
	@echo "No $@ found, will use defaults !"
	@echo
	@echo "OR press CTRL-C to provide your own $@" && 	\
	sleep 1 &&						\
	$(dd) if=/dev/urandom of=$@ bs=1M count=1

clean:
	$(RM) *.pfi *~

distclean: clean
	$(RM) *.bin
