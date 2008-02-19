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

PATH := $(PATH):/opt/ppc/usr/bin:../perl:..

dd		= dd
sed		= sed
bin2nand	= bin2nand
ubigen		= ubigen
mkpfi		= mkpfi -v
pfi2bin		= pfi2bin -v

vmlinux_bin	?= test_vmlinux.bin
rootfs_bin	?= test_rootfs.bin
spl_bin		?= test_u-boot.bin
pdd_txt		?= pdd.txt

flashtype	?= nand
pagesize	?= 2048

compl		?= $(card)_complete
compl_pfi	?= $(compl).pfi
compl_img	?= $(compl).img

compl_nand2048_mif=$(compl).$(flashtype)$(pagesize).mif
compl_nand2048_img=$(compl).$(flashtype)$(pagesize).img

all: $(compl_pfi) $(compl_nand2048_mif)

$(compl_pfi): $(vmlinux_bin) $(rootfs_bin) $(spl_bin)
	$(mkpfi) -c $(mkpfi_cfg)

# Binary data and out of band data (OOB)
#
$(compl_nand2048_mif): $(compl_img)
	$(bin2nand) -p $(pagesize) -o $(compl_nand2048_mif) $<

# Binary data only
#
$(compl_img): $(compl_pfi)
	$(pfi2bin) -j $(pdd_txt) -o $@ $<

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
	$(RM) *.bin *.mif *.oob *.img
