/*
 * dag.h - DAG generation
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef LIBDAG_DAG_H
#define	LIBDAG_DAG_H

/*
 * Based on
 * https://github.com/ethereum/wiki/wiki/Ethash
 */

#include <stdbool.h>
#include <stdint.h>

#include "dagalgo.h"


#define	SEED_BYTES		32
#define HASH_BYTES		64
#define MIX_BYTES		128

#define	CACHE_LINE_BYTES	HASH_BYTES
#define	DAG_LINE_BYTES		MIX_BYTES


extern enum dag_algo dag_algo;


int get_epoch(unsigned block_number);

unsigned get_cache_size(int epoch);
unsigned get_full_lines(int epoch);

void get_seedhash(uint8_t *seed, unsigned epoch);

void mkcache_init(uint8_t *cache, unsigned cache_bytes, const uint8_t *seed);
void mkcache_round(uint8_t *cache, unsigned cache_bytes);
void mkcache(uint8_t *cache, unsigned cache_bytes, const uint8_t *seed);

void calc_dataset_range(uint8_t *dag, unsigned start, unsigned lines,
    const uint8_t *cache, unsigned cache_bytes);
void calc_dataset(uint8_t *dag, unsigned full_lines,
    const uint8_t *cache, unsigned cache_bytes);

#endif /* !LIBDAG_DAG_H */
