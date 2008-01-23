#!/bin/sh
#
# Testcase for JFFS2 verification. We do not want to see any
# kernel errors occuring when this is executed.
#
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

echo "***********************************************************************"
echo "*        jffs2 testing ...                                            *"
echo "***********************************************************************"

ulimit -c unlimited

for i in `seq 5000`; do
    echo "Testing $i byte (dd if=/dev/urandom of=foo bs=$i count=1) ... "
    dd if=/dev/urandom of=test.bin bs=$i count=1;
    if [ $? -ne "0" ] ; then
        exit_failure
    fi
    passed

    echo "Copy to different file ... "
    dd if=test.bin of=new.bin bs=$i count=1;
    if [ $? -ne "0" ] ; then
        exit_failure
    fi
    passed

    echo "Comparing files ... "
    cmp test.bin new.bin
    dd if=test.bin of=new.bin bs=$i count=1;
    if [ $? -ne "0" ] ; then
        exit_failure
    fi
    passed
done

for i in `seq 5000`; do
    echo "Testing $i byte (dd if=/dev/urandom of=foo bs=$i count=1) ... "
    dd if=/dev/urandom of=foo bs=$i count=1;
    if [ $? -ne "0" ] ; then
        exit_failure
    fi
    passed
done

for i in `seq 5000`; do 
    echo "Testing $i byte (dd if=/dev/zero of=foo bs=$i count=1) ... "
    dd if=/dev/zero of=foo bs=$i count=1;
    if [ $? -ne "0" ] ; then
        exit_failure
    fi
    passed
done

echo "***********************************************************************"
echo "*               Congratulations, no errors found!                     *"
echo "*              Have fun with your cool JFFS2 using system!            *"
echo "***********************************************************************"

exit_success
