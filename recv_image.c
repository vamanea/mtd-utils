
#define _XOPEN_SOURCE 500

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
#include <netinet/in.h>
#include <sys/ioctl.h>
#include "crc32.h"
#include "mtd/mtd-user.h"
#include "mcast_image.h"

#define min(x,y) (  (x)>(y)?(y):(x) )

int main(int argc, char **argv)
{
	struct sockaddr_storage server_addr;
	socklen_t server_addrlen = sizeof(server_addr);
	struct addrinfo *ai;
	struct addrinfo hints;
	struct addrinfo *runp;
	int ret;
	int sock;
	struct image_pkt pktbuf;
	size_t len;
	int flfd;
	struct mtd_info_user meminfo;
	unsigned char *eb_buf;
	unsigned char *blockmap = NULL;
	unsigned char *subblockmap;
	int nr_blocks = 0;
	int nr_subblocks = 0;
	int pkts_per_block;
	int block_nr = -1;
	uint32_t image_crc;
	uint32_t blocks_received = 0;
	uint32_t block_ofs;
	loff_t mtdoffset = 0;
	int *stats;
	int badcrcs = 0;
	int duplicates = 0;
	int missing = -1;
	int file_mode = 0;

	if (argc != 4) {
		fprintf(stderr, "usage: %s <host> <port> <mtddev>\n",
			(strrchr(argv[0], '/')?:argv[0]-1)+1);
		exit(1);
	}
	/* Open the device */
	flfd = open(argv[3], O_RDWR);

	if (flfd >= 0) {
		/* Fill in MTD device capability structure */
		if (ioctl(flfd, MEMGETINFO, &meminfo) != 0) {
			perror("MEMGETINFO");
			close(flfd);
			flfd = -1;
		} else {
			printf("Receive to MTD device %s with erasesize %d\n",
			       argv[3], meminfo.erasesize);
		}
	}
	if (flfd == -1) {
		/* Try again, as if it's a file */
		flfd = open(argv[3], O_CREAT|O_TRUNC|O_WRONLY, 0644);
		if (flfd < 0) {
			perror("open");
			exit(1);
		}
		meminfo.erasesize = 131072;
		file_mode = 1;
		printf("Receive to file %s with (assumed) erasesize %d\n",
		       argv[3], meminfo.erasesize);
	}

	pkts_per_block = (meminfo.erasesize + PKT_SIZE - 1) / PKT_SIZE;

	stats = malloc(pkts_per_block + 1);
	if (!stats) {
		fprintf(stderr, "No memory for statistics\n");
		exit(1);
	}
	memset(stats, 0, sizeof(int) * (pkts_per_block + 1));

	eb_buf = malloc(pkts_per_block * PKT_SIZE);
	if (!eb_buf) {
		fprintf(stderr, "No memory for eraseblock buffer\n");
		exit(1);
	}
	memset(eb_buf, 0, pkts_per_block * PKT_SIZE);

	subblockmap = malloc(pkts_per_block + 1);
	if (!subblockmap) {
		fprintf(stderr, "No memory for subblock map\n");
		exit(1);
	}
	memset(subblockmap, 0, pkts_per_block + 1);

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
		if (runp->ai_family == AF_INET &&
		    IN_MULTICAST( ntohl(((struct sockaddr_in *)runp->ai_addr)->sin_addr.s_addr))) {
			struct ip_mreq rq;
			rq.imr_multiaddr = ((struct sockaddr_in *)runp->ai_addr)->sin_addr;
			rq.imr_interface.s_addr = INADDR_ANY;
			if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &rq, sizeof(rq))) {
				perror("IP_ADD_MEMBERSHIP"); 
				close(sock);
				continue;
			}
			
		} else if (runp->ai_family == AF_INET6 &&
			   ((struct sockaddr_in6 *)runp->ai_addr)->sin6_addr.s6_addr[0] == 0xff) {
			struct ipv6_mreq rq;
			rq.ipv6mr_multiaddr =  ((struct sockaddr_in6 *)runp->ai_addr)->sin6_addr;
			rq.ipv6mr_interface = 0;
			if (setsockopt(sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &rq, sizeof(rq))) {
				perror("IPV6_ADD_MEMBERSHIP"); 
				close(sock);
				continue;
			}
		} else printf("not multicast?\n");
		if (bind(sock, runp->ai_addr, runp->ai_addrlen)) {
			perror("bind");
			close(sock);
			continue;
		}
		break;
	}
	if (!runp)
		exit(1);

	while ((len = read(sock, &pktbuf, sizeof(pktbuf))) >= 0) {
		if (len < sizeof(pktbuf.hdr)) {
			fprintf(stderr, "Short read %d bytes\n", len);
			continue;
		}
		if (len != sizeof(pktbuf.hdr) + ntohl(pktbuf.hdr.thislen)) {
			fprintf(stderr, "Wrong length %d bytes (expected %d+%d)\n",
				len, sizeof(pktbuf.hdr), ntohl(pktbuf.hdr.thislen));
			continue;
		}
		/* Holds _data_ length */
		len -= sizeof(pktbuf.hdr);
			
		if (!blockmap) {
			image_crc = pktbuf.hdr.totcrc;
			if (meminfo.erasesize != ntohl(pktbuf.hdr.blocksize)) {
				fprintf(stderr, "Erasesize mismatch (0x%x not 0x%x)\n",
					ntohl(pktbuf.hdr.blocksize), meminfo.erasesize);
				exit(1);
			}
			nr_blocks = ntohl(pktbuf.hdr.nr_blocks);
			nr_subblocks = pkts_per_block + 2;
			blockmap = malloc(nr_blocks);
			if (!blockmap) {
				fprintf(stderr, "No memory for block map\n");
				exit(1);
			}
			memset(blockmap, 0, nr_blocks);
		}
		if (image_crc != pktbuf.hdr.totcrc) {
			fprintf(stderr, "Image CRC changed from 0x%x to 0x%x. Aborting\n",
				ntohl(image_crc), ntohl(pktbuf.hdr.totcrc));
			exit(1);
		}
		if (ntohl(pktbuf.hdr.block_nr) != block_nr) {
			/* Hm, new block */
			if (nr_subblocks < pkts_per_block &&
			    block_nr != -1) 
				printf("Lost image block at %08x with only %d/%d packets\n",
				       block_nr * meminfo.erasesize, nr_subblocks,
				       pkts_per_block + 1);


			if (nr_subblocks < pkts_per_block + 2)
				stats[nr_subblocks]++;

			nr_subblocks = 0;
			missing = -1;
			memset(subblockmap, 0, pkts_per_block + 1);
			block_nr = ntohl(pktbuf.hdr.block_nr);
			if (block_nr > nr_blocks) {
				fprintf(stderr, "Erroneous block_nr %d (> %d)\n",
					block_nr, nr_blocks);
				exit(1);
			}
			if (blockmap[block_nr]) {
				printf("Discard chunk at 0x%08x for already-flashed eraseblock (%d to go)\n",
				       block_nr * meminfo.erasesize, nr_blocks - blocks_received);
				nr_subblocks = pkts_per_block + 2;
				continue;
			}
		}
		if (nr_subblocks == pkts_per_block) {
			/* We have a parity block but we didn't need it */
			nr_subblocks++;
			continue;
		}
		if (blockmap[block_nr])
			continue;

		block_ofs = ntohl(pktbuf.hdr.block_ofs);
		if (block_ofs == meminfo.erasesize)
			block_ofs = PKT_SIZE * pkts_per_block;

		if (len != PKT_SIZE && len + block_ofs != meminfo.erasesize) {
			fprintf(stderr, "Bogus packet size 0x%x (expected 0x%x)\n",
				ntohl(pktbuf.hdr.thislen),
				min(PKT_SIZE, meminfo.erasesize - block_ofs));
			exit(1);
		}

		if (crc32(-1, pktbuf.data, len) != ntohl(pktbuf.hdr.thiscrc)) {
			printf("Discard chunk %08x with bad CRC (%08x not %08x)\n",
			       block_nr * meminfo.erasesize + block_ofs,
			       crc32(-1, pktbuf.data, pktbuf.hdr.thislen),
			       ntohl(pktbuf.hdr.thiscrc));
			badcrcs++;
			continue;
		}
		if (subblockmap[block_ofs / PKT_SIZE]) {
			printf("Discarding duplicate packet at %08x\n",
			       block_nr * meminfo.erasesize + block_ofs);
			duplicates++;
			continue;
		}
		subblockmap[block_ofs / PKT_SIZE] = 1;
		nr_subblocks++;
		if (block_ofs < meminfo.erasesize) {
			/* Normal data packet */
			memcpy(eb_buf + block_ofs, pktbuf.data, len);
//			printf("Received data block at %08x\n", block_nr * meminfo.erasesize + block_ofs);
		} else {
			/* Parity block */
			int i;

			/* If we don't have enough to recover, skip */
			if (nr_subblocks < pkts_per_block)
				continue;

			for (i = 0; i<pkts_per_block; i++) {
				if (subblockmap[i]) {
					int j;
					for (j=0; j<PKT_SIZE; j++)
						pktbuf.data[j] ^= eb_buf[i*PKT_SIZE + j];
				} else
					missing = i;
			}

			if (missing == -1) {
				fprintf(stderr, "dwmw2 is stupid\n");
				exit(1);
			}
//			printf("Recover missing packet at %08x from parity\n",
//			       block_nr * meminfo.erasesize + missing * PKT_SIZE);
			memcpy(eb_buf + (missing * PKT_SIZE), pktbuf.data, PKT_SIZE);
		}

		if (nr_subblocks == pkts_per_block) {

			blockmap[block_nr] = 1;
			blocks_received++;

			if (file_mode) {
				printf("Received image block %08x%s (%d/%d)\n",
				       block_nr * meminfo.erasesize,
				       (missing==-1)?"":" (parity)",
				       blocks_received, nr_blocks);
				pwrite(flfd, eb_buf, meminfo.erasesize, block_nr * meminfo.erasesize);
			} else {
				ssize_t wrotelen;
			again:
				if (mtdoffset >= meminfo.size) {
					fprintf(stderr, "Run out of space on flash\n");
					exit(1);
				}
				while (ioctl(flfd, MEMGETBADBLOCK, &mtdoffset) > 0) {
					printf("Skipping flash bad block at %08x\n", (uint32_t)mtdoffset);
					mtdoffset += meminfo.erasesize;
				}
				wrotelen = pwrite(flfd, eb_buf, meminfo.erasesize, mtdoffset);
				if (wrotelen != meminfo.erasesize) {
					struct erase_info_user erase;
					
					if (wrotelen < 0)
						perror("flash write");
					else
						fprintf(stderr, "Short write to flash at %08x: %zd bytes\n",
							(uint32_t)mtdoffset, wrotelen);
					
					erase.start = mtdoffset;
					erase.length = meminfo.erasesize;
					
					if (ioctl(flfd, MEMERASE, erase)) {
						perror("MEMERASE");
						exit(1);
					}
					/* skip it */
					//				ioctl(flfd, MEMSETBADBLOCK, &mtdoffset);
					mtdoffset += meminfo.erasesize;
					goto again;
				}
				printf("Wrote image block %08x (%d/%d) to flash offset %08x%s\n",
				       block_nr * meminfo.erasesize, 
				       blocks_received, nr_blocks,
				       (uint32_t)mtdoffset,
				       (missing==-1)?"":" (parity)");
				mtdoffset += meminfo.erasesize;
			}
			if (!(blocks_received%100) || blocks_received == nr_blocks) {
				int i, printed = 0;
				for (i=0; i <= pkts_per_block + 1; i++) {
					if (printed || stats[i]) {
						printf("Number of blocks with %d packets received: %d\n",
						       i, stats[i]);
						printed = 1;
					}
				}
				printf("Bad CRC: %d\n", badcrcs);
				printf("Duplicate: %d\n", duplicates);

			}
			if (blocks_received == nr_blocks) {
				printf("Got all %08x bytes of image. Bye!\n",
				       nr_blocks * meminfo.erasesize);
				exit(0);
			}
		}
	}
	close(sock);
}
