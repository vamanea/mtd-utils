#!/bin/sh

exit_success ()
{
	echo "UBI Utils Test Scripts - SUCCESS!"
	exit 0
}

exit_failure ()
{
	echo $1
	echo "UBI Utils Test Scripts - FAILED!"
	exit 1
}

echo UBI Utils Test Scripts

devno=$1
logfile=temp-test-log.txt

if test -z "$devno";
then
	echo "Usage is $0 <mtd device number>"
	exit 1
fi

cwd=`pwd` || exit_failure "pwd failed"

log="${cwd}/${logfile}"

PATH=$PATH:$cwd:..

cat /dev/null > $log || exit_failure "Failed to create $log"

echo "Setting up for jffs2_test.sh" | tee -a $log

avail=`cat /sys/class/ubi/ubi${devno}/avail_eraseblocks`
size=`cat /sys/class/ubi/ubi${devno}/eraseblock_size`

bytes=`expr $avail \* $size`

ubimkvol -d$devno -s$bytes -n0 -Njtstvol || exit_failure "ubimkvol failed"

mkdir -p /mnt/test_file_system || exit_failure "mkdir failed"

mtd=`cat /proc/mtd | grep jtstvol | cut -d: -f1`

if test -z "$mtd";
then
	exit_failure "mtd device not found"
fi

mount -t jffs2 $mtd /mnt/test_file_system || exit_failure "mount failed"

cd /mnt/test_file_system || exit_failure "cd failed"

echo Running jffs2_test.sh | tee -a $log

jffs2_test.sh >> $log 2>&1 || exit_failure "jffs2_test.sh failed"

rm -f *

cd $cwd || exit_failure "cd failed"

umount /mnt/test_file_system || exit_failure "umount failed"

ubirmvol -d$devno -n0 || exit_failure "ubirmvol failed"

major=`cat /sys/class/ubi/ubi${devno}/dev | cut -d: -f1`

for minor in `seq 0 32`; do
	if test ! -e /dev/ubi${devno}_$minor ;
	then
		mknod /dev/ubi${devno}_$minor c $major $(($minor + 1))
	fi
done

rm -f testdata.bin readdata.bin

echo Running ubi_jffs2_test.sh | tee -a $log

ubi_jffs2_test.sh >> $log 2>&1 || exit_failure "ubi_jffs2_test.sh failed"

echo Running ubi_test.sh | tee -a $log

ubi_test.sh >> $log 2>&1 || exit_failure "ubi_test.sh failed"

for minor in `seq 0 32`; do
	if test -e /sys/class/ubi/ubi${devno}/$minor;
	then
		ubirmvol -d$devno -n$minor || exit_failure "ubirmvol failed"
	fi
done

echo Running ubi_tools_test.sh | tee -a $log

ubi_tools_test.sh >> $log 2>&1 || exit_failure "ubi_tools_test failed"

rm -f $log

exit_success
