#define _POSIX_C_SOURCE 199309

#include <time.h>

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
int pkt_delay;

#undef RANDOMDROP

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
	unsigned char *image, *blockptr;
	uint32_t block_nr;
	int nr_blocks;
	struct timeval then, now, nextpkt;
	long time_msecs;
	unsigned char **src_pkts;
	unsigned char last_src_pkt[PKT_SIZE];
	int pkts_extra = 6;
	int pkts_per_block;
	struct fec_parms *fec;

	if (argc == 7) {
		tx_rate = atol(argv[6]) * 1024;
		if (tx_rate < PKT_SIZE || tx_rate > 20000000) {
			fprintf(stderr, "Bogus TX rate %d KiB/s\n", tx_rate);
			exit(1);
		}
		argc = 6;
	}
	if (argc == 6) {
		pkts_extra = atol(argv[5]);
		if (pkts_extra < 0 || pkts_extra > 200) {
			fprintf(stderr, "Bogus redundancy %d packets\n", pkts_extra);
			exit(1);
		}
		argc = 5;
	}
	       
	if (argc != 5) {
		fprintf(stderr, "usage: %s <host> <port> <image> <erasesize> [<redundancy>] [<tx_rate>]\n",
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

	pkts_per_block = (erasesize + PKT_SIZE - 1) / PKT_SIZE;
	src_pkts = malloc(pkts_per_block * sizeof(unsigned char *));
	if (!src_pkts) {
		fprintf(stderr, "Failed to allocate memory for packet pointers\n");
		exit(1);
	}
	/* We have to pad it with zeroes, so can't use it in-place */
	src_pkts[pkts_per_block-1] = last_src_pkt;

	fec = fec_new(pkts_per_block, pkts_per_block + pkts_extra);
	if (!fec) {
		fprintf(stderr, "Error initialising FEC\n");
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
	fflush(stdout);

	pktbuf.hdr.resend = 0;
	pktbuf.hdr.totcrc = htonl(crc32(-1, image, st.st_size));
	pktbuf.hdr.nr_blocks = htonl(nr_blocks);
	pktbuf.hdr.blocksize = htonl(erasesize);
	pktbuf.hdr.thislen = htonl(PKT_SIZE);
	pktbuf.hdr.nr_pkts = htons(pkts_per_block + pkts_extra);

	printf("%08x\n", ntohl(pktbuf.hdr.totcrc));

 again:
	printf("Image size %ld KiB (%08lx). %d redundant packets per block (%d total)\n"
	       "Data to send %d KiB. Estimated transmit time: %ds\n", 
	       (long)st.st_size / 1024, (long) st.st_size, pkts_extra, pkts_extra+pkts_per_block,
	       nr_blocks * PKT_SIZE * (pkts_per_block+pkts_extra) / 1024,
	       nr_blocks * (pkts_per_block+pkts_extra) * pkt_delay / 1000000);
	gettimeofday(&then, NULL);
	nextpkt = then;

#ifdef RANDOMDROP
	srand((unsigned)then.tv_usec);
	printf("Random seed %u\n", (unsigned)then.tv_usec);
#endif
	blockptr = image;

	for (block_nr = 0; block_nr < nr_blocks; block_nr++) {
		int i;
		long tosleep;

		blockptr = image + (erasesize * block_nr);

		pktbuf.hdr.block_crc = htonl(crc32(-1, blockptr, erasesize));

		for (i=0; i < pkts_per_block-1; i++)
			src_pkts[i] = blockptr + (i*PKT_SIZE);

		memcpy(last_src_pkt, blockptr + (i*PKT_SIZE),
		       erasesize - (i * PKT_SIZE));

		pktbuf.hdr.block_nr = htonl(block_nr);

		for (i=0; i < pkts_per_block + pkts_extra; i++) {

			fec_encode(fec, src_pkts, pktbuf.data, i, PKT_SIZE);
			
			printf("\rSending data block %08x packet %3d/%d",
			       block_nr * erasesize, i, pkts_per_block + pkts_extra);

			if (block_nr && !i) {
				gettimeofday(&now, NULL);

				time_msecs = (now.tv_sec - then.tv_sec) * 1000;
				time_msecs += ((int)(now.tv_usec - then.tv_usec)) / 1000;
				printf("    (%ld KiB/s)    ",
				       (block_nr * sizeof(pktbuf) * (pkts_per_block+pkts_extra))
				       / 1024 * 1000 / time_msecs);
			}

			fflush(stdout);
			pktbuf.hdr.pkt_nr = htons(i);
			pktbuf.hdr.thiscrc = htonl(crc32(-1, pktbuf.data, PKT_SIZE));

#ifdef RANDOMDROP
			if ((rand() % 1000) < 20) {
				printf("\nDropping packet %d\n", i+1);
				continue;
			}
#endif
			gettimeofday(&now, NULL);
#if 1
			tosleep = nextpkt.tv_usec - now.tv_usec + 
				(1000000 * (nextpkt.tv_sec - now.tv_sec));

			/* We need hrtimers for this to actually work */
			if (tosleep > 0) {
				struct timespec req;

				req.tv_nsec = (tosleep % 1000000) * 1000;
				req.tv_sec = tosleep / 1000000;

				nanosleep(&req, NULL);
			}
#else
			while (now.tv_sec < nextpkt.tv_sec ||
				 (now.tv_sec == nextpkt.tv_sec &&
				  now.tv_usec < nextpkt.tv_usec)) {
				gettimeofday(&now, NULL);
			}
#endif
			nextpkt.tv_usec += pkt_delay;
			if (nextpkt.tv_usec >= 1000000) {
				nextpkt.tv_sec += nextpkt.tv_usec / 1000000;
				nextpkt.tv_usec %= 1000000;
			}

			/* If the time for the next packet has already
			   passed, then we've lost time. Adjust our expected
			   timings accordingly. */
			if (now.tv_usec > (now.tv_usec + 
					   1000000 * (nextpkt.tv_sec - now.tv_sec))) { 
				nextpkt = now;
			}

			if (write(sock, &pktbuf, sizeof(pktbuf)) < 0) {
				perror("write");
				writeerrors++;
				if (writeerrors > 10) {
					fprintf(stderr, "Too many consecutive write errors\n");
					exit(1);
				}
			} else
				writeerrors = 0;


			
		}
	}
	gettimeofday(&now, NULL);

	time_msecs = (now.tv_sec - then.tv_sec) * 1000;
	time_msecs += ((int)(now.tv_usec - then.tv_usec)) / 1000;
	printf("\n%d KiB sent in %ldms (%ld KiB/s)\n",
	       nr_blocks * sizeof(pktbuf) * (pkts_per_block+pkts_extra) / 1024, time_msecs,
	       nr_blocks * sizeof(pktbuf) * (pkts_per_block+pkts_extra) / 1024 * 1000 / time_msecs);

	munmap(image, st.st_size);
	close(rfd);
	close(sock);
	return 0;
}
