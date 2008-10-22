/*
 * This code was taken from the linux kernel. The license is GPL Version 2.
 */

#ifndef __CRC32_H__
#define __CRC32_H__

#include <stdint.h>

extern const uint32_t crc32_table[256];

/* Return a 32-bit CRC of the contents of the buffer. */
static inline uint32_t ubifs_crc32(uint32_t val, const void *ss, int len)
{
	const unsigned char *s = ss;

	while (--len >= 0)
		val = crc32_table[(val ^ *s++) & 0xff] ^ (val >> 8);
	return val;
}

#endif /* __CRC32_H__ */
