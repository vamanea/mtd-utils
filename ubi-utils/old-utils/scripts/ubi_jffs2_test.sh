#!/bin/sh
#
# UBI Volume creation/deletion/write/read and JFFS2 on top of UBI
# testcases.
#
# Written in shell language to reduce dependencies to more sophisticated
# interpreters, which may not be available on some stupid platforms.
#
# Author: Frank Haverkamp <haver@vnet.ibm.com>
#
# 1.0 Initial version
# 1.1 Added fixup for delayed device node creation by udev
#     This points to a problem in the tools, mabe in the desing
#     Tue Oct 31 14:14:54 CET 2006
#

VERSION="1.1"

export PATH=$PATH:/bin:~/bin:/usr/local/bin:/home/dedekind/work/prj/ubi/tools/flashutils/bin/

ITERATIONS=250
ALIGNMENT=2048

UBIMKVOL="ubimkvol -a $ALIGNMENT"
UBIRMVOL=ubirmvol
UBIUPDATEVOL=ubiupdatevol

SIZE_512K=524288
SIZE_1M=1310720

MINVOL=10
MAXVOL=12

TLOG=/dev/null

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
    echo "*** Fixing the sysfs issue with the /dev nodes ... "

    minor=0
    major=`grep ubi0 /proc/devices | sed -e 's/\(.*\) ubi0/\1/'`

    rm -rf /dev/ubi0
    mknod /dev/ubi0 c $major 0

    for minor in `seq $MINVOL $MAXVOL`; do
	echo " -> mknod /dev/ubi0_$minor c $major $(($minor + 1))"
        rm -rf /dev/ubi0_$minor
        mknod /dev/ubi0_$minor c $major $(($minor + 1))
    done
    passed
}

#
# FIXME Udev needs some time until the device nodes are created.
#       This will cause trouble if after ubimkvol an update attempt
#       is started immediately, since the device node is not yet
#       available. We should either fix the tools with inotify or
#       other ideas or figure out a different way to solve the problem
#       e.g. to use ubi0 and make the volume device nodes obsolete...
#
udev_wait ()
{
    echo -n "FIXME Waiting for udev to create/delete device node "
    grep 2\.6\.5 /proc/version > /dev/null
    if [ $? -eq "0" ]; then
	for i in `seq 0 5`; do
	    sleep 1; echo -n ".";
	done
	echo " ok"
    fi
}

# delete_volume - Delete a volume. If it does not exist, do not try
#                 to delete it.
# @id:     volume id
#
delete_volume ()
{
    volume=$1

    ### FIXME broken sysfs!!!!
    if [ -e /sys/class/ubi/$volume -o \
	 -e /sys/class/ubi/ubi0/$volume -o \
	 -e /sys/class/ubi/ubi0_$volume ]; then

	echo "*** Truncate volume if it exists ... "
	echo "    $UBIUPDATEVOL -d0 -n$volume -t"
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
	# udev_wait
    fi
}

# writevol_test - Tests volume creation and writing data to it.
#
# @volume:  Volume number
# @size:    Size of random data to write
# @type:    Volume type static or dynamic
#
writevol_test ()
{
    volume=$1
    size=$2
    type=$3

    echo "*** Write volume test with size $size"

### Make sure that volume exist, delete existing volume, create new

    delete_volume $volume

    echo "*** Try to create volume"
    echo "    $UBIMKVOL -d0 -n$volume -t$type -NNEW$volume -s $size ... "
    $UBIMKVOL -d0 -n$volume -t$type -N"NEW$volume" -s $size
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed
    udev_wait

### Try to create same volume again
    echo -n "*** Try to create some volume again, this must fail ... "
    $UBIMKVOL -d0 -n$volume -t$type -N"NEW$volume" -s $size
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
    echo "    $UBIUPDATEVOL -d0 -n$volume testdata.bin"
    ls -l testdata.bin
    $UBIUPDATEVOL -d0 -n$volume testdata.bin
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed

    echo "*** Download data with dd bs=1 ... "
    dd if=/dev/ubi0_$volume of=readdata.bin bs=$size count=1
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed

    echo -n "*** Comparing data ... "
    cmp readdata.bin testdata.bin
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed

    echo -n "*** Now truncate volume ... "
    $UBIUPDATEVOL -d0 -n$volume -t
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed
}

