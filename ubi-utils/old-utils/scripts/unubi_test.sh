#!/bin/sh
#
# Use raw NAND data, extract UBI image and apply tool to it.
# Test basic functionality.
#
# 2007 Frank Haverkamp <haver@vnet.ibm.com>
#

version=1.1

image=data.mif
oob=oob.bin
data=data.bin
pagesize=2048
volmax=31
datadir=unubi_data

# general arguments e.g. debug enablement
# unubi_args="-D"

echo "------------------------------------------------------------------------"
echo "Testcase: ${0} Version: ${version}"
echo "------------------------------------------------------------------------"
echo "Testing nand2bin ..."
echo "  Input:    ${image}"
echo "  Data:     ${data}"
echo "  OOB:      ${oob}"
echo "  Pagesize: ${pagesize}"
nand2bin --pagesize ${pagesize} -o ${data} -O ${oob} ${image}
echo

echo "------------------------------------------------------------------------"
echo "Testing unubi ..."
echo "------------------------------------------------------------------------"
unubi --version
echo

echo "------------------------------------------------------------------------"
echo "Trying to extract first ${volmax} volumes ..."
echo "------------------------------------------------------------------------"
mkdir -p ${datadir}/volumes
for v in `seq 0 ${volmax}` ; do
    unubi ${unubi_args} -r${v} -d${datadir}/volumes ${data}
    echo -n "."
done
echo "ok"
ls -l ${datadir}/volumes
echo

echo "------------------------------------------------------------------------"
echo "Extracting graphics ..."
echo "------------------------------------------------------------------------"
unubi -a  -d${datadir} ${data}
echo "Use gnuplot to display:"
ls ${datadir}/*.plot
ls ${datadir}/*.data
echo

echo "------------------------------------------------------------------------"
echo "eb-split"
echo "------------------------------------------------------------------------"
unubi -e -d${datadir}/eb-split ${data}
ls -l ${datadir}/eb-split
echo

echo "------------------------------------------------------------------------"
echo "vol-split"
echo "------------------------------------------------------------------------"
unubi -v -d${datadir}/vol-split ${data}
ls  -l ${datadir}/vol-split
echo
echo "The generated images contain only the data (126KiB in our   "
echo "case) not including the UBI erase count and volume info     "
echo "header. For dynamic volumes the data should be the full     "
echo "126KiB. Unubi cannot know how much of the data is valid.    "
echo

echo "------------------------------------------------------------------------"
echo "!vol-split"
echo "------------------------------------------------------------------------"
unubi -V -d${datadir}/vol-split! ${data}
ls -l ${datadir}/vol-split\!
echo
echo "The generated images contain the full block data of 128KiB  "
echo "including the UBI erase count and volume information header."
echo

echo "------------------------------------------------------------------------"
echo "Extracting volume info table ..."
echo "------------------------------------------------------------------------"
unubi -i -d${datadir} ${data}
echo "I strongly hope that empty ubi blocks are filled with 0xff! "
echo

echo "------------------------------------------------------------------------"
echo "Table 0"
echo "------------------------------------------------------------------------"
cat ${datadir}/vol_info_table0
echo

echo "------------------------------------------------------------------------"
echo "Table 1"
echo "------------------------------------------------------------------------"
cat ${datadir}/vol_info_table1
echo
