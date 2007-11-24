#!/bin/sh
#
# Use raw NAND data, extract UBI image and apply tool to it.
# Test basic functionality.
#
# 2007 Frank Haverkamp <haver@vnet.ibm.com>
#

version=1.0

image=data.mif
oob=oob.bin
data=data.bin
pagesize=2048
volmax=31
datadir=unubi_data.bin

echo "Testcase: ${0} Version: ${version}"
echo

if [ -f $1 ]; then
    image=${1}
fi

echo "Testing nand2bin ..."
echo "  Input:    ${image}"
echo "  Data:     ${data}"
echo "  OOB:      ${oob}"
echo "  Pagesize: ${pagesize}"

nand2bin --pagesize ${pagesize} -o ${data} -O ${oob} ${image}
echo

echo "Testing unubi ..."
unubi --version

echo "Trying to extract first ${volmax} volumes ..."
for v in `seq 0 ${volmax}` ; do
    unubi -r${v} ${data}
    echo -n "."
done
echo "ok"

ls -l unubi_data.bin/

echo "Extracting graphics ..."
unubi -a ${data}


echo "Extracting volume info table ..."
unubi -i ${data}

echo
echo "Table 0"
echo "-------"
cat ${datadir}/vol_info_table0

echo
echo "Table 1"
echo "-------"
cat ${datadir}/vol_info_table1