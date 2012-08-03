#!/bin/sh -euf

srcdir="$(readlink -ev -- ${0%/*})"
PATH="$srcdir:$srcdir/../..:$PATH"

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

# Just print parameters of the 'run_test' funcion in a user-friendly form.
print_params()
{
	local module="$1";    shift
	local size="$1";      shift
	local peb_size="$1";  shift
	local page_size="$1"; shift
	local fastmap="$1";   shift
	local fm_str

	if [ "$fastmap" = "0" ]; then
		fm_str="disabled"
	else
		fm_str="enabled"
	fi

	printf "%s"   "${size}MiB $module with ${peb_size}KiB PEB, "
	[ "$module" != "nandsim" ] || printf "%s" "${page_size}KiB NAND pages, "
	printf "%s\n" "fastmap $fm_str" 
}

print_separator()
{
	echo "======================================================================"
}

# Run a test on nandsim or mtdram with certain geometry.
# Usage: run_test <nandsim|mtdram> <flash size> <PEB size> <Page size> <FM>
#
# Flash size is specified in MiB
# PEB size is specified in KiB
# Page size is specified in bytes
# If fast-map should be enabled, pass 0 or 1
run_test()
{
	local module="$1";
	local size="$2";
	local peb_size="$3";
	local page_size="$4";
	local fastmap="$5";  
	local fm_supported fm_str fm_param mtdnum

	print_separator

	# Check if fastmap is supported
	if modinfo ubi | grep -q fm_auto; then
		fm_supported="yes"
	else
		fm_supported="no"
	fi

	if [ "$fastmap" = "0" ]; then
		fm_str="disabled"
		fm_param=
	elif [ "$fm_supported" = "yes" ]; then
		fm_str="enabled"
		fm_param="fm_auto"
	else
		echo "Fastmap is not supported, will test without fastmap"
		fm_str="disabled"
		fm_param=
	fi

	if [ "$module" = "nandsim" ]; then
		print_params "$@"

		load_nandsim.sh "$size" "$peb_size" "$page_size" ||
			echo "Cannot load nandsim, test skipped"

		mtdnum="$(find_mtd_device "$nandsim_patt")"
	else
		fatal "$module is not supported"
	fi

	modprobe ubi mtd="$mtdnum" $fm_param
	runtests.sh /dev/ubi0 ||:

	sudo rmmod ubi
	sudo rmmod "$module"
}

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

print_separator
print_separator

for fm in 1 2; do
	run_test "nandsim" "16"  "16" "512" "$fm"
	run_test "nandsim" "32"  "16" "512" "$fm"
	run_test "nandsim" "64"  "16" "512" "$fm"
	run_test "nandsim" "128" "16" "512" "$fm"
	run_test "nandsim" "256" "16" "512" "$fm"

	run_test "nandsim" "64"   "64" "2048" "$fm"
	run_test "nandsim" "128"  "64" "2048" "$fm"
	run_test "nandsim" "256"  "64" "2048" "$fm"
	run_test "nandsim" "512"  "64" "2048" "$fm"
	run_test "nandsim" "1024" "64" "2048" "$fm"

	run_test "nandsim" "64"   "128" "2048" "$fm"
	run_test "nandsim" "128"  "128" "2048" "$fm"
	run_test "nandsim" "256"  "128" "2048" "$fm"
	run_test "nandsim" "512"  "128" "2048" "$fm"
	run_test "nandsim" "1024" "128" "2048" "$fm"

	run_test "nandsim" "64"   "256" "2048" "$fm"
	run_test "nandsim" "128"  "256" "2048" "$fm"
	run_test "nandsim" "256"  "256" "2048" "$fm"
	run_test "nandsim" "512"  "256" "2048" "$fm"
	run_test "nandsim" "1024" "256" "2048" "$fm"

	run_test "nandsim" "64"   "512" "2048" "$fm"
	run_test "nandsim" "128"  "512" "2048" "$fm"
	run_test "nandsim" "256"  "512" "2048" "$fm"
	run_test "nandsim" "512"  "512" "2048" "$fm"
	run_test "nandsim" "1024" "512" "2048" "$fm"
done
