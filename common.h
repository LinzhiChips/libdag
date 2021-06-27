/*
 * common.h - Common definitions and helper function
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef LIBDAG_COMMON_H
#define	LIBDAG_COMMON_H

#include <stdint.h>


#define	WORD_BYTES		4
#define	DATASET_BYTES_INIT	(1 << 30)
#define	DATASET_BYTES_GROWTH	(1 << 23)
#define	CACHE_BYTES_INIT	(1 << 24)
#define	CACHE_BYTES_GROWTH	(1 << 17)
#define	EPOCH_LENGTH		30000
#define	DATASET_PARENTS		256
#define	CACHE_ROUNDS		3
#define	ACCESSES		64


/* ----- Helper functions -------------------------------------------------- */


/*
 * Word access functions that take care of endianness. Since we're always
 * little-endian, they don't do much.
 */

static inline uint32_t read32(const uint8_t *p)
{
	return *(const uint32_t *) p;
}


static inline void write32(uint8_t *p, uint32_t v)
{
	*(uint32_t *) p = v;
}


static inline void write64(uint8_t *p, uint64_t v)
{
	*(uint64_t *) p = v;
}


/* ----- Data aggregation function ----------------------------------------- */


#define	FNV_PRIME	0x01000193


static inline uint32_t fnv(uint32_t v1, uint32_t v2)
{
	return (v1 * FNV_PRIME) ^ v2;
}


#endif /* !LIBDAG_COMMON_H */
