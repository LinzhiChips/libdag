/*
 * mine.h - Ethash-familiy calculations, for development and verification
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef LIBDAG_MINE_H
#define	LIBDAG_MINE_H

/*
 * Based on
 * https://github.com/ethereum/wiki/wiki/Ethash
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dagio.h"


#define	HEADER_HASH_BYTES	32
#define	NONCE_BYTES		8
#define	CMIX_BYTES		32
#define	RESULT_BYTES		32
#define	TARGET_BYTES		RESULT_BYTES


extern FILE *mine_trace;
extern bool mine_trace_linear;


void mix_setup(uint8_t *mix, uint8_t *s, const uint8_t *header_hash,
    uint64_t nonce);
uint32_t mix_dag_line(unsigned round0, const uint8_t *mix, const uint8_t *s,
    unsigned full_lines);
void mix_do_mix(uint8_t *mix, const uint8_t *this_dag_line);
void mix_finish(uint8_t *cmix, uint8_t *result, const uint8_t *mix,
    const uint8_t *s);

void hashimoto(uint8_t *cmix, uint8_t *result, const uint8_t *header_hash,
    uint64_t nonce, const uint8_t *dag, unsigned full_lines);
void hashimoto_fd(uint8_t *cmix, uint8_t *result, const uint8_t *header_hash,
    uint64_t nonce, int dag_fd, unsigned full_lines);
void hashimoto_dh(uint8_t *cmix, uint8_t *result, const uint8_t *header_hash,
    uint64_t nonce, struct dag_handle *dh, unsigned full_lines);
void hashimoto_light(uint8_t *cmix, uint8_t *result, const uint8_t *header_hash,
    uint64_t nonce, const uint8_t *cache, unsigned cache_bytes,
    unsigned full_lines);

/*
 * Lowest 64 bits of difficulty are in difficulty[0], highest in [3]
 */

void get_target(uint8_t *target, const uint64_t difficulty[4]);
bool below_target(const uint8_t *result, const uint8_t *target);

#endif /* !LIBDAG_MINE_H */