jffs2_torture ()
{
    cat /dev/null > TLOG

    echo "*** Torture test ... "

    for i in `seq $iterations`; do
	dd if=/dev/urandom of=test.bin bs=$i count=1 2>> $TLOG
	if [ $? -ne "0" ] ; then
	    echo "Testing $i byte (dd if=/dev/urandom of=foo bs=$i count=1) ... "
	    exit_failure
	fi
	#passed

	dd if=test.bin of=new.bin bs=$i count=1 2>> $TLOG
	if [ $? -ne "0" ] ; then
	    echo "dd if=test.bin of=new.bin bs=$i count=1 2>> $TLOG"
	    exit_failure
	fi
	#passed

	#echo "Comparing files ... "
	cmp test.bin new.bin
	dd if=test.bin of=new.bin bs=$i count=1 2>> $TLOG
	if [ $? -ne "0" ] ; then
	    exit_failure
	fi
	#passed
	#echo -n "."
    done

    echo -n "step0:ok "

    for i in `seq $iterations`; do
	dd if=/dev/urandom of=foo bs=$i count=1 2>> $TLOG
	if [ $? -ne "0" ] ; then
	    echo "Testing $i byte (dd if=/dev/urandom of=foo bs=$i count=1) ... "
	    exit_failure
	fi
	#passed
    done

    echo -n "step1:ok "

    for i in `seq $iterations`; do
	dd if=/dev/zero of=foo bs=1 count=$i 2>> $TLOG
	if [ $? -ne "0" ] ; then
	    echo "Testing $i byte (dd if=/dev/zero of=foo bs=1 count=$i) ... "
	    exit_failure
	fi
	#passed
    done

    echo -n "step2:ok "

    for i in `seq $iterations`; do
	dd if=/dev/zero of=foo bs=$i count=16 2>> $TLOG
	if [ $? -ne "0" ] ; then
	    echo "Testing $i byte (dd if=/dev/zero of=foo bs=$i count=1024) ... "
	    exit_failure
	fi
	#passed
    done

    echo -n "step3:ok "

    passed
}

# writevol_test - Tests volume creation and writing data to it.
#
# @volume:  Volume number
# @size:    Size of random data to write
# @type:    Volume type static or dynamic
#
jffs2_test ()
{
    name=$1
    iterations=$2
    directory=`pwd`

    ### Setup
    ulimit -c unlimited

    echo -n "*** Create directory /mnt/$name ... "
    mkdir -p /mnt/$name
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed

    echo -n "*** mount -t jffs2 mtd:$name /mnt/$name ... "
    mount -t jffs2 mtd:$name /mnt/$name
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed

    echo -n "*** change directory ... "
    cd /mnt/$name
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed

    ls
    echo "*** list directory ... "
    ls -la
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed

    ### Torture
    echo -n "*** touch I_WAS_HERE ... "
    touch I_WAS_HERE
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed

    jffs2_torture

    echo "*** list directory ... "
    ls -la
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed

    ### Cleanup
    echo -n "*** go back ... "
    cd $directory
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed

    ### Still mounted, ubiupdatevol must fail!

    echo -n "*** $UBIUPDATEVOL -d0 -n$volume -t must fail! ..."
    $UBIUPDATEVOL -d0 -n$volume -t
    if [ $? -eq "0" ] ; then
	exit_failure
    fi
    passed

    echo -n "*** umount /mnt/$name ... "
    umount /mnt/$name
    if [ $? -ne "0" ] ; then
	exit_failure
    fi
    passed

    return
}

echo "***********************************************************************"
echo "*           UBI JFFS2 Testing starts now ...                          *"
echo "*                                 Good luck!                          *"
echo "***********************************************************************"
echo "VERSION: $VERSION"

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

for volume in `seq $MINVOL $MAXVOL`; do
    echo -n "************ VOLUME $volume NEW$volume "
    echo "******************************************"
    writevol_test $volume $SIZE_1M dynamic
    jffs2_test NEW$volume $ITERATIONS
    delete_volume $volume
done

echo "***********************************************************************"
echo "*               Congratulations, no errors found!                     *"
echo "*              Have fun with your cool UBI system!                    *"
echo "***********************************************************************"

exit_success
