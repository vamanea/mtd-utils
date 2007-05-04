#!/bin/sh

ubidev="$1"
ubiloadcmd="$2"
tests="mkvol_basic mkvol_bad mkvol_paral rsvol io_basic io_read io_update io_paral"

if test -z "$ubidev" || test -z "$ubiloadcmd";
then
	echo "Usage:"
	echo "$0 <UBI device> <ubi module load command>"
	exit 1
fi

ubiname=`echo $ubidev | cut -d/ -f3`

major=`cat /sys/class/ubi/$ubiname/dev | cut -d: -f1`

for minor in `seq 0 4`; do
	if test ! -e ${ubidev}_${minor} ;
	then
		mknod ${ubidev}_${minor} c $major $(($minor + 1))
	fi
done

if ! test -c "$ubidev";
then
	echo "Error: $ubidev is not character device"
	exit 1
fi

for t in `echo $tests`;
do
	echo "Running $t $ubidev"
	"./$t" "$ubidev" || exit 1
done

./integ "$ubiloadcmd" || exit 1

echo SUCCESS

exit 0
