#include <errno.h>  	
#include <error.h> 	
#include <netdb.h> 	
#include <stdio.h> 	
#include <stdlib.h> 	
#include <string.h>
#include <unistd.h> 	
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "crc32.h"
#include "mcast_image.h"

int tx_rate = 80000;
int pkt_delay = 12500 * PKT_SIZE / 1024;

int main(int argc, char **argv)
{
	struct addrinfo *ai;
	struct addrinfo hints;
	struct addrinfo *runp;
	int ret;
	int sock;
	struct image_pkt pktbuf;
	int rfd;
	struct stat st;
	int writeerrors = 0;
	uint32_t erasesize;
	unsigned char parbuf[PKT_SIZE];
	unsigned char *image, *blockptr;
	uint32_t block_nr;
	uint32_t block_ofs;
	int nr_blocks;
	uint32_t droppoint = -1;
	struct timeval then, now, nextpkt;
	long time_msecs;

	if (argc == 6) {
		tx_rate = atol(argv[5]) * 1024;
		if (tx_rate < PKT_SIZE || tx_rate > 20000000) {
			fprintf(stderr, "Bogus TX rate %d KiB/s\n", tx_rate);
			exit(1);
		}
		argc = 5;
	}
	       
	if (argc != 5) {
		fprintf(stderr, "usage: %s <host> <port> <image> <erasesize> [<tx_rate>]\n",
			(strrchr(argv[0], '/')?:argv[0]-1)+1);
		exit(1);
	}
	pkt_delay = (sizeof(pktbuf) * 1000000) / tx_rate;
	printf("Inter-packet delay (avg): %dµs\n", pkt_delay);
	printf("Transmit rate: %d KiB/s\n", tx_rate / 1024);

	erasesize = atol(argv[4]);
	if (!erasesize) {
		fprintf(stderr, "erasesize cannot be zero\n");
		exit(1);
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_DGRAM;
	
	ret = getaddrinfo(argv[1], argv[2], &hints, &ai);
	if (ret) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
		exit(1);
	}
	runp = ai;
	for (runp = ai; runp; runp = runp->ai_next) {
		sock = socket(runp->ai_family, runp->ai_socktype,
			      runp->ai_protocol);
		if (sock == -1) {
			perror("socket");
			continue;
		}
		if (connect(sock, runp->ai_addr, runp->ai_addrlen) == 0)
			break;
		perror("connect");
		close(sock);
	}
	if (!runp)
		exit(1);

	rfd = open(argv[3], O_RDONLY);
	if (rfd < 0) {
		perror("open");
		exit(1);
	}

	if (fstat(rfd, &st)) {
		perror("fstat");
		exit(1);
	}

	if (st.st_size % erasesize) {
		fprintf(stderr, "Image size %ld bytes is not a multiple of erasesize %d bytes\n",
			st.st_size, erasesize);
		exit(1);
	}
	image = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, rfd, 0);
	if (image == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	nr_blocks = st.st_size / erasesize;

	printf("Checking CRC....");

	pktbuf.hdr.resend = 0;
	pktbuf.hdr.totcrc = htonl(crc32(-1, image, st.st_size));
	pktbuf.hdr.nr_blocks = htonl(nr_blocks);
	pktbuf.hdr.blocksize = htonl(erasesize);

	printf("%08x\n", ntohl(pktbuf.hdr.totcrc));

	gettimeofday(&then, NULL);
	nextpkt = then;

	blockptr = image;

	for (block_nr = 0; block_nr < nr_blocks; block_nr++) {
		int len;
		int dropped = 0;

		pktbuf.hdr.block_nr = htonl(block_nr);

		for (block_ofs = 0; block_ofs <= erasesize; block_ofs += len) {
			int i;
			
			if (block_ofs + PKT_SIZE > erasesize)
				len = erasesize - block_ofs;
			else
				len = PKT_SIZE;

			if (block_ofs == erasesize) {
				gettimeofday(&now, NULL);

				time_msecs = (now.tv_sec - then.tv_sec) * 1000;
				time_msecs += ((int)(now.tv_usec - then.tv_usec)) / 1000;

				printf("\rSending parity block: %08x     (%ld KiB/s)    ",
				       block_nr * erasesize,
				       (block_ofs + (block_nr * (erasesize+sizeof(pktbuf)))) / 1024 * 1000 / time_msecs);

				len = PKT_SIZE;
				memcpy(pktbuf.data, parbuf, PKT_SIZE);
			} else {
				if (!block_ofs)
					memcpy(parbuf, blockptr, PKT_SIZE);
				else for (i=0; i < len; i++)
					     parbuf[i] ^= blockptr[i];

				memcpy(pktbuf.data, blockptr, len);
				printf("\rSending data block at %08x",
				       block_nr * erasesize + block_ofs);
				blockptr += len;
			}

			fflush(stdout);
			pktbuf.hdr.thislen = htonl(len);
			pktbuf.hdr.block_ofs = htonl(block_ofs);
			pktbuf.hdr.thiscrc = htonl(crc32(-1, pktbuf.data, len));

			if (droppoint == block_ofs && !dropped) {
				dropped = 1;
				if (droppoint == 0)
					droppoint = erasesize;
				else if (droppoint == erasesize)
					droppoint = ((erasesize - 1) / PKT_SIZE) * PKT_SIZE;
				else droppoint -= PKT_SIZE;
				printf("\nDropping data block at %08x\n", block_ofs);
				continue;
			}

			if (write(sock, &pktbuf, sizeof(pktbuf.hdr)+len) < 0) {
				perror("write");
				writeerrors++;
				if (writeerrors > 10) {
					fprintf(stderr, "Too many consecutive write errors\n");
					exit(1);
				}
			} else
				writeerrors = 0;

			do {
				gettimeofday(&now, NULL);
			} while (now.tv_sec < nextpkt.tv_sec ||
				 (now.tv_sec == nextpkt.tv_sec &&
				  now.tv_usec < nextpkt.tv_usec));

			nextpkt.tv_usec = now.tv_usec + pkt_delay;
			if (nextpkt.tv_usec > 1000000) {
				nextpkt.tv_sec += nextpkt.tv_usec / 1000000;
				nextpkt.tv_usec %= 1000000;
			}
#if 0
			/* Delay, if we are so far ahead of ourselves that we have at
			   least one tick to wait. */

			gettimeofday(&now, NULL);
			nextpkt.tv_usec = now.tv_usec + 
			/* Add to pkt_delay if necessary */
			delay_accum += pkt_delay;
			delay_accum -= (now.tv_usec - blkthen.tv_usec);
			delay_accum -= 1000000 * (now.tv_sec - blkthen.tv_sec);
			if (delay_accum < 0)
				delay_accum = 0;
			blkthen = now;
			
			if (delay_accum >= usec_per_tick) {
				usleep(delay_accum);

				delay_accum = 0;
			} printf("delay_accum %ld\n", delay_accum);
#endif
		}
	}
	munmap(image, st.st_size);
	close(rfd);
	close(sock);
	gettimeofday(&now, NULL);

	time_msecs = (now.tv_sec - then.tv_sec) * 1000;
	time_msecs += ((int)(now.tv_usec - then.tv_usec)) / 1000;
	printf("\n%d KiB sent in %ldms (%ld KiB/s)\n",
	       nr_blocks * (erasesize+sizeof(pktbuf)) / 1024, time_msecs,
	       nr_blocks * (erasesize+sizeof(pktbuf)) / 1024 * 1000 / time_msecs);
	return 0;
}
