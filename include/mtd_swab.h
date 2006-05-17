#ifndef MTD_SWAB_H
#define MTD_SWAB_H

#include <endian.h>

#define swab16(x) \
        ((uint16_t)( \
                (((uint16_t)(x) & (uint16_t)0x00ffU) << 8) | \
                (((uint16_t)(x) & (uint16_t)0xff00U) >> 8) ))
#define swab32(x) \
        ((uint32_t)( \
                (((uint32_t)(x) & (__u32)0x000000ffUL) << 24) | \
                (((uint32_t)(x) & (__u32)0x0000ff00UL) <<  8) | \
                (((uint32_t)(x) & (__u32)0x00ff0000UL) >>  8) | \
                (((uint32_t)(x) & (__u32)0xff000000UL) >> 24) ))

#if __BYTE_ORDER == __BIG_ENDIAN
#define cpu_to_le16(x) ({ uint16_t _x = x; swab16(_x); })
#define cpu_to_le32(x) ({ uint32_t _x = x; swab32(_x); })
#define cpu_to_be16(x) (x)
#define cpu_to_be32(x) (x)
#else
#define cpu_to_le16(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_be16(x) ({ uint16_t _x = x; swab16(_x); })
#define cpu_to_be32(x) ({ uint32_t _x = x; swab32(_x); })
#endif
#define le16_to_cpu(x) cpu_to_le16(x)
#define be16_to_cpu(x) cpu_to_be16(x)
#define le32_to_cpu(x) cpu_to_le32(x)
#define be32_to_cpu(x) cpu_to_be32(x)

#endif
