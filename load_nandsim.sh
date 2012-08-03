#!/bin/bash

#
# This script inserts NAND simulator module to emulate NAND flash of specified
# size.
#
# Author: Artem Bityutskiy
#

fatal()
{
        echo "Error: $1" 1>&2
        exit 1
}

usage()
{
	cat 1>&2 <<EOF
Load NAND simulator to simulate flash of a specified size.

Usage: ${0##*/} <size in MiB> <eraseblock size in KiB> \\
       <page size (512 or 2048)>

Only the first parameter is mandatory. Default eraseblock size
is 16KiB, default NAND page size is 512 bytes.

Only the following combinations are supported:
--------------------------------------------------
| size (MiB) | EB size (KiB) | Page size (bytes) |
--------------------------------------------------
| 16         | 16            | 512               |
| 32         | 16            | 512               |
| 64         | 16            | 512               |
| 128        | 16            | 512               |
| 256        | 16            | 512               |
| 64         | 64            | 2048              |
| 64         | 128           | 2048              |
| 64         | 256           | 2048              |
| 64         | 512           | 2048              |
| 128        | 64            | 2048              |
| 128        | 128           | 2048              |
| 128        | 256           | 2048              |
| 128        | 512           | 2048              |
| 256        | 64            | 2048              |
| 256        | 128           | 2048              |
| 256        | 256           | 2048              |
| 256        | 512           | 2048              |
| 512        | 64            | 2048              |
| 512        | 128           | 2048              |
| 512        | 256           | 2048              |
| 512        | 512           | 2048              |
| 1024       | 64            | 2048              |
| 1024       | 128           | 2048              |
| 1024       | 256           | 2048              |
| 1024       | 512           | 2048              |
--------------------------------------------------
EOF
}

if grep -q "NAND simulator" /proc/mtd; then
	fatal "nandsim is already loaded"
fi

if [ "$#" -lt "1" ]; then
	usage
	exit 1
fi

SZ="$1"
EBSZ="$2"
PGSZ="$3"
if [ "$#" = "1" ]; then
	EBSZ="16"
	PGSZ="512"
elif [ "$#" = "2" ]; then
	PGSZ="512"
fi

if [ "$PGSZ" -eq 512 ] && [ "$EBSZ" -ne "16" ]; then
	fatal "only 16KiB eraseblocks are possible in case of 512 bytes page"
fi

if [ "$PGSZ" -eq "512" ]; then
	case "$SZ" in
	16)  modprobe nandsim first_id_byte=0x20 second_id_byte=0x33 ;;
	32)  modprobe nandsim first_id_byte=0x20 second_id_byte=0x35 ;;
	64)  modprobe nandsim first_id_byte=0x20 second_id_byte=0x36 ;;
	128) modprobe nandsim first_id_byte=0x20 second_id_byte=0x78 ;;
	256) modprobe nandsim first_id_byte=0x20 second_id_byte=0x71 ;;
	*) fatal "flash size ${SZ}MiB is not supported, try 16, 32, 64 or 256"
	esac
elif [ "$PGSZ" -eq "2048" ]; then
	case "$EBSZ" in
	64)  FOURTH="0x05" ;;
	128) FOURTH="0x15" ;;
	256) FOURTH="0x25" ;;
	512) FOURTH="0x35" ;;
	*)   fatal "eraseblock ${EBSZ}KiB is not supported"
	esac

	case "$SZ" in
	64)  modprobe nandsim first_id_byte=0x20 second_id_byte=0xa2 third_id_byte=0x00 fourth_id_byte="$FOURTH" ;;
	128) modprobe nandsim first_id_byte=0xec second_id_byte=0xa1 third_id_byte=0x00 fourth_id_byte="$FOURTH" ;;
	256) modprobe nandsim first_id_byte=0x20 second_id_byte=0xaa third_id_byte=0x00 fourth_id_byte="$FOURTH" ;;
	512) modprobe nandsim first_id_byte=0x20 second_id_byte=0xac third_id_byte=0x00 fourth_id_byte="$FOURTH" ;;
	1024) modprobe nandsim first_id_byte=0xec second_id_byte=0xd3 third_id_byte=0x51 fourth_id_byte="$FOURTH" ;;
	*) fatal "unable to emulate ${SZ}MiB flash with ${EBSZ}KiB eraseblock"
	esac
else
	fatal "bad NAND page size ${PGSZ}KiB, it has to be either 512 or 2048"
fi

if [ "$?" != "0" ]; then
	fatal "Error: cannot load nandsim"
fi

echo "Loaded NAND simulator (${SZ}MiB, ${EBSZ}KiB eraseblock, $PGSZ bytes NAND page)"
exit 0
