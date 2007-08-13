
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
	struct addrinfo *ai;
	struct addrinfo hints;
	struct addrinfo *runp;
	int ret;
	int sock;
	size_t len;
	int flfd;
	struct mtd_info_user meminfo;
	unsigned char *eb_buf;
	unsigned char *blockmap = NULL;
	int nr_blocks = 0;
	int *pkt_indices;
	unsigned char **pkts;
	int nr_pkts = 0;
	int pkts_per_block;
	int block_nr = -1;
	uint32_t image_crc;
	uint32_t blocks_received = 0;
	loff_t mtdoffset = 0;
	int *stats;
	int badcrcs = 0;
	int duplicates = 0;
	int file_mode = 0;
	struct fec_parms *fec;
	int i;

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

	eb_buf = malloc(pkts_per_block * PKT_SIZE);
	if (!eb_buf) {
		fprintf(stderr, "No memory for eraseblock buffer\n");
		exit(1);
	}

	pkt_indices = malloc(sizeof(int) * pkts_per_block);
	if (!pkt_indices) {
		fprintf(stderr, "No memory for packet indices\n");
		exit(1);
	}
	memset(pkt_indices, 0, sizeof(int) * pkts_per_block);

	pkts = malloc(sizeof(unsigned char *) * pkts_per_block);
	if (!pkts) {
		fprintf(stderr, "No memory for packet pointers\n");
		exit(1);
	}
	for (i=0; i<pkts_per_block; i++) {
		pkts[i] = malloc(sizeof(struct image_pkt_hdr) + PKT_SIZE);
		if (!pkts[i]) {
			printf("No memory for packets\n");
			exit(1);
		}
		pkts[i] += sizeof(struct image_pkt_hdr);
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
		}
		if (bind(sock, runp->ai_addr, runp->ai_addrlen)) {
			perror("bind");
			close(sock);
			continue;
		}
		break;
	}
	if (!runp)
		exit(1);

	while (1) {
		struct image_pkt *thispkt;

		if (nr_pkts < pkts_per_block)
			thispkt = (void *)(pkts[nr_pkts] - sizeof(struct image_pkt_hdr));
		else
			thispkt = (void *)(pkts[0] - sizeof(struct image_pkt_hdr));

		len = read(sock, thispkt, sizeof(*thispkt));

		if (len < 0) {
			perror("read socket");
			break;
		}
		if (len < sizeof(*thispkt)) {
			fprintf(stderr, "Wrong length %d bytes (expected %d)\n",
				len, sizeof(*thispkt));
			continue;
		}
		if (!blockmap) {
			image_crc = thispkt->hdr.totcrc;
			if (meminfo.erasesize != ntohl(thispkt->hdr.blocksize)) {
				fprintf(stderr, "Erasesize mismatch (0x%x not 0x%x)\n",
					ntohl(thispkt->hdr.blocksize), meminfo.erasesize);
				exit(1);
			}
			nr_blocks = ntohl(thispkt->hdr.nr_blocks);
			nr_pkts = 0;

			fec = fec_new(pkts_per_block, ntohs(thispkt->hdr.nr_pkts));

			blockmap = malloc(nr_blocks);
			if (!blockmap) {
				fprintf(stderr, "No memory for block map\n");
				exit(1);
			}
			memset(blockmap, 0, nr_blocks);
			stats = malloc(sizeof(int) * (ntohs(thispkt->hdr.nr_pkts) + 1));
			if (!stats) {
				fprintf(stderr, "No memory for statistics\n");
				exit(1);
			}
			memset(stats, 0, sizeof(int) * (ntohs(thispkt->hdr.nr_pkts) + 1));
		}
		if (image_crc != thispkt->hdr.totcrc) {
			fprintf(stderr, "Image CRC changed from 0x%x to 0x%x. Aborting\n",
				ntohl(image_crc), ntohl(thispkt->hdr.totcrc));
			exit(1);
		}
		if (ntohl(thispkt->hdr.block_nr) != block_nr) {
			/* Hm, new block */
			if (block_nr != -1) {
				if (!blockmap[block_nr]) {
					printf("Lost image block %08x with only %d/%d (%d) packets\n",
					       block_nr * meminfo.erasesize, nr_pkts, pkts_per_block,
					       ntohs(thispkt->hdr.nr_pkts));
				}
				if (blockmap[block_nr] < 2) {
					stats[nr_pkts]++;
					if (blockmap[block_nr]) {
						if (file_mode)
							printf(" with %d/%d (%d) packets\n",
							       nr_pkts, pkts_per_block,
							       ntohs(thispkt->hdr.nr_pkts));
						blockmap[block_nr] = 2;
					}
				}
			}
			/* Put this packet first */
			if (nr_pkts != 0 && nr_pkts < pkts_per_block) {
				unsigned char *tmp = pkts[0];
				pkts[0] = pkts[nr_pkts];
				pkts[nr_pkts] = tmp;

			}
			nr_pkts = 0;

			block_nr = ntohl(thispkt->hdr.block_nr);

			if (block_nr > nr_blocks) {
				fprintf(stderr, "Erroneous block_nr %d (> %d)\n",
					block_nr, nr_blocks);
				exit(1);
			}
			if (blockmap[block_nr]) {
				printf("Discard chunk at 0x%08x for already-flashed eraseblock (%d to go)\n",
				       block_nr * meminfo.erasesize, nr_blocks - blocks_received);
				continue;
			}
		}
		if (nr_pkts >= pkts_per_block) {
			/* We have a parity block but we didn't need it */
			nr_pkts++;
			continue;
		}
		if (blockmap[block_nr])
			continue;

		for (i=0; i < nr_pkts; i++) {
			if (pkt_indices[i] == ntohs(thispkt->hdr.pkt_nr)) {
				printf("Discarding duplicate packet at %08x pkt %d\n",
				       block_nr * meminfo.erasesize, pkt_indices[i]);
				duplicates++;
				break;
			}
		} /* And if we broke out, skip the packet... */
		if (i < nr_pkts)
			continue;

		if (crc32(-1, thispkt->data, PKT_SIZE) != ntohl(thispkt->hdr.thiscrc)) {
			printf("Discard %08x pkt %d with bad CRC (%08x not %08x)\n",
			       block_nr * meminfo.erasesize, ntohs(thispkt->hdr.pkt_nr),
			       crc32(-1, thispkt->data, PKT_SIZE),
			       ntohl(thispkt->hdr.thiscrc));
			badcrcs++;
			continue;
		}

		pkt_indices[nr_pkts] = ntohs(thispkt->hdr.pkt_nr);
		nr_pkts++;

		if (nr_pkts == pkts_per_block) {

			if (fec_decode(fec, pkts, pkt_indices, PKT_SIZE)) {
				/* Eep. This cannot happen */
				printf("The world is broken. fec_decode() returned error\n");
				exit(1);
			}
			blockmap[block_nr] = 1;
			blocks_received++;

			/* Put data into order in eb_buf */
			for (i=0; i < pkts_per_block; i++)
				memcpy(eb_buf + (i * PKT_SIZE), pkts[i], PKT_SIZE);

			if (crc32(-1, eb_buf, meminfo.erasesize) != ntohl(thispkt->hdr.block_crc)) {
				printf("FEC error. CRC %08x != %08x\n", 
				       crc32(-1, eb_buf, meminfo.erasesize),
				       ntohl(thispkt->hdr.block_crc));
				*(int *)0 = 0;
				exit(1);
			}
			if (file_mode) {
				printf("Received image block %08x (%d/%d)",
				       block_nr * meminfo.erasesize,
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
				printf("Wrote image block %08x (%d/%d) to flash offset %08x\n",
				       block_nr * meminfo.erasesize, 
				       blocks_received, nr_blocks,
				       (uint32_t)mtdoffset);
				mtdoffset += meminfo.erasesize;
			}
			if (!(blocks_received%100) || blocks_received == nr_blocks) {
				int i, printed = 0;
				printf("\n");
				for (i=0; i <= ntohs(thispkt->hdr.nr_pkts); i++) {
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
