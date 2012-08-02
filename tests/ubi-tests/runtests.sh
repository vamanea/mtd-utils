#!/bin/sh -euf

ubidev="$1"
tests="mkvol_basic mkvol_bad mkvol_paral rsvol io_basic io_read io_update io_paral volrefcnt"

fatal()
{
	echo "Error: $1" 2>&1
	exit 1
}

if [ -z "$ubidev" ]; then
	echo "Usage:" 2>&1
	echo "$0 <UBI device>"
	exit 1
fi

[ -c "$ubidev" ] || fatal "$ubidev is not character device"

for t in $tests; do
	echo "Running $t $ubidev"
	"./$t" "$ubidev" || fatal "$t failed"
done

echo "SUCCESS"
