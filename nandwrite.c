/*
 *  nandwrite.c
 *
 *  Copyright (C) 2000 Steven J. Hill (sjhill@realitydiluted.com)
 *		  2003 Thomas Gleixner (tglx@linutronix.de)
 *
 * $Id: nandwrite.c,v 1.32 2005/11/07 11:15:13 gleixner Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Overview:
 *   This utility writes a binary image directly to a NAND flash
 *   chip or NAND chips contained in DoC devices. This is the
 *   "inverse operation" of nanddump.
 *
 * tglx: Major rewrite to handle bad blocks, write data with or without ECC
 *	 write oob data only on request
 *
 * Bug/ToDo:
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <getopt.h>

#include <asm/types.h>
#include "mtd/mtd-user.h"

#define PROGRAM "nandwrite"
#define VERSION "$Revision: 1.32 $"

#define MAX_PAGE_SIZE	2048
#define MAX_OOB_SIZE	64

/*
 * Buffer array used for writing data
 */
unsigned char writebuf[MAX_PAGE_SIZE];
unsigned char oobbuf[MAX_OOB_SIZE];
unsigned char oobreadbuf[MAX_OOB_SIZE];

// oob layouts to pass into the kernel as default
struct nand_oobinfo none_oobinfo = {
	.useecc = MTD_NANDECC_OFF,
};

struct nand_oobinfo jffs2_oobinfo = {
	.useecc = MTD_NANDECC_PLACE,
	.eccbytes = 6,
	.eccpos = { 0, 1, 2, 3, 6, 7 }
};

struct nand_oobinfo yaffs_oobinfo = {
	.useecc = MTD_NANDECC_PLACE,
	.eccbytes = 6,
	.eccpos = { 8, 9, 10, 13, 14, 15}
};

struct nand_oobinfo autoplace_oobinfo = {
	.useecc = MTD_NANDECC_AUTOPLACE
};

void display_help (void)
{
	printf("Usage: nandwrite [OPTION] MTD_DEVICE INPUTFILE\n"
	       "Writes to the specified MTD device.\n"
	       "\n"
	       "  -a, --autoplace	Use auto oob layout\n"
	       "  -j, --jffs2		force jffs2 oob layout (legacy support)\n"
	       "  -y, --yaffs		force yaffs oob layout (legacy support)\n"
	       "  -f, --forcelegacy     force legacy support on autoplacement enabled mtd device\n"
	       "  -n, --noecc		write without ecc\n"
	       "  -o, --oob		image contains oob data\n"
	       "  -s addr, --start=addr set start address (default is 0)\n"
	       "  -p, --pad             pad to page size\n"
	       "  -b, --blockalign=1|2|4 set multiple of eraseblocks to align to\n"
	       "  -q, --quiet		don't display progress messages\n"
	       "      --help		display this help and exit\n"
	       "      --version		output version information and exit\n");
	exit(0);
}

void display_version (void)
{
	printf(PROGRAM " " VERSION "\n"
	       "\n"
	       "Copyright (C) 2003 Thomas Gleixner \n"
	       "\n"
	       PROGRAM " comes with NO WARRANTY\n"
	       "to the extent permitted by law.\n"
	       "\n"
	       "You may redistribute copies of " PROGRAM "\n"
	       "under the terms of the GNU General Public Licence.\n"
	       "See the file `COPYING' for more information.\n");
	exit(0);
}

char	*mtd_device, *img;
int	mtdoffset = 0;
int	quiet = 0;
int	writeoob = 0;
int	autoplace = 0;
int	forcejffs2 = 0;
int	forceyaffs = 0;
int	forcelegacy = 0;
int	noecc = 0;
int	pad = 0;
int	blockalign = 1; /*default to using 16K block size */

void process_options (int argc, char *argv[])
{
	int error = 0;

	for (;;) {
		int option_index = 0;
		static const char *short_options = "ab:fjnopqs:y";
		static const struct option long_options[] = {
			{"help", no_argument, 0, 0},
			{"version", no_argument, 0, 0},
			{"autoplace", no_argument, 0, 'a'},
			{"blockalign", required_argument, 0, 'b'},
			{"forcelegacy", no_argument, 0, 'f'},
			{"jffs2", no_argument, 0, 'j'},
			{"noecc", no_argument, 0, 'n'},
			{"oob", no_argument, 0, 'o'},
			{"pad", no_argument, 0, 'p'},
			{"quiet", no_argument, 0, 'q'},
			{"start", required_argument, 0, 's'},
			{"yaffs", no_argument, 0, 'y'},
			{0, 0, 0, 0},
		};

		int c = getopt_long(argc, argv, short_options,
				    long_options, &option_index);
		if (c == EOF) {
			break;
		}

		switch (c) {
		case 0:
			switch (option_index) {
			case 0:
				display_help();
				break;
			case 1:
				display_version();
				break;
			}
			break;
		case 'q':
			quiet = 1;
			break;
		case 'a':
			autoplace = 1;
			break;
		case 'j':
			forcejffs2 = 1;
			break;
		case 'y':
			forceyaffs = 1;
			break;
		case 'f':
			forcelegacy = 1;
			break;
		case 'n':
			noecc = 1;
			break;
		case 'o':
			writeoob = 1;
			break;
		case 'p':
			pad = 1;
			break;
		case 's':
			mtdoffset = strtol (optarg, NULL, 0);
			break;
		case 'b':
			blockalign = atoi (optarg);
			break;
		case '?':
			error = 1;
			break;
		}
	}

	if ((argc - optind) != 2 || error)
		display_help ();

	mtd_device = argv[optind++];
	img = argv[optind];
}

