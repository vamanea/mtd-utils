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



/*
 * Main program
 */
int main(int argc, char **argv)
{
	int fd;
	int block, i;
	unsigned char *wbuf, *rbuf;
	struct mtd_info_user meminfo;
	struct mtd_ecc_stats oldstats, newstats;
	int seed;
	int pass;
	int nr_passes = 1;

	if (argc == 4) {
		seed = atol(argv[3]);
		argc = 3;
	}
	if (argc == 3) {
		nr_passes = atol(argv[2]);
		argc = 2;
	}
	if (argc != 2) {
		fprintf(stderr, "usage: %s <device> [<passes>] [<random seed>]\n",
			(strrchr(argv[0],',')?:argv[0]-1)+1);
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(1);
	}
	
	if (ioctl(fd, MEMGETINFO, &meminfo)) {
		perror("MEMGETINFO");
		close(fd);
		exit(1);
	}

	wbuf = malloc(meminfo.erasesize * 2);
	if (!wbuf) {
		fprintf(stderr, "Could not allocate %d bytes for buffer\n",
			meminfo.erasesize * 2);
		exit(1);
	}
	rbuf = wbuf + meminfo.erasesize;

	if (ioctl(fd, ECCGETSTATS, &oldstats)) {
		perror("ECCGETSTATS");
		close(fd);
		exit(1);
	}

	printf("ECC corrections: %d\n", oldstats.corrected);
	printf("ECC failures   : %d\n", oldstats.failed);
	printf("Bad blocks     : %d\n", oldstats.badblocks);
	printf("BBT blocks     : %d\n", oldstats.bbtblocks);

	for (pass = 0; pass < nr_passes; pass++) {
		
		for (block = 0; block < meminfo.size / meminfo.erasesize ; block++) {
			loff_t ofs = block * meminfo.erasesize;
			struct erase_info_user er;
			ssize_t len;

			seed = rand();
			srand(seed);

			if (ioctl(fd, MEMGETBADBLOCK, &ofs)) {
				printf("\rBad block at 0x%08x\n", (unsigned)ofs);
				continue;
			}

			printf("\r%08x: erasing... ", (unsigned)ofs);
			fflush(stdout);

			er.start = ofs;
			er.length = meminfo.erasesize;

			if (ioctl(fd, MEMERASE, &er)) {
				perror("MEMERASE");
				exit(1);
			}

			printf("\r%08x: writing...", (unsigned)ofs);
			fflush(stdout);

			for (i=0; i<meminfo.erasesize; i++)
				wbuf[i] = rand();

			len = pwrite(fd, wbuf, meminfo.erasesize, ofs);
			if (len < 0) {
				printf("\n");
				perror("write");
				ioctl(fd, MEMSETBADBLOCK, &ofs);
				continue;
			}
			if (len < meminfo.erasesize) {
				printf("\n");
				fprintf(stderr, "Short write (%d bytes)\n", len);
				exit(1);
			}

			printf("\r%08x: reading...", (unsigned)ofs);
			fflush(stdout);

			len = pread(fd, rbuf, meminfo.erasesize, ofs);
			if (len < meminfo.erasesize) {
				printf("\n");
				if (len)
					fprintf(stderr, "Short read (%d bytes)\n", len);
				else
					perror("read");
				exit(1);
			}
		
			if (ioctl(fd, ECCGETSTATS, &newstats)) {
				printf("\n");
				perror("ECCGETSTATS");
				close(fd);
				exit(1);
			}
		
			if (newstats.corrected > oldstats.corrected) {
				printf("\nECC corrected at %08x\n", (unsigned) ofs);
				oldstats.corrected = newstats.corrected;
			}
			if (newstats.failed > oldstats.failed) {
				printf("\nECC failed at %08x\n", (unsigned) ofs);
				oldstats.corrected = newstats.corrected;
			}
			if (len < meminfo.erasesize)
				exit(1);

			printf("\r%08x: checking...", (unsigned)ofs);
			fflush(stdout);

			if (memcmp(wbuf, rbuf, meminfo.erasesize)) {
				printf("\n");
				fprintf(stderr, "compare failed. seed %d\n", seed);
				for (i=0; i<meminfo.erasesize; i++) {
					if (wbuf[i] != rbuf[i])
						printf("Byte 0x%x is %02x should be %02x\n",
						       i, rbuf[i], wbuf[i]);
				}
				exit(1);
			}
		}
		printf("\nFinished pass %d successfully\n", pass+1);
	}
	/* Return happy */
	return 0;
}
