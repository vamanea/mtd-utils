/*
 * FILE flash_lock.c
 *
 * This utility locks one or more sectors of flash device.
 *
 */

#define PROGRAM_NAME "flash_lock"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <string.h>

#include <mtd/mtd-user.h>

int main(int argc, char *argv[])
{
	int fd;
	struct mtd_info_user mtdInfo;
	struct erase_info_user mtdLockInfo;
	int count;

	/*
	 * Parse command line options
	 */
	if (argc < 2) {
		fprintf(stderr, "USAGE: %s <mtd device> <offset> <block count>\n", PROGRAM_NAME);
		exit(1);
	} else if (strncmp(argv[1], "/dev/mtd", 8) != 0) {
		fprintf(stderr, "'%s' is not a MTD device.  Must specify mtd device: /dev/mtd?\n", argv[1]);
		exit(1);
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Could not open mtd device: %s\n", argv[1]);
		exit(1);
	}

	if (ioctl(fd, MEMGETINFO, &mtdInfo)) {
		fprintf(stderr, "Could not get MTD device info from %s\n", argv[1]);
		close(fd);
		exit(1);
	}

	if (argc > 2)
		mtdLockInfo.start = strtol(argv[2], NULL, 0);
	else
		mtdLockInfo.start = 0;
	if (mtdLockInfo.start > mtdInfo.size) {
		fprintf(stderr, "%#x is beyond device size %#x\n",
			mtdLockInfo.start, mtdInfo.size);
		close(fd);
		exit(1);
	}

	if (argc > 3) {
		count = strtol(argv[3], NULL, 0);
		if (count == -1)
			mtdLockInfo.length = mtdInfo.size - mtdInfo.erasesize;
		else
			mtdLockInfo.length = mtdInfo.erasesize * count;
	} else {
		mtdLockInfo.length = mtdInfo.size - mtdInfo.erasesize;
	}
	if (mtdLockInfo.start + mtdLockInfo.length > mtdInfo.size) {
		fprintf(stderr, "lock range is more than device supports\n");
		exit(1);
	}

	if (ioctl(fd, MEMLOCK, &mtdLockInfo)) {
		fprintf(stderr, "Could not lock MTD device: %s\n", argv[1]);
		close(fd);
		exit(1);
	}

	return 0;
}
