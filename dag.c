/*
 * dag.c - DAG generation
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h> /* for intptr_t */
#include <math.h>
#include <assert.h>

#include "keccak.h"
#include "blake2.h"
#include "common.h"
#include "dagalgo.h"
#include "dag.h"


enum dag_algo dag_algo;


/* ----- Helper functions -------------------------------------------------- */


static bool isprime(unsigned x)
{
	unsigned i, last;

	if (x == 2)
		return 1;
	if (!(x & 1))
		return 0;
	last = sqrt(x);
	for (i = 3; i <= last; i += 2)
		if (!(x % i))
			return 0;
	return 1;
}


/* ----- Parameters -------------------------------------------------------- */


int get_epoch(unsigned block_number)
{
	switch (dag_algo) {
	case da_ethash:
	case da_ubqhash:
		return block_number / EPOCH_LENGTH;
	case da_etchash:
		return block_number / EPOCH_LENGTH / 2;
	default:
		abort();
	}
	return block_number / EPOCH_LENGTH;
}


unsigned get_cache_size(int epoch)
{
	unsigned sz = CACHE_BYTES_INIT + CACHE_BYTES_GROWTH * epoch;

	sz -= HASH_BYTES;
	while (!isprime(sz / HASH_BYTES))
		sz -= 2 * HASH_BYTES;
	return sz;
}


unsigned get_full_lines(int epoch)
{
	unsigned sz = DATASET_BYTES_INIT / DAG_LINE_BYTES +
	    DATASET_BYTES_GROWTH / DAG_LINE_BYTES * epoch;

	sz--;
	while (!isprime(sz))
		sz -= 2;
	return sz;
}


/* ----- Seedhash ---------------------------------------------------------- */


void get_seedhash(uint8_t *seed, unsigned epoch)
{
	unsigned rounds, i;

	switch (dag_algo) {
	case da_ethash:
	case da_ubqhash:
		rounds = epoch;
		break;
	case da_etchash:
		rounds = epoch * 2;
		break;
	default:
		abort();
	}

	memset(seed, 0, SEED_BYTES);
	for (i = 0; i != rounds; i++)
		KEC_256(seed, seed, SEED_BYTES);
}


/* ----- Cache generation -------------------------------------------------- */


void mkcache_init(uint8_t *cache, unsigned cache_bytes, const uint8_t *seed)
{
	unsigned n = cache_bytes / HASH_BYTES;
	uint8_t *p;

	assert(n);

	/* squentially produce the initial dataset */
	KEC_512(cache, seed, 32); 
	for (p = cache; p != cache + (n - 1) * HASH_BYTES; p += HASH_BYTES)
		KEC_512(p + HASH_BYTES, p, HASH_BYTES);
}


void mkcache_round(uint8_t *cache, unsigned cache_bytes)
{
	unsigned n = cache_bytes / HASH_BYTES;
	uint8_t *p;
	unsigned j, k;
	uint8_t tmp[HASH_BYTES];

	for (j = 0; j != n; j++) {
		p = cache + HASH_BYTES * j;

		uint32_t prev = (j + n - 1) % n;
		uint32_t v = read32(p) % n;

		for (k = 0; k != HASH_BYTES; k++)
			tmp[k] = cache[prev * HASH_BYTES + k] ^
			    cache[v * HASH_BYTES + k];
		KEC_512(p, tmp, HASH_BYTES);
	}

}


void mkcache(uint8_t *cache, unsigned cache_bytes, const uint8_t *seed)
{
	unsigned i;

	mkcache_init(cache, cache_bytes, seed);

	/* use a low-round version of randmemohash */
	for (i = 0; i != CACHE_ROUNDS; i++)
		mkcache_round(cache, cache_bytes);
}

/* ----- Cache generation ubqhash ------------------------------------------- */


void mkcache_init_ubqhash(uint8_t *cache, unsigned cache_bytes,
    const uint8_t *seed)
{
	unsigned n = cache_bytes / HASH_BYTES;
	uint8_t *p;

	assert(n);

	/* squentially produce the initial dataset */
	BLAKE2B_512(cache, seed, 32);
	for (p = cache; p != cache + (n - 1) * HASH_BYTES; p += HASH_BYTES)
		BLAKE2B_512(p + HASH_BYTES, p, HASH_BYTES);
}


void mkcache_round_ubqhash(uint8_t *cache, unsigned cache_bytes)
{
	unsigned n = cache_bytes / HASH_BYTES;
	uint8_t *p;
	unsigned j, k;
	uint8_t tmp[HASH_BYTES];

	for (j = 0; j != n; j++) {
		p = cache + HASH_BYTES * j;

		uint32_t prev = (j + n - 1) % n;
		uint32_t v = read32(p) % n;

		for (k = 0; k != HASH_BYTES; k++)
			tmp[k] = cache[prev * HASH_BYTES + k] ^
			    cache[v * HASH_BYTES + k];
		BLAKE2B_512(p, tmp, HASH_BYTES);
	}

}


void mkcache_ubqhash(uint8_t *cache, unsigned cache_bytes, const uint8_t *seed)
{
	unsigned i;

	mkcache_init_ubqhash(cache, cache_bytes, seed);

	/* use a low-round version of randmemohash */
	for (i = 0; i != CACHE_ROUNDS; i++)
		mkcache_round_ubqhash(cache, cache_bytes);
}


/* ----- Full dataset calculation ------------------------------------------ */


/*
 * "mix" must have a size of HASH_BYTES
 */

static void calc_dataset_item(uint8_t *mix, const uint8_t *cache,
    unsigned cache_bytes, unsigned i)
{
	unsigned n = cache_bytes / HASH_BYTES;
	unsigned r = HASH_BYTES / WORD_BYTES;
	unsigned j, k;

	assert(cache_bytes >= HASH_BYTES);

	/* initialize the mix */
	memcpy(mix, cache + HASH_BYTES * (i % n), HASH_BYTES);
	write32(mix, read32(mix) ^ i);
	KEC_512(mix, mix, HASH_BYTES);

	/* fnv it with a lot of random cache nodes based on i */
	for (j = 0; j != DATASET_PARENTS; j++) {
		unsigned cache_index =
		    fnv(i ^ j, read32(mix + 4 * (j % r))) % n;

		for (k = 0; k != HASH_BYTES / 4; k++)
			write32(mix + k * 4, fnv(read32(mix + k * 4),
			    read32(cache + cache_index * HASH_BYTES + k * 4)));
	}
	KEC_512(mix, mix, HASH_BYTES);
}


void calc_dataset_range(uint8_t *dag, unsigned start, unsigned lines,
    const uint8_t *cache, unsigned cache_bytes)
{
	unsigned i;

	for (i = 0; i != 2 * lines; i++)
		calc_dataset_item(dag + (intptr_t) i * HASH_BYTES,
		    cache, cache_bytes, 2 * start + i);
}


void calc_dataset(uint8_t *dag, unsigned full_lines,
    const uint8_t *cache, unsigned cache_bytes)
{
	calc_dataset_range(dag, 0, full_lines, cache, cache_bytes);
}
