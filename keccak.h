/*
 * keccak.h - A single-file implementation of SHA-3 and SHAKE
 *
 * Interface to libkeccak-tiny,
 * Implementor: David Leon Gil
 * License: CC0, attribution kindly requested. Blame taken too,
 * but not liability.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

#define decsha3(bits) \
	int sha3_##bits(uint8_t*, size_t, uint8_t const*, size_t);

decsha3(256)
decsha3(512)

static inline void KEC_256(uint8_t *ret, uint8_t const *data,
    size_t const size)
{
	sha3_256(ret, 32, data, size);
}

static inline void KEC_512(uint8_t *ret, uint8_t const *data, size_t const size)
{
	sha3_512(ret, 64, data, size);
}

#ifdef __cplusplus
}
#endif
