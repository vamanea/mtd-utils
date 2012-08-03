#!/bin/sh -euf

srcdir="$(readlink -ev -- ${0%/*})"
PATH="$srcdir:$PATH"

fatal()
{
	echo "Error: $1" 1>&2
	exit 1
}

usage()
{
	cat 1>&2 <<EOF
Stress-test an UBI device. This test is basically built on top of
'runtests.sh' and runs it several times for different configurations.

The nandsim and mtdram drivers have to be compiled as kernel modules.

Usage:
  ${0##*/} run
EOF
}

cleanup_handler()
{
	local ret="$1"
	rmmod ubi >/dev/null 2>&1 ||:
	rmmod nandsim >/dev/null 2>&1 ||:
	rmmod mtdram >/dev/null 2>&1  ||:

	# Below is magic to exit with correct exit code
	if [ "$ret" != "0" ]; then
		trap false EXIT
	else
		trap true EXIT
	fi
}
trap 'cleanup_handler $?' EXIT
trap 'cleanup_handler 1' HUP PIPE INT QUIT TERM

# Find MTD device number by pattern in /proc/mtd
# Usage: find_mtd_device <pattern>
find_mtd_device()
{
	printf "%s" "$(grep "$1" /proc/mtd | sed -e "s/^mtd\([0-9]\+\):.*$/\1/")"
}

exit_handler()
{
	echo exit
	cleanup
}

runtests="runtests.sh"

if [ "$#" -lt 1 ] || [ "$1" != "run" ]; then
	usage
	exit 1
fi

# Make sure neither mtdram nor nandsim are used
nandsim_patt="NAND simulator"
mtdram_patt="mtdram test device"
! grep -q "$nandsim_patt" /proc/mtd ||
	fatal "the nandsim driver is already used"
! grep -q "$mtdram_patt" /proc/mtd ||
	fatal "the mtdram driver is already used"

rmmod ubi >/dev/null 2>&1 ||:

if modinfo ubi | grep -q fm_auto; then
	fastmap_supported="yes"
else
	fastmap_supported="no"
fi

echo "=================================================="
echo "512MiB nandsim, 2KiB NAND pages, no fastmap" 
echo "=================================================="

modprobe nandsim first_id_byte=0x20 second_id_byte=0xac \
		 third_id_byte=0x00 fourth_id_byte=0x15
mtdnum="$(find_mtd_device "$nandsim_patt")"
modprobe ubi mtd="$mtdnum"
$runtests /dev/ubi0 && echo "SUCCESS" || echo "FAILURE"

echo "=================================================="
echo "256MiB nandsim, 512-byte NAND pages, no fastmap" 
echo "=================================================="

modprobe nandsim first_id_byte=0x20 second_id_byte=0x71
mtdnum="$(find_mtd_device "$nandsim_patt")"
modprobe ubi mtd="$mtdnum"
$runtests /dev/ubi0 && echo "SUCCESS" || echo "FAILURE"
