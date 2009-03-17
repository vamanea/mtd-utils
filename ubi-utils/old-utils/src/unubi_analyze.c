/*
 * Copyright (c) International Business Machines Corp., 2006, 2007
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Authors: Drake Dowsett, dowsett@de.ibm.com
 * Contact: Andreas Arnez, arnez@de.ibm.com
 *
 * unubi uses the following functions to generate analysis output based on
 * the header information in a raw-UBI image
 */

/*
 * TODO: use OOB data to check for eraseblock validity in NAND images
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mtd_swab.h>

#include "unubi_analyze.h"
#include "crc32.h"

#define EC_X_INT	50

/**
 * intcmp - function needed by qsort to order integers
 **/
int intcmp(const void *a, const void *b)
{
	int A = *(int *)a;
	int B = *(int *)b;
	return A - B;
}

int longcmp(const void *a, const void *b)
{
	long long A = *(long long *)a;
	long long B = *(long long *)b;
	return A - B;
}


/**
 * unubi_analyze_group_index - finds the normalized index in an array
 * item:	look for this item in the array
 * array:	array to search through
 * size:	length of the array
 * array should be sorted for this algorithm to perform properly;
 * if the item is not found returns -1, otherwise return value is the
 * index in the array (note this contricts the array size to 2^32-1);
 **/
int
norm_index(uint32_t item, uint32_t *array, size_t length)
{
	size_t i, index;

	for (index = 0, i = 0; i < length; i++) {
		if ((i != 0) && (array[i] != array[i - 1]))
			index++;

		if (item == array[i])
			return index;
	}

	return -1;
}


/**
 * unubi_analyze_ec_hdr - generate data table and plot script
 * first:	head of simple linked list
 * path:	folder to write into
 * generates a data file containing the eraseblock index in the image
 * and the erase counter found in its ec header;
 * if the crc check fails, the line is commented out in the data file;
 * also generates a simple gnuplot sript for quickly viewing one
 * display of the data file;
 **/
int
unubi_analyze_ec_hdr(struct eb_info *first, const char *path)
{
	char filename[PATH_MAX + 1];
	size_t count, eraseblocks;
	uint32_t crc, crc32_table[256];
	uint64_t *erase_counts;
	FILE* fpdata;
	FILE* fpplot;
	struct eb_info *cur;

	if (first == NULL)
		return -1;

	/* crc check still needed for `first' linked list */
	init_crc32_table(crc32_table);

	/* prepare output files */
	memset(filename, 0, PATH_MAX + 1);
	snprintf(filename, PATH_MAX, "%s/%s", path, FN_EH_DATA);
	fpdata = fopen(filename, "w");
	if (fpdata == NULL)
		return -1;

	memset(filename, 0, PATH_MAX + 1);
	snprintf(filename, PATH_MAX, "%s/%s", path, FN_EH_PLOT);
	fpplot = fopen(filename, "w");
	if (fpplot == NULL) {
		fclose(fpdata);
		return -1;
	}

	/* make executable */
	chmod(filename, 0755);

	/* first run: count elements */
	count = 0;
	cur = first;
	while (cur != NULL) {
		cur = cur->next;
		count++;
	}
	eraseblocks = count;

	erase_counts = malloc(eraseblocks * sizeof(*erase_counts));
	if (!erase_counts) {
		perror("out of memory");
		exit(EXIT_FAILURE);
	}

	memset(erase_counts, 0, eraseblocks * sizeof(*erase_counts));

	/* second run: populate array to sort */
	count = 0;
	cur = first;
	while (cur != NULL) {
		erase_counts[count] = be64_to_cpu(cur->ec.ec);
		cur = cur->next;
		count++;
	}
	qsort(erase_counts, eraseblocks, sizeof(*erase_counts),
	      (void *)longcmp);

	/* third run: generate data file */
	count = 0;
	cur = first;
	fprintf(fpdata, "# eraseblock_no actual_erase_count "
			"sorted_erase_count\n");
	while (cur != NULL) {
		crc = clc_crc32(crc32_table, UBI_CRC32_INIT, &cur->ec,
				UBI_EC_HDR_SIZE_CRC);

		if ((be32_to_cpu(cur->ec.magic) != UBI_EC_HDR_MAGIC) ||
		    (crc != be32_to_cpu(cur->ec.hdr_crc)))
			fprintf(fpdata, "# ");

		fprintf(fpdata, "%zu %llu %llu", count,
			(unsigned long long)be64_to_cpu(cur->ec.ec),
			(unsigned long long)erase_counts[count]);

		if (be32_to_cpu(cur->ec.magic) != UBI_EC_HDR_MAGIC)
			fprintf(fpdata, " ## bad magic: %08x",
				be32_to_cpu(cur->ec.magic));

		if (crc != be32_to_cpu(cur->ec.hdr_crc))
			fprintf(fpdata, " ## CRC mismatch: given=%08x, "
				"calc=%08x", be32_to_cpu(cur->ec.hdr_crc),
				crc);

		fprintf(fpdata, "\n");

		cur = cur->next;
		count++;
	}
	fclose(fpdata);

	fprintf(fpplot, "#!/usr/bin/gnuplot -persist\n");
	fprintf(fpplot, "set xlabel \"eraseblock\"\n");

	/* fourth run: generate plot file xtics */
	count = 0;
	cur = first;
	fprintf(fpplot, "set xtics (");
	while (cur != NULL) {
		if ((count % EC_X_INT) == 0) {
			if (count > 0)
				fprintf(fpplot, ", ");
			fprintf(fpplot, "%zd", count);
		}

		cur = cur->next;
		count++;
	}
	fprintf(fpplot, ")\n");

	fprintf(fpplot, "set ylabel \"erase count\"\n");
	fprintf(fpplot, "set xrange [-1:%zu]\n", eraseblocks + 1);
	fprintf(fpplot, "# set yrange [-1:%llu]\n",
		(unsigned long long)erase_counts[eraseblocks - 1] + 1);
	fprintf(fpplot, "plot \"%s\" u 1:2 t \"unsorted: %s\" with boxes\n",
		FN_EH_DATA, FN_EH_DATA);
	fprintf(fpplot, "# replot \"%s\" u 1:3 t \"sorted: %s\" with lines\n",
		FN_EH_DATA, FN_EH_DATA);
	fprintf(fpplot, "pause -1 \"press ENTER\"\n");

	fclose(fpplot);

	return 0;
}


