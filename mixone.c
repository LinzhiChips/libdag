/*
 * mixone.c - Run one Ethash calculation
 * 
 * Copyright (C) 2021 Linzhi Ltd.
 *   
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 *
 *
 * Example
 * ./mixone 589824 0x0000000000000000000000000000000000000000000000000000000000001234 0x303
 * mix 4cfa39f339fc10ed76a9fecfba646b0741b7f2e9bfd579f7e7f89487323dbe2ba1ce23d1d4487314b9ed3379db8deb5361145dd16d3319f810c0c0f66aba93f492621eee85ebf9221823861dd782bcf954f7c3cbdc1f618fc6842f53c1fe6441feef9bbcd9c8e363ede2c7f617af7371e6d195c57e5fc01ef8c4d96ff9a90501
 * cmix 5b05ca86b8602a37d67023dd7ebdbb8b8396e0ffbd1a0b83464ed67e1a9f0c36
 * res 10ffef979047b8d63d39135c6bf812047ffa6bfaf01dfeb33bc1dd2a19d970a9
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "linzhi/alloc.h"

#include "common.h"
#include "dag.h"
#include "mine.h"

#include "util.h"


enum mode {
	mode_synth,	/* cache size 1, all-zero seed hash, DAG lines */
	mode_epoch,	/* parameter is (ETH) epoch number */
	mode_block,	/* parameter is (ETH) block number */
};

static bool trace = 0;
static bool quiet = 0;


/* ----- Helper functions -------------------------------------------------- */


static void dump_bytes(const char *s, uint8_t *buf, unsigned bytes)
{
	unsigned i;

	printf("%s ", s);
	for (i = 0; i != bytes; i++)
		printf("%02x", buf[i]);
	printf("\n");
}


static void dump_bytes_reversed(const char *s, uint8_t *buf, unsigned bytes)
{
	unsigned i;

	printf("%s ", s);
	for (i = 0; i != bytes; i++)
		printf("%02x", buf[bytes - i - 1]);
	printf("\n");
}


static void reverse_bytes(uint8_t *buf, unsigned bytes)
{
	unsigned i;
	uint8_t tmp;

	for (i = 0; i != bytes / 2; i++) {
		tmp = buf[i];
		buf[i] = buf[bytes - i - 1];
		buf[bytes - i - 1] = tmp;
	}
}


/* ----- Run the nonce ----------------------------------------------------- */


static void doit(enum mode mode, unsigned n, const uint8_t *header_hash,
    uint64_t nonce, unsigned pattern_bytes, uint8_t pattern[TARGET_BYTES])
{
	uint8_t *cache;
	uint8_t seed[SEED_BYTES];
	uint8_t cmix[CMIX_BYTES];
	uint8_t result[RESULT_BYTES];
	uint8_t s[HASH_BYTES];
	uint8_t mix[MIX_BYTES];
	uint8_t line[DAG_LINE_BYTES];
	unsigned cache_bytes;
	unsigned dag_lines;
	uint32_t dag_line;
	unsigned i;

	switch (mode) {
	case mode_synth:
		/* this is the setup used by cgen, not real Ethash */
		memset(seed, 0, sizeof(seed));
		cache_bytes = CACHE_LINE_BYTES;
		dag_lines = n;
		break;
	case mode_block:
		n = get_epoch(n);
		/* fall through */
	case mode_epoch:
		cache_bytes = get_cache_size(n);
		dag_lines = get_full_lines(n);
		get_seedhash(seed, n);
		break;
	default:
		abort();
	}
	cache = alloc_size(cache_bytes);
	mkcache(cache, cache_bytes, seed);

	do {
		/*
		 * This is hashimoto_light, but we copy it here to have access
		 * to "mix".
		 */
		mix_setup(mix, s, header_hash, nonce);
		if (!quiet)
			dump_bytes_reversed("mix", mix, MIX_BYTES);

		for (i = 0; i != ACCESSES; i++) {
			dag_line = mix_dag_line(i, mix, s, dag_lines);
			if (trace)
				printf("DA%-2d 0x%07x\n", i + 1, dag_line);
			calc_dataset_range(line, dag_line, 1, cache,
			    cache_bytes);
			mix_do_mix(mix, line);
		}
		mix_finish(cmix, result, mix, s);

		if (pattern_bytes && !memcmp(result, pattern, pattern_bytes))
			printf("0x%llx\n", (unsigned long long) nonce);
		if (!quiet) {
			dump_bytes_reversed("mix", mix, MIX_BYTES);
			dump_bytes("cmix", cmix, CMIX_BYTES);
			dump_bytes("res", result, RESULT_BYTES);
		}
		nonce++;
	} while (pattern_bytes);
}


/* ----- Command-line processing ------------------------------------------- */


static unsigned get_pattern(uint8_t *pattern, const char *s)
{
	unsigned n = 0;
	unsigned long v;
	char *end;

	while (*s) {
		if (n == TARGET_BYTES) {
			fprintf(stderr, "pattern length is <= %u bytes\n",
			    TARGET_BYTES);
			exit(1);
		}
		v = strtoul(s, &end, 16);
		if ((*end && *end != ',') || v > 255) {
			fprintf(stderr, "bad pattern value \"%s\"\n", s);
			exit(1);
		}
		s = end + !!*end;
		*pattern++ = v;
		n++;
	}
	return n;
}


static void usage(const char *name)
{
	fprintf(stderr,
"usage: %s [-r] [-t] {-b block | -e epoch | dag_lines} header_hash nonce\n\n"
"  -b block\n"
"      use real Ethash parameters, for given block.\n"
"  -e epoch\n"
"      use real Ethash parameters, for any block in given epoch.\n"
"  -p hex-byte,...\n"
"      search for nonces matching the specified pattern\n"
"  -q  quiet operation\n"
"  -r  byte-reverse the header hash\n"
"  -t  trace DAG addresses over the mixing rounds\n"
	    , name);
	exit(1);
}


int main(int argc, char **argv)
{
	enum mode mode = mode_synth;
	unsigned n;
	uint8_t header_hash[HEADER_HASH_BYTES];
	unsigned long long nonce;
	uint8_t pattern[TARGET_BYTES];
	unsigned pattern_bytes = 0;
	bool reverse = 0;
	char *end;
	int c;

	while ((c = getopt(argc, argv, "bertp:q")) != EOF)
		switch (c) {
		case 'b':
			mode = mode_block;
			break;
		case 'e':
			mode = mode_epoch;
			break;
		case 'r':
			reverse = 1;
			break;
		case 't':
			trace = 1;
			break;
		case 'p':
			pattern_bytes = get_pattern(pattern, optarg);
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			usage(*argv);
		}

	switch (argc - optind) {
	case 3:
		break;
	default:
		usage(*argv);
	}

	n = strtoull(argv[optind], &end, 0);
	if (*end)
		usage(*argv);
	hex_decode_big_endian(header_hash, argv[optind + 1], HEADER_HASH_BYTES);
	if (reverse)
		reverse_bytes(header_hash, HEADER_HASH_BYTES);
	nonce = strtoull(argv[optind + 2], &end, 16);
	if (*end)
		usage(*argv);

	doit(mode, n, header_hash, nonce, pattern_bytes, pattern);

	return 0;
}
