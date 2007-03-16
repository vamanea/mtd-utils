#!/bin/sh
#
# UBI Volume creation/deletion/write/read test script
#
# Written in shell language to reduce dependencies to more sophisticated 
# interpreters, which may not be available on some stupid platforms.
#
# Author: Frank Haverkamp <haver@vnet.ibm.com>
#
# 1.0 Initial version
# 1.1 Use ubiupdatevol instead of ubiwritevol
#

VERSION="1.1"

export PATH=$PATH:~/bin:/usr/local/bin:/home/dedekind/work/prj/ubi/tools/flashutils/bin/

UBIMKVOL=ubimkvol
UBIRMVOL=ubirmvol
UBIUPDATEVOL=ubiupdatevol

# 128 KiB 131072
# 256 KiB 262144
# 512 KiB 524288

SIZE_512K=524288
SIZE_1M=1310720

SELF=$0
MINVOL=10
MAXVOL=12

#
# To have a standardized output I define the following function to be
# used when a test was ok or when it failed.
#
failed () 
{
    echo "FAILED"
}

passed ()
{
    echo "PASSED"
}

#
# Print sucess message. Consider to exit with zero as return code.
#
exit_success ()
{
    echo "SUCCESS"
    exit 0
}

#
# Print failure message. Consider to exit with non zero return code.
#
exit_failure ()
{
    echo "FAILED"
    exit 1
}

###############################################################################
#
# START
#
###############################################################################

fix_sysfs_issue ()
{
    echo -n "*** Fixing the sysfs issue with the /dev nodes ... "

    minor=0
    major=`grep ubi0 /proc/devices | sed -e 's/\(.*\) ubi0/\1/'`

    rm -rf /dev/ubi0
    mknod /dev/ubi0 c $major 0

    for minor in `seq 0 $MAXVOL`; do
	### echo " mknod /dev/ubi0_$minor c $major $(($minor + 1))"
        rm -rf /dev/ubi0_$minor
        mknod /dev/ubi0_$minor c $major $(($minor + 1))
    done
    passed
}

# delete_volume - Delete a volume. If it does not exist, do not try
#                 to delete it.
# @id:     volume id
#
delete_volume ()
{
    volume=$1

    ### FIXME broken sysfs!!!!
    if [ -e /sys/class/ubi/$volume -o -e /sys/class/ubi/ubi0/$volume -o -e /sys/class/ubi/ubi0_$volume ]; then

	echo -n "*** Truncate volume if it exists ... "
	$UBIUPDATEVOL -d0 -n$volume -t
	if [ $? -ne "0" ] ; then
	    exit_failure
	fi
	passed

	echo -n "*** Delete volume if it exists ... "
	$UBIRMVOL -d0 -n$volume
	if [ $? -ne "0" ] ; then
	    exit_failure
	fi
	passed
    fi
}

mkvol_rmvol_test ()
{
    type=$1

### Test if volume delete on non-existing volumes fails nicely

    for i in `seq $MINVOL $MAXVOL`; do
	echo "*** Delete if exist or not $i ... "

	delete_volume $i
	passed
    done

### Now deleting volumes must fail

    for i in `seq $MINVOL $MAXVOL`; do
	echo "*** Trying to delete non existing UBI Volume $i ... "

	$UBIRMVOL -d0 -n$i
	if [ $? -eq "0" ] ; then
	    exit_failure
	fi
	passed
    done

### Test if volume creation works ok

    for i in `seq $MINVOL $MAXVOL`; do
	echo "*** Creating UBI Volume $i ... "
	echo "    $UBIMKVOL -d0 -n$i -t$type -NNEW$i -s $SIZE_512K"

	$UBIMKVOL -d0 -n$i -t$type -N"NEW$i" -s $SIZE_512K
	if [ $? -ne "0" ] ; then
	    exit_failure
	fi
	passed
    done

### Now deleting volumes must be ok

    for i in `seq $MINVOL $MAXVOL`; do
	echo "*** Trying to delete UBI Volume $i ... "

	$UBIRMVOL -d0 -n$i
	if [ $? -ne "0" ] ; then
	    exit_failure
	fi
	passed
    done

### Now allocate too large volume

    echo -n "*** Try to create too large volume"
    $UBIMKVOL -d0 -n$MINVOL -t$type -N"NEW$MINVOL" -s 800000000
    if [ $? -eq "0" ] ; then
	exit_failure
    fi
    passed
}

