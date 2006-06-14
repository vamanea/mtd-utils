/*
 * NAND ecc functions
 */
#ifndef _NAND_ECC_H
#define _NAND_ECC_H

#include <stdint.h>

extern int nand_calculate_ecc(const uint8_t *dat, uint8_t *ecc_code);

#endif
