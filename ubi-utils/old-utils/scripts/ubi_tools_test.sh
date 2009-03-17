#!/bin/sh
#
# UBI Volume creation/deletion/write/read test script.
# Uses our flash update tools and the associated toolchain for flash
# image creation.
#
# Written in shell language to reduce dependencies to more sophisticated 
# interpreters, which may not be available on some stupid platforms.
#
# Author: Frank Haverkamp <haver@vnet.ibm.com>
#
# 1.0 Initial version
#

VERSION="1.0"

export PATH=$PATH:~/bin:/usr/local/bin:/home/dedekind/work/prj/ubi/tools/flashutils/bin/

UBIMKVOL=ubimkvol
UBIRMVOL=ubirmvol
UBIWRITEVOL=ubiupdatevol
PFIFLASH=pfiflash
CMP=cmp

MAXVOL=32

test_pfi=test_complete.pfi
real_pfi=example_complete.pfi

# 128 KiB 131072
# 256 KiB 262144
# 512 KiB 524288

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
	$UBIWRITEVOL -d0 -n$volume -t
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

echo "***********************************************************************"
echo "*           UBI Tools Testing starts now ...                          *"
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
    echo "Running on other hardware"
fi

### Test basic stuff
pfiflash_basic ()
{
    echo "Calling pfiflash with test-data ... "
    echo "    $PFIFLASH $test_pfi"
    $PFIFLASH $test_pfi
    if [ $? -ne "0" ]; then
	echo "Uhhh something went wrong!"
	exit_failure
    fi
    passed
    
    echo "Testing if data is correct 10 and 11 ... "
    $CMP /dev/ubi0_10 /dev/ubi0_11
    if [ $? -ne "0" ]; then
	echo "Mirrored volumes not equal!"
	exit_failure
    fi
    passed
    
    echo "Comparing against original data ... "
    $CMP /dev/ubi0_10 test_u-boot.bin
    if [ $? -ne "0" ]; then
	echo "Compared volume not equal!"
	exit_failure
    fi
    passed
    
    echo "Testing if data is correct 12 and 13 ... "
    $CMP /dev/ubi0_12 /dev/ubi0_13
    if [ $? -ne "0" ]; then
	echo "Mirrored volumes not equal!"
	exit_failure
    fi
    passed
    
    echo "Comparing against original data ... "
    $CMP /dev/ubi0_12 test_vmlinux.bin
    if [ $? -ne "0" ]; then
	echo "Compared volume not equal!"
	exit_failure
    fi
    passed
    
    echo "Testing if data is correct 14 and 15 ... "
    $CMP /dev/ubi0_14 /dev/ubi0_15
    if [ $? -ne "0" ]; then
	echo "Mirrored volumes not equal!"
	exit_failure
    fi
    passed
}

### Test each and everything
pfiflash_advanced ()
{
    if [ -e  example_complete.pfi ]; then
	echo "Calling pfiflash with real data ... "
	$PFIFLASH -p overwrite --complete example_complete.pfi
	if [ $? -ne "0" ]; then
	    echo "Uhhh something went wrong!"
	    exit_failure
	fi
	passed
	
	echo "Testing if data is correct 2 and 3 ... "
	$CMP /dev/ubi0_2 /dev/ubi0_3
	if [ $? -ne "0" ]; then
	    echo "Mirrored volumes not equal!"
	    exit_failure
	fi
	passed
	
	echo "Comparing against original data ... "
	$CMP /dev/ubi0_2 u-boot.bin
	if [ $? -ne "0" ]; then
	    echo "Compared volume not equal!"
	    exit_failure
	fi
	passed
	
	echo "Testing if data is correct 6 and 7 ... "
	$CMP /dev/ubi0_6 /dev/ubi0_7
	if [ $? -ne "0" ]; then
	    echo "Mirrored volumes not equal!"
	    exit_failure
	fi
	passed
	
	echo "Comparing against original data ... "
	$CMP /dev/ubi0_6 vmlinux.bin
	if [ $? -ne "0" ]; then
	    echo "Compared volume not equal!"
	    exit_failure
	fi
	passed
    fi
}

echo "***********************************************************************"
echo "*                Testing pfiflash ...                                 *"
echo "***********************************************************************"
echo "VERSION: $VERSION"

pfiflash_basic
pfiflash_advanced
    
echo "***********************************************************************"
echo "*               Congratulations, no errors found!                     *"
echo "*              Have fun with your cool UBI system!                    *"
echo "***********************************************************************"

exit_success