# writevol_test - Tests volume creation and writing data to it.
#
# @size:    Size of random data to write
# @type:    Volume type static or dynamic
#
writevol_test ()
{
    size=$1
    type=$2

    echo "*** Write volume test with size $size"

### Make sure that volume exist, delete existing volume, create new

    delete_volume $MINVOL

    echo -n "*** Try to create volume ... "
    $UBIMKVOL -d0 -n$MINVOL -t$type -N"NEW$MINVOL" -s $SIZE_1M
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed
    
### Try to create same volume again
    echo -n "*** Try to create some volume again, this must fail ... "
    $UBIMKVOL -d0 -n$MINVOL -t$type -N"NEW$MINVOL" -s $SIZE_1M
    if [ $? -eq "0" ] ; then
	exit_failure
    fi
    passed
    
### Now create test data, write it, read it, compare it
    echo -n "*** Create test data ... "
    dd if=/dev/urandom of=testdata.bin bs=$size count=1
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed

    echo "*** Now writing data to volume ... "
    # sleep 5
    ls -l testdata.bin
    echo "    $UBIUPDATEVOL -d0 -n$MINVOL testdata.bin"
    $UBIUPDATEVOL -d0 -n$MINVOL testdata.bin
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed

    if [ $type = "static" ] ; then
	echo "*** Download data with cat ... "
	cat /dev/ubi0_$MINVOL > readdata.bin
	if [ $? -ne "0" ] ; then
	    exit_failure
	fi
	passed
    else
	echo "*** Download data with dd bs=1 ... "
	dd if=/dev/ubi0_$MINVOL of=readdata.bin bs=$size count=1
	if [ $? -ne "0" ] ; then
	    exit_failure
	fi
	passed

	# Size 1 does not work with this test ...
	#
	#echo "*** Download data with dd bs=$size ... "
	#dd if=/dev/ubi0_$MINVOL of=readdata2.bin bs=$size count=1
	#if [ $? -ne "0" ] ; then
	#    exit_failure
	#fi
	#passed

	#echo -n "*** Comparing data (1) ... "
	#cmp readdata.bin readdata2.bin
	#if [ $? -ne "0" ] ; then
	#    exit_failure
	#fi
	#passed
    fi

    echo -n "*** Comparing data ... "
    cmp readdata.bin testdata.bin
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed
}

echo "***********************************************************************"
echo "*           UBI Testing starts now ...                                *"
echo "*                                 Good luck!                          *"
echo "***********************************************************************"

# Set to zero if not running on example hardware
grep ubi /proc/devices > /dev/null
if [ $? -ne "0" ]; then
    echo "No UBI found in /proc/devices! I am broken!"
    exit_failure
fi

# Set to zero if not running on example hardware
grep 1142 /proc/cpuinfo > /dev/null
if [ $? -eq "0" ]; then
    echo "Running on example hardware"
    mount -o remount,rw / /
    sleep 1
    fix_sysfs_issue
else
    echo "Running on Artems hardware"
fi

echo "***********************************************************************"
echo "*        mkvol/rmvol testing for static volumes ...                   *"
echo "***********************************************************************"

mkvol_rmvol_test static

echo "***********************************************************************"
echo "*        mkvol/rmvol testing for dynamic volumes ...                  *"
echo "***********************************************************************"

mkvol_rmvol_test dynamic

echo "***********************************************************************"
echo "*                write to static volumes ...                          *"
echo "***********************************************************************"

# 10 Erase blocks = (128 KiB - 64 * 2) * 10
#                 = 1309440 bytes
# 128 KiB 131072
# 256 KiB 262144
# 512 KiB 524288

for size in 262144 131073 131072 2048 1 4096 12800 31313  ; do
    writevol_test $size static
done

echo "***********************************************************************"
echo "*                write to dynamic volumes ...                         *"
echo "***********************************************************************"
echo "VERSION: $VERSION"

for size in 131073 131072 2048 1 4096 12800 31313 262144 ; do
    writevol_test $size dynamic
done

echo "***********************************************************************"
echo "*               Congratulations, no errors found!                     *"
echo "*              Have fun with your cool UBI system!                    *"
echo "***********************************************************************"

exit_success