/*
 * Main program
 */
int main(int argc, char **argv)
{
	int cnt, fd, ifd, imglen = 0, pagelen, baderaseblock, blockstart = -1;
	struct mtd_info_user meminfo;
	struct mtd_oob_buf oob;
	loff_t offs;
	int ret, readlen;
	int oobinfochanged = 0;
	struct nand_oobinfo old_oobinfo;

	process_options(argc, argv);

	memset(oobbuf, 0xff, sizeof(oobbuf));

	if (pad && writeoob) {
		fprintf(stderr, "Can't pad when oob data is present.\n");
		exit(1);
	}

	/* Open the device */
	if ((fd = open(mtd_device, O_RDWR)) == -1) {
		perror("open flash");
		exit(1);
	}

	/* Fill in MTD device capability structure */
	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		perror("MEMGETINFO");
		close(fd);
		exit(1);
	}

	/* Set erasesize to specified number of blocks - to match jffs2
	 * (virtual) block size */
	meminfo.erasesize *= blockalign;

	/* Make sure device page sizes are valid */
	if (!(meminfo.oobsize == 16 && meminfo.writesize == 512) &&
	    !(meminfo.oobsize == 8 && meminfo.writesize == 256) &&
	    !(meminfo.oobsize == 64 && meminfo.writesize == 2048)) {
		fprintf(stderr, "Unknown flash (not normal NAND)\n");
		close(fd);
		exit(1);
	}

	if (autoplace) {
		/* Read the current oob info */
		if (ioctl (fd, MEMGETOOBSEL, &old_oobinfo) != 0) {
			perror ("MEMGETOOBSEL");
			close (fd);
			exit (1);
		}

		// autoplace ECC ?
		if (autoplace && (old_oobinfo.useecc != MTD_NANDECC_AUTOPLACE)) {

			if (ioctl (fd, MEMSETOOBSEL, &autoplace_oobinfo) != 0) {
				perror ("MEMSETOOBSEL");
				close (fd);
				exit (1);
			}
			oobinfochanged = 1;
		}
	}

	if (noecc)  {
		switch (ioctl(fd, MTDFILEMODE, (void *) MTD_MODE_RAW)) {

		case -ENOTTY:
			if (ioctl (fd, MEMGETOOBSEL, &old_oobinfo) != 0) {
				perror ("MEMGETOOBSEL");
				close (fd);
				exit (1);
			}
			if (ioctl (fd, MEMSETOOBSEL, &none_oobinfo) != 0) {
				perror ("MEMSETOOBSEL");
				close (fd);
				exit (1);
			}
			oobinfochanged = 1;
			break;

		case 0:
			oobinfochanged = 2;
			break;
		default:
			perror ("MTDFILEMODE");
			close (fd);
			exit (1);
		}
	}

	/*
	 * force oob layout for jffs2 or yaffs ?
	 * Legacy support
	 */
	if (forcejffs2 || forceyaffs) {
		struct nand_oobinfo *oobsel = forcejffs2 ? &jffs2_oobinfo : &yaffs_oobinfo;

		if (autoplace) {
			fprintf(stderr, "Autoplacement is not possible for legacy -j/-y options\n");
			goto restoreoob;
		}
		if ((old_oobinfo.useecc == MTD_NANDECC_AUTOPLACE) && !forcelegacy) {
			fprintf(stderr, "Use -f option to enforce legacy placement on autoplacement enabled mtd device\n");
			goto restoreoob;
		}
		if (meminfo.oobsize == 8) {
			if (forceyaffs) {
				fprintf (stderr, "YAFSS cannot operate on 256 Byte page size");
				goto restoreoob;
			}
			/* Adjust number of ecc bytes */
			jffs2_oobinfo.eccbytes = 3;
		}

		if (ioctl (fd, MEMSETOOBSEL, oobsel) != 0) {
			perror ("MEMSETOOBSEL");
			goto restoreoob;
		}
	}

	oob.length = meminfo.oobsize;
	oob.ptr = noecc ? oobreadbuf : oobbuf;

	/* Open the input file */
	if ((ifd = open(img, O_RDONLY)) == -1) {
		perror("open input file");
		goto restoreoob;
	}

	// get image length
	imglen = lseek(ifd, 0, SEEK_END);
	lseek (ifd, 0, SEEK_SET);

	pagelen = meminfo.writesize + ((writeoob == 1) ? meminfo.oobsize : 0);

	// Check, if file is pagealigned
	if ((!pad) && ((imglen % pagelen) != 0)) {
		fprintf (stderr, "Input file is not page aligned\n");
		goto closeall;
	}

	// Check, if length fits into device
	if ( ((imglen / pagelen) * meminfo.writesize) > (meminfo.size - mtdoffset)) {
		fprintf (stderr, "Image %d bytes, NAND page %d bytes, OOB area %u bytes, device size %u bytes\n",
				imglen, pagelen, meminfo.writesize, meminfo.size);
		perror ("Input file does not fit into device");
		goto closeall;
	}

	/* Get data from input and write to the device */
	while (imglen && (mtdoffset < meminfo.size)) {
		// new eraseblock , check for bad block(s)
		// Stay in the loop to be sure if the mtdoffset changes because
		// of a bad block, that the next block that will be written to
		// is also checked. Thus avoiding errors if the block(s) after the
		// skipped block(s) is also bad (number of blocks depending on
		// the blockalign
		while (blockstart != (mtdoffset & (~meminfo.erasesize + 1))) {
			blockstart = mtdoffset & (~meminfo.erasesize + 1);
			offs = blockstart;
			baderaseblock = 0;
			if (!quiet)
				fprintf (stdout, "Writing data to block %x\n", blockstart);

			/* Check all the blocks in an erase block for bad blocks */
			do {
				if ((ret = ioctl(fd, MEMGETBADBLOCK, &offs)) < 0) {
					perror("ioctl(MEMGETBADBLOCK)");
					goto closeall;
				}
				if (ret == 1) {
					baderaseblock = 1;
					if (!quiet)
						fprintf (stderr, "Bad block at %x, %u block(s) "
							 "from %x will be skipped\n",
							 (int) offs, blockalign, blockstart);
				}

				if (baderaseblock) {
					mtdoffset = blockstart + meminfo.erasesize;
				}
				offs +=  meminfo.erasesize / blockalign ;
			} while ( offs < blockstart + meminfo.erasesize );

		}

		readlen = meminfo.writesize;
		if (pad && (imglen < readlen))
		{
			readlen = imglen;
			memset(writebuf + readlen, 0xff, meminfo.writesize - readlen);
		}

		/* Read Page Data from input file */
		if ((cnt = read(ifd, writebuf, readlen)) != readlen) {
			if (cnt == 0)	// EOF
				break;
			perror ("File I/O error on input file");
			goto closeall;
		}

		if (writeoob) {
			/* Read OOB data from input file, exit on failure */
			if ((cnt = read(ifd, oobreadbuf, meminfo.oobsize)) != meminfo.oobsize) {
				perror ("File I/O error on input file");
				goto closeall;
			}
			if (!noecc) {
				int i, start, len;
				/*
				 *  We use autoplacement and have the oobinfo with the autoplacement
				 * information from the kernel available
				 *
				 * Modified to support out of order oobfree segments,
				 * such as the layout used by diskonchip.c
				 */
				if (!oobinfochanged && (old_oobinfo.useecc == MTD_NANDECC_AUTOPLACE)) {
					for (i = 0;old_oobinfo.oobfree[i][1]; i++) {
						/* Set the reserved bytes to 0xff */
						start = old_oobinfo.oobfree[i][0];
						len = old_oobinfo.oobfree[i][1];
						memcpy(oobbuf + start,
							oobreadbuf + start,
							len);
					}
				} else {
					/* Set at least the ecc byte positions to 0xff */
					start = old_oobinfo.eccbytes;
					len = meminfo.oobsize - start;
					memcpy(oobbuf + start,
						oobreadbuf + start,
						len);
				}
			}
			/* Write OOB data first, as ecc will be placed in there*/
			oob.start = mtdoffset;
			if (ioctl(fd, MEMWRITEOOB, &oob) != 0) {
				perror ("ioctl(MEMWRITEOOB)");
				goto closeall;
			}
			imglen -= meminfo.oobsize;
		}

		/* Write out the Page data */
		if (pwrite(fd, writebuf, meminfo.writesize, mtdoffset) != meminfo.writesize) {
			perror ("pwrite");
			goto closeall;
		}
		imglen -= readlen;
		mtdoffset += meminfo.writesize;
	}

 closeall:
	close(ifd);

 restoreoob:
	if (oobinfochanged == 1) {
		if (ioctl (fd, MEMSETOOBSEL, &old_oobinfo) != 0) {
			perror ("MEMSETOOBSEL");
			close (fd);
			exit (1);
		}
	}

	close(fd);

	if (imglen > 0) {
		perror ("Data was only partially written due to error\n");
		exit (1);
	}

	/* Return happy */
	return 0;
}
