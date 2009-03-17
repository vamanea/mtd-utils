#!/bin/sh
#
# Testcase for nand2bin and bin2nand. Generate testdata and inject
# biterrors. Convert data back and compare with original data.
#
# Conversion:
#    bin -> bin2nand -> mif -> nand2bin -> img
#

inject_biterror=./scripts/inject_biterror.pl

pagesize=2048
oobsize=64

# Create test data
dd if=/dev/urandom of=testblock.bin bs=131072 count=1

echo "Test conversion without bitflips ..."

echo -n "Convert bin to mif ... "
bin2nand --pagesize=${pagesize} -o testblock.mif testblock.bin
if [ $? -ne "0" ]; then
    echo "failed!"
    exit 1
else
    echo "ok"
fi

echo -n "Convert mif to bin ... "
nand2bin --pagesize=${pagesize} -o testblock.img testblock.mif
if [ $? -ne "0" ]; then
    echo "failed!"
    exit 1
else
    echo "ok"
fi

echo -n "Comparing data ... "
diff testblock.bin testblock.img
if [ $? -ne "0" ]; then
    echo "failed!"
    exit 1
else
    echo "ok"
fi

echo "Test conversion with uncorrectable ECC erors ..."
echo -n "Inject biterror at offset $ioffs ... "
${inject_biterror} --offset=0 --bitmask=0x81 \
    --input=testblock.mif \
    --output=testblock_bitflip.mif
if [ $? -ne "0" ]; then
    echo "failed!"
    exit 1
else
    echo "ok"
fi

echo "Convert mif to bin ... "
rm testblock.img
nand2bin --correct-ecc --pagesize=${pagesize} -o testblock.img \
    testblock_bitflip.mif
if [ $? -ne "0" ]; then
    echo "failed!"
    exit 1
else
    echo "ok"
fi

echo -n "Comparing data, must fail due to uncorrectable ECC ... "
diff testblock.bin testblock.img
if [ $? -ne "0" ]; then
    echo "ok" # Must fail!
else
    echo "failed!"
    exit 1
fi

echo "Test bitflips in data ... "
for offs in `seq 0 255` ; do

    cp testblock.mif testblock_bitflip.mif

    for xoffs in 0 256 512 768 ; do
	let ioffs=$offs+$xoffs

	cp testblock_bitflip.mif testblock_bitflip_tmp.mif
	echo -n "Inject biterror at offset $ioffs ... "
	${inject_biterror} --offset=${ioffs} --bitmask=0x01 \
	    --input=testblock_bitflip_tmp.mif \
	    --output=testblock_bitflip.mif
	if [ $? -ne "0" ]; then
	    echo "failed!"
	    exit 1
	else
	    echo "ok"
	fi
    done

    echo "Convert mif to bin ... "
    rm testblock.img
    nand2bin --correct-ecc --pagesize=${pagesize} -o testblock.img \
	testblock_bitflip.mif
    if [ $? -ne "0" ]; then
	echo "failed!"
	exit 1
    else
	echo "ok"
    fi

    echo -n "Comparing data ... "
    diff testblock.bin testblock.img
    if [ $? -ne "0" ]; then
	hexdump testblock.bin > testblock.bin.txt
	hexdump testblock.img > testblock.img.txt
	echo "Use tkdiff testblock.bin.txt testblock.img.txt to compare"
	echo "failed!"
	exit 1
    else
	echo "ok"
    fi

    # Without correction
    echo "Convert mif to bin ... "
    rm testblock.img
    nand2bin --pagesize=${pagesize} -o testblock.img \
	testblock_bitflip.mif
    if [ $? -ne "0" ]; then
	echo "failed!"
	exit 1
    else
	echo "ok"
    fi

    echo -n "Comparing data must differ, correction is disabled ... "
    diff testblock.bin testblock.img
    if [ $? -ne "0" ]; then
	echo "ok" # must fail
    else
	echo "failed!"
	exit 1
    fi
done

echo "Test bitflips in OOB data ... "
for offs in `seq 0 $oobsize` ; do

    let ioffs=$pagesize+$offs

    echo -n "Inject biterror at offset $ioffs ... "
    ${inject_biterror} --offset=${ioffs} --bitmask=0x01 \
	--input=testblock.mif \
	--output=testblock_bitflip.mif
    if [ $? -ne "0" ]; then
	echo "failed!"
	exit 1
    else
	echo "ok"
    fi

    echo "Convert mif to bin ... "
    rm testblock.img
    nand2bin --correct-ecc --pagesize=${pagesize} -o testblock.img \
	testblock_bitflip.mif
    if [ $? -ne "0" ]; then
	echo "failed!"
	exit 1
    else
	echo "ok"
    fi

    echo -n "Comparing data ... "
    diff testblock.bin testblock.img
    if [ $? -ne "0" ]; then
	hexdump testblock.bin > testblock.bin.txt
	hexdump testblock.img > testblock.img.txt
	echo "Use tkdiff testblock.bin.txt testblock.img.txt to compare"
	echo "failed!"
	exit 1
    else
	echo "ok"
    fi
done

