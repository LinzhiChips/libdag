/*
 * check.c - Run Ethash calculation (this is the messy ancestor of mixone)
 * 
 * Copyright (C) 2021 Linzhi Ltd.
 *   
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 *
 *
 * Reference example (a job received from ethermine.org):
 * ./check -v dag183 183 \
 *   0x892a2e92b8a050dff196e1a19efcb2a903655584913e719435c0ad2b53cfa7bd \
 *   0x46c089bc0ce5b456
 * The difficulty was "New pool difficulty:  4.0000 gigahashes"
 * The output we get:
 * Loaded 2608856192 bytes DAG
 * --- Header hash (32 bytes) ---
 * 89 2a 2e 92 b8 a0 50 df  f1 96 e1 a1 9e fc b2 a9
 * 03 65 55 84 91 3e 71 94  35 c0 ad 2b 53 cf a7 bd
 * --- Nonce (8 bytes) ---
 * 56 b4 e5 0c bc 89 c0 46
 * --- CMix (8 words) ---
 * 3510642988 2194159695 2124062449 2460317509
 * 4231393820 1567677794 3918344751 4225057859
 * --- CMix (32 bytes) ---
 * 2c 29 40 d1 4f 38 c8 82  f1 9e 9a 7e 45 77 a5 92
 * 1c f2 35 fc 62 dd 70 5d  2f 32 8d e9 43 44 d5 fb
 * --- Result (8 words) ---
 *          0 2127151796 3401956219  129989373
 * 1591517314 3351923581 1515291955 1194704386
 * --- Result (32 bytes) ---
 * 00 00 00 00 b4 c2 c9 7e  7b bb c5 ca fd 7a bf 07
 * 82 a0 dc 5e 7d 4b ca c7  33 85 51 5a 02 be 35 47
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <assert.h>

#include "dag.h"
#include "mdag.h"
#include "mine.h"
#include "keccak.h"

#include "util.h"


static bool verbose = 0;
static bool quiet = 0;
static bool stable = 0;


/* ----- Get the DAG ------------------------------------------------------- */


static const void *generate_dag(const char *path,
    unsigned cache_size, unsigned *full_lines, unsigned epoch)
{
	uint8_t seed[SEED_BYTES];
	bool cache_override = 0, dag_override = 0;

	if (cache_size)
		cache_override = 1;
	else
		cache_size = get_cache_size(epoch);

	if (*full_lines)
		dag_override = 1;
	else
		*full_lines = get_full_lines(epoch);

	uint8_t *cache = malloc(cache_size);
	uint8_t *dag = malloc((size_t) *full_lines * DAG_LINE_BYTES);

	if (!quiet && !stable)
		printf("Epoch %u, %u bytes cache%s, %llu bytes DAG%s\n",
		    epoch,
		    cache_size, cache_override ? " (override)" : "",
		    (unsigned long long) *full_lines * DAG_LINE_BYTES,
		    dag_override ? " (override)" : "");

	t_start();
	get_seedhash(seed, epoch);
	if (verbose && !stable)
		t_print("Seed");

	t_start();
	mkcache(cache, cache_size, seed);
	if (verbose && !stable)
		t_print("Cache");

	t_start();
	calc_dataset(dag, *full_lines, cache, cache_size);
	if (verbose && !stable)
		t_print("DAG");

	if (path && strcmp(path, "-"))
		mdag_write(path, dag, *full_lines);

	free(cache);

	return dag;
}


static const void *get_dag(const char *path,
    unsigned cache_size, unsigned *full_lines, unsigned epoch)
{
	const void *dag;

	if (path && strcmp(path, "-") && !access(path, R_OK)) {
		bool override = 0;
		unsigned got;

		if (*full_lines)
			override = 1;
		else
			*full_lines = get_full_lines(epoch);
		dag = mdag_open(path, &got);
		if (got != *full_lines) {
			fprintf(stderr,
			    "Epoch %u DAG should be %llu bytes%s, "
			    "loaded %llu bytes from %s\n", epoch,
			    (unsigned long long) *full_lines * DAG_LINE_BYTES,
			    override ? " (override)" : "",
			    (unsigned long long) got * DAG_LINE_BYTES, path);
			exit(1);
		}
		if (verbose && !stable)
			printf("Loaded %llu bytes DAG%s\n",
			    (unsigned long long) *full_lines * DAG_LINE_BYTES,
			    override ? " (override)" : "");
	} else {
		dag = generate_dag(path, cache_size, full_lines, epoch);
	}
	if (stable && verbose)
		printf("DAG: %u lines, %llu bytes\n", *full_lines,
		    (unsigned long long) *full_lines * DAG_LINE_BYTES);
	return dag;
}


/* ----- Mining test ------------------------------------------------------- */


