/*
 * flash_info.c -- print info about a MTD device
 */

#define PROGRAM_NAME "flash_info"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#include <mtd/mtd-user.h>

int main(int argc, char *argv[])
{
	int regcount;
	int fd;

	if (1 >= argc) {
		fprintf(stderr, "Usage: %s device\n", PROGRAM_NAME);
		return 16;
	}

	/* Open and size the device */
	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "File open error\n");
		return 8;
	}

	if (ioctl(fd, MEMGETREGIONCOUNT, &regcount) == 0) {
		int i;
		region_info_t reginfo;
		printf("Device %s has %d erase regions\n", argv[1], regcount);
		for (i = 0; i < regcount; i++) {
			reginfo.regionindex = i;
			if (ioctl(fd, MEMGETREGIONINFO, &reginfo) == 0) {
				printf("Region %d is at 0x%x with size 0x%x and "
						"has 0x%x blocks\n", i, reginfo.offset,
						reginfo.erasesize, reginfo.numblocks);
			} else {
				printf("Strange can not read region %d from a %d region device\n",
						i, regcount);
			}
		}
	}

	return 0;
}