/**
 * unubi_analyze_vid_hdr - generate data table and plot script
 * head:	head of complex linked list (eb_chain)
 * path:	folder to write into
 * generates a data file containing the volume id, logical number, leb version,
 * and data size from the vid header;
 * all eraseblocks listed in the eb_chain are valid (checked in unubi);
 * also generates a simple gnuplot sript for quickly viewing one
 * display of the data file;
 **/
int
unubi_analyze_vid_hdr(struct eb_info **head, const char *path)
{
	char filename[PATH_MAX + 1];
	int rc, y1, y2;
	size_t count, step, breadth;
	uint32_t *leb_versions, *data_sizes;
	FILE* fpdata;
	FILE* fpplot;
	struct eb_info *cur;

	if (head == NULL || *head == NULL)
		return -1;

	rc = 0;
	fpdata = NULL;
	fpplot = NULL;
	data_sizes = NULL;
	leb_versions = NULL;

	/* prepare output files */
	memset(filename, 0, PATH_MAX + 1);
	snprintf(filename, PATH_MAX, "%s/%s", path, FN_VH_DATA);
	fpdata = fopen(filename, "w");
	if (fpdata == NULL) {
		rc = -1;
		goto exit;
	}

	memset(filename, 0, PATH_MAX + 1);
	snprintf(filename, PATH_MAX, "%s/%s", path, FN_VH_PLOT);
	fpplot = fopen(filename, "w");
	if (fpplot == NULL) {
		rc = -1;
		goto exit;
	}

	/* make executable */
	chmod(filename, 0755);

	/* first run: count elements */
	count = 0;
	cur = *head;
	while (cur != NULL) {
		cur = cur->next;
		count++;
	}
	breadth = count;

	leb_versions = malloc(breadth * sizeof(uint32_t));
	if (leb_versions == NULL) {
		rc = -1;
		goto exit;
	}
	memset(leb_versions, 0, breadth * sizeof(uint32_t));

	data_sizes = malloc(breadth * sizeof(uint32_t));
	if (data_sizes == NULL) {
		rc = -1;
		goto exit;
	}
	memset(data_sizes, 0, breadth * sizeof(*data_sizes));

	/* second run: populate arrays to sort */
	count = 0;
	cur = *head;
	while (cur != NULL) {
		leb_versions[count] = be32_to_cpu(cur->vid.leb_ver);
		data_sizes[count] = be32_to_cpu(cur->vid.data_size);
		cur = cur->next;
		count++;
	}
	qsort(leb_versions, breadth, sizeof(*leb_versions), (void *)intcmp);
	qsort(data_sizes, breadth, sizeof(*data_sizes), (void *)intcmp);

	/* third run: generate data file */
	count = 0;
	cur = *head;
	fprintf(fpdata, "# x_axis vol_id lnum   y1_axis leb_ver   "
		"y2_axis data_size\n");
	while (cur != NULL) {
		y1 = norm_index(be32_to_cpu(cur->vid.leb_ver), leb_versions,
				breadth);
		y2 = norm_index(be32_to_cpu(cur->vid.data_size), data_sizes,
				breadth);

		if ((y1 == -1) || (y2 == -1)) {
			rc = -1;
			goto exit;
		}

		fprintf(fpdata, "%zu %u %u   %u %u   %u %u\n",
			count,
			be32_to_cpu(cur->vid.vol_id),
			be32_to_cpu(cur->vid.lnum),
			y1,
			be32_to_cpu(cur->vid.leb_ver),
			y2,
			be32_to_cpu(cur->vid.data_size));
		cur = cur->next;
		count++;
	}

	fprintf(fpplot, "#!/usr/bin/gnuplot -persist\n");
	fprintf(fpplot, "set xlabel \"volume\"\n");

	/* fourth run: generate plot file xtics */
	count = 0;
	step = 0;
	cur = *head;
	fprintf(fpplot, "set xtics (");
	while (cur != NULL) {
		if (count > 0)
			fprintf(fpplot, ", ");
		if (step != be32_to_cpu(cur->vid.vol_id)) {
			step = be32_to_cpu(cur->vid.vol_id);
			fprintf(fpplot, "\"%zd\" %zd 0", step, count);
		}
		else
			fprintf(fpplot, "\"%d\" %zd 1",
				be32_to_cpu(cur->vid.lnum), count);
		cur = cur->next;
		count++;
	}
	fprintf(fpplot, ")\n");
	fprintf(fpplot, "set nox2tics\n");

	/* fifth run: generate plot file ytics */
	count = 0;
	cur = *head;
	fprintf(fpplot, "set ylabel \"leb version\"\n");
	fprintf(fpplot, "set ytics (");
	while (cur->next != NULL) {
		y1 = norm_index(be32_to_cpu(cur->vid.leb_ver), leb_versions,
				breadth);

		if (y1 == -1) {
			rc = -1;
			goto exit;
		}

		if (count > 0)
			fprintf(fpplot, ", ");

		fprintf(fpplot, "\"%u\" %u", be32_to_cpu(cur->vid.leb_ver),
			y1);

		cur = cur->next;
		count++;
	}
	fprintf(fpplot, ")\n");

	/* sixth run: generate plot file y2tics */
	count = 0;
	cur = *head;
	fprintf(fpplot, "set y2label \"data size\"\n");
	fprintf(fpplot, "set y2tics (");
	while (cur != NULL) {
		y2 = norm_index(be32_to_cpu(cur->vid.data_size),
				data_sizes, breadth);

		if (y2 == -1) {
			rc = -1;
			goto exit;
		}

		if (count > 0)
			fprintf(fpplot, ", ");

		fprintf(fpplot, "\"%u\" %u", be32_to_cpu(cur->vid.data_size),
			y2);

		cur = cur->next;
		count++;
	}
	fprintf(fpplot, ")\n");

	y1 = norm_index(leb_versions[breadth - 1], leb_versions, breadth);
	y2 = norm_index(data_sizes[breadth - 1], data_sizes, breadth);
	fprintf(fpplot, "set xrange [-1:%zu]\n", count + 1);
	fprintf(fpplot, "set yrange [-1:%u]\n", y1 + 1);
	fprintf(fpplot, "set y2range [-1:%u]\n", y2 + 1);
	fprintf(fpplot, "plot \"%s\" u 1:4 t \"leb version: %s\" "
			"axes x1y1 with lp\n", FN_VH_DATA, FN_VH_DATA);
	fprintf(fpplot, "replot \"%s\" u 1:6 t \"data size: %s\" "
			"axes x1y2 with lp\n", FN_VH_DATA, FN_VH_DATA);
	fprintf(fpplot, "pause -1 \"press ENTER\"\n");

 exit:
	if (fpdata != NULL)
		fclose(fpdata);
	if (fpplot != NULL)
		fclose(fpplot);
	if (data_sizes != NULL)
		free(data_sizes);
	if (leb_versions != NULL)
		free(leb_versions);

	return rc;
}


/**
 * unubi_analyze - run all analyses
 * head:	eb_chain head
 * first:	simple linked list of eraseblock headers (use .next)
 * path:	directory (without trailing slash) to output to
 * returns 0 upon successful completion, or -1 otherwise
 **/
int
unubi_analyze(struct eb_info **head, struct eb_info *first, const char *path)
{
	int ec_rc, vid_rc;

	if (path == NULL)
		return -1;

	ec_rc = unubi_analyze_ec_hdr(first, path);
	vid_rc = unubi_analyze_vid_hdr(head, path);
	if (ec_rc < 0 || vid_rc < 0)
		return -1;

	return 0;
}