static void try_before(const uint8_t *header_hash, uint64_t nonce)
{
	if (verbose) {
		dump_blob("Header hash", header_hash, HEADER_HASH_BYTES);
		dump_blob("Nonce", &nonce, sizeof(nonce));
	}
}


static void try_after(const uint8_t *cmix, const uint8_t *result,
    unsigned long long difficulty)
{
	if (verbose)
		dump_decimal("CMix", cmix, CMIX_BYTES);
	if (verbose)
		dump_blob("CMix", cmix, CMIX_BYTES);
	if (verbose)
		dump_decimal("Result", result, RESULT_BYTES);
	if (!quiet)
		dump_blob("Result", result, RESULT_BYTES);
	if (difficulty) {
		const uint64_t diff[] = { difficulty, 0, 0, 0 };
		uint8_t target[TARGET_BYTES];

		get_target(target, diff);
		if (verbose)
			dump_blob("Target", target, TARGET_BYTES);
		if (below_target(result, target)) {
			printf("Below target\n");
		} else {
			fprintf(stderr, "Above target\n");
			exit(1);
		}
	}
}


static void try(const uint8_t *dag, unsigned full_lines,
    const uint8_t *header_hash, uint64_t nonce, unsigned long long difficulty)
{
	uint8_t cmix[CMIX_BYTES];
	uint8_t result[RESULT_BYTES];

	try_before(header_hash, nonce);
	hashimoto(cmix, result, header_hash, nonce, dag, full_lines);
	try_after(cmix, result, difficulty);
}


static void try_light(unsigned epoch, unsigned cache_bytes, unsigned full_lines,
    const uint8_t *header_hash, uint64_t nonce, unsigned long long difficulty)
{
	uint8_t *cache;
	uint8_t seed[SEED_BYTES];
	uint8_t cmix[CMIX_BYTES];
	uint8_t result[RESULT_BYTES];

	if (!cache_bytes)
		cache_bytes = get_cache_size(epoch);
	if (!full_lines)
		full_lines = get_full_lines(epoch);

	get_seedhash(seed, epoch);
	cache = malloc(cache_bytes);
	mkcache(cache, cache_bytes, seed);

	try_before(header_hash, nonce);
	hashimoto_light(cmix, result, header_hash, nonce, cache, cache_bytes,
	    full_lines);
	try_after(cmix, result, difficulty);
}


/* ----- Command-line processing ------------------------------------------- */


static void usage(const char *name)
{
	fprintf(stderr,
"usage: %s [dag-file|-] [-c cache_lines] [-d difficulty|-t target_bits]\n"
"       %*s[-f dag_lines] [-q] [-s] [-v [-v [-l]]]\n"
"       %*sepoch header_hash nonce\n"
	    , name, (int) strlen(name) + 1, "", (int) strlen(name) + 1, "");
	exit(1);
}


int main(int argc, char **argv)
{
	const char *dag_arg = NULL;
	const uint8_t *dag = NULL;
	unsigned cache_size = 0, full_lines = 0;
	uint8_t header_hash[HEADER_HASH_BYTES];
	unsigned epoch = 0;
	unsigned long long nonce = 0x42;
	unsigned long long difficulty = 0;
	unsigned target_bits = 0;
	char *end;
	int c;

	while ((c = getopt(argc, argv, "c:d:f:lqst:v")) != EOF)
		switch (c) {
		case 'c':
			cache_size =
			    strtoull(optarg, &end, 0) * CACHE_LINE_BYTES;
			if (*end)
				usage(*argv);
			break;
		case 'd':
			difficulty = strtoull(optarg, &end, 0);
			if (*end)
				usage(*argv);
			break;
		case 'f':
			full_lines = strtoull(optarg, &end, 0);
			if (*end)
				usage(*argv);
			break;
		case 'l':
			mine_trace_linear = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 's':
			stable = 1;
			break;
		case 't':
			target_bits = strtoul(optarg, &end, 0);
			if (*end)
				usage(*argv);
			break;
		case 'v':
			if (verbose)
				mine_trace = stdout;
			verbose = 1;
			break;
		default:
			usage(*argv);
		}

	switch (argc - optind) {
	case 4:
		dag_arg = argv[optind];
		optind++;
		/* fall through */
	case 3:
		epoch = strtoul(argv[optind], &end, 0);
		if (*end)
			usage(*argv);
		if (dag_arg)
			dag = get_dag(dag_arg, cache_size, &full_lines, epoch);
		hex_decode_big_endian(header_hash, argv[optind + 1],
		    HEADER_HASH_BYTES);
		nonce = strtoull(argv[optind + 2], &end, 16);
		if (*end)
			usage(*argv);
		break;
	default:
		usage(*argv);
	}

	(void) target_bits; /* @@@ for later */
	if (dag)
		try(dag, full_lines, header_hash, nonce, difficulty);
	else
		try_light(epoch, cache_size, full_lines, header_hash, nonce,
		    difficulty);

	return 0;
}
