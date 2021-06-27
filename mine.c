/*
 * mine.c - Ethash-familiy calculations, for development and verification
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#define _FILE_OFFSET_BITS 64

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "keccak.h"
#include "common.h"
#include "util.h"
#include "dag.h"
#include "dagio.h"
#include "mine.h"


FILE *mine_trace = NULL;
bool mine_trace_linear = 0;


/* ----- Main loop --------------------------------------------------------- */


/*
 * "mix" is 128 bytes (MIX_BYTES)
 * "s" is 64 bytes (HASH_BYTES)
 * header_hash must be 32 bytes (KEC-256 result)
 * "cmix" and "result" are both 32 bytes
 */

void mix_setup(uint8_t *mix, uint8_t *s, const uint8_t *header_hash,
    uint64_t nonce)
{
	/* combine header+nonce into a 64 byte seed */
	uint8_t tmp[HEADER_HASH_BYTES + NONCE_BYTES];

	memcpy(tmp, header_hash, 32);
	write64(tmp + HEADER_HASH_BYTES, nonce);
	if (mine_trace)
		dump_blob("Pre-KEC512", tmp, sizeof(tmp));
	KEC_512(s, tmp, sizeof(tmp));

	/* start the mix with replicated s */
	unsigned i;

	for (i = 0; i != MIX_BYTES / HASH_BYTES; i++)
		memcpy(mix + i * HASH_BYTES, s, HASH_BYTES);
}


uint32_t mix_dag_line(unsigned round0, const uint8_t *mix, const uint8_t *s,
    unsigned full_lines)
{
	unsigned w = MIX_BYTES / WORD_BYTES;

	uint32_t v1 = round0 ^ read32(s);
	uint32_t word_index = round0 % w;
	uint32_t v2 = read32(mix + WORD_BYTES * word_index);
	uint32_t f = fnv(v1, v2);
	uint32_t line = f % full_lines;

	if (mine_trace) {
		fprintf(mine_trace,
		    "--- Calculate DAG address, round %u (0x%x) ---\n"
		    "round0: %u (0x%08x)\n"
		    "s0: 0x%08x\n"
		    "v1 = round0 ^ s0: 0x%08x\n"
		    "w: 0x%08x\n"
		    "word_index = round0 %% w: 0x%08x\n"
		    "v2 = mix[word_index]: 0x%08x\n"
		    "f = fnv(v1, v2): 0x%08x\n"
		    "lines: 0x%08x\n"
		    "line = f %% lines: 0x%08x\n",

		    round0 + 1, round0 + 1,
		    round0, round0, read32(s), v1,
		    w, word_index, v2,
		    f,
		    full_lines, line);
	}

	return line;
}


/*
 * The ASIC organizes data as follows:
 * - the last word comes first, the first word comes last,
 * - we interleave even and odd 32-bit words,
 * - instead of little-endian, words are big-endian,
 *
 * w[30] w[28] w[26] ... w[0] w[31] w[29] ... w[1]
 */

static void print_line_asic(FILE *file, const char *s, const uint8_t *p)
{
	unsigned w = MIX_BYTES / WORD_BYTES;
	unsigned j;

	for (j = 0; j < w; j += 2) {
		if (!(j & 6)) {
			if (j)
				fprintf(file, "\n%*s", (int) strlen(s), "");
			else
				fprintf(file, "%s", s);
		}
		fprintf(mine_trace, " %08x",
		    read32(p + (w - 1 - j) * WORD_BYTES));
	}
	for (j = 1; j < w; j += 2) {
		if (!(j & 6)) {
			if (j > 1)
				fprintf(file, "\n%*s", (int) strlen(s), "");
			else
				fprintf(file, "\n%*s",
				    (int) strlen(s), "(odd)");
		}
		fprintf(mine_trace, " %08x",
		    read32(p + (w - 1 - j) * WORD_BYTES));
	}
	fprintf(file, "\n");
}


static void print_line_linear(FILE *file, const char *s, const uint8_t *p)
{
	unsigned j;

	for (j = 0; j != MIX_BYTES; j++) {
		if (!(j & 15)) {
			if (j)
				fprintf(file, "\n%*s", (int) strlen(s), "");
			else
				fprintf(file, "%s", s);
		}
		fprintf(mine_trace, " %02x", p[j]);
	}
	fprintf(file, "\n");
}


static void print_line(FILE *file, const char *s, const uint8_t *p)
{
	if (mine_trace_linear)
		print_line_linear(file, s, p);
	else
		print_line_asic(file, s, p);
}


void mix_do_mix(uint8_t *mix, const uint8_t *this_dag_line)
{
	unsigned w = MIX_BYTES / WORD_BYTES;
	unsigned j;

	if (mine_trace) {
		fprintf(mine_trace, "--- Mix ---\n");
		print_line(mine_trace, "Mix in: ", mix);
		print_line(mine_trace, "DAG in: ", this_dag_line);
	}
	for (j = 0; j != w; j++) {
		uint8_t *m = mix + j * WORD_BYTES;
		uint32_t v1 = read32(m);
		uint32_t v2 = read32(this_dag_line + j * WORD_BYTES);
		uint32_t f = fnv(v1, v2);

#if 0
		if (mine_trace) {
			fprintf(mine_trace,
			    "j: 0x%08x\n"
			    "   v1 = mix[j]: 0x%08x\n"
			    "   v2 = dag[dag_addr][j]: 0x%08x\n"
			    "   mix[j] = fnv(v1, v2): 0x%08x\n",

			    j, v1,
			    v2,
			    f);
		}
#endif
		write32(m, f);
	}
	if (mine_trace)
		print_line(mine_trace, "Mix out:", mix);
}


void mix_finish(uint8_t *cmix, uint8_t *result, const uint8_t *mix,
    const uint8_t *s)
{
	unsigned w = MIX_BYTES / WORD_BYTES;
	unsigned i;

	if (mine_trace)
		fprintf(mine_trace, "--- Compress mix ---\n");

	/* compress mix */
	for (i = 0; i != w; i += 4) {
		uint32_t v1 = read32(mix + 4 * i);
		uint32_t v2 = read32(mix + 4 * i + 4);
		uint32_t v3 = read32(mix + 4 * i + 8);
		uint32_t v4 = read32(mix + 4 * i + 12);
		uint32_t f1 = fnv(v1, v2);
		uint32_t f2 = fnv(f1, v3);
		uint32_t f3 = fnv(f2, v4);

		if (mine_trace) {
			fprintf(mine_trace,
			    "i: 0x%08x\n"
			    "   v1 = mix[i]: 0x%08x\n"
			    "   v2 = mix[i + 1]: 0x%08x\n"
			    "   v3 = mix[i + 2]: 0x%08x\n"
			    "   v4 = mix[i + 3]: 0x%08x\n"
			    "   f1 = fnv(v1, v2): 0x%08x\n"
			    "   f2 = fnv(f1, v3): 0x%08x\n"
			    "   cmix[i] = fnv(f2, v3): 0x%08x\n",
			    i, v1, v2, v3, v4,
			    f1, f2, f3);
		}
		write32(cmix + i, fnv(fnv(fnv(read32(mix + 4 * i),
		    read32(mix + 4 * i + 4)),
		    read32(mix + 4 * i + 8)),
		    read32(mix + 4 * i + 12)));
	}

	uint8_t tmp2[HASH_BYTES + CMIX_BYTES];

	memcpy(tmp2, s, HASH_BYTES);
	memcpy(tmp2 + HASH_BYTES, cmix, CMIX_BYTES);
	if (mine_trace)
		dump_blob("Pre-KEC256", tmp2, HASH_BYTES + CMIX_BYTES);
	KEC_256(result, tmp2, HASH_BYTES + CMIX_BYTES);
}


void hashimoto(uint8_t *cmix, uint8_t *result, const uint8_t *header_hash,
    uint64_t nonce, const uint8_t *dag, unsigned full_lines)
{
	uint8_t s[HASH_BYTES];
	uint8_t mix[MIX_BYTES];
	unsigned i;
	uint32_t dag_line;

	mix_setup(mix, s, header_hash, nonce);
	for (i = 0; i != ACCESSES; i++) {
		dag_line = mix_dag_line(i, mix, s, full_lines);
		mix_do_mix(mix, dag + (ptrdiff_t) dag_line * DAG_LINE_BYTES);
	}
	mix_finish(cmix, result, mix, s);
}


void hashimoto_fd(uint8_t *cmix, uint8_t *result, const uint8_t *header_hash,
    uint64_t nonce, int dag_fd, unsigned full_lines)
{
	uint8_t s[HASH_BYTES];
	uint8_t mix[MIX_BYTES];
	uint8_t buf[DAG_LINE_BYTES];
	unsigned i;
	uint32_t dag_line;

	mix_setup(mix, s, header_hash, nonce);
	for (i = 0; i != ACCESSES; i++) {
		dag_line = mix_dag_line(i, mix, s, full_lines);
		pread_dag_line(dag_fd, dag_line, buf);
		mix_do_mix(mix, buf);
	}
	mix_finish(cmix, result, mix, s);
}


void hashimoto_dh(uint8_t *cmix, uint8_t *result, const uint8_t *header_hash,
    uint64_t nonce, struct dag_handle *dh, unsigned full_lines)
{
	uint8_t s[HASH_BYTES];
	uint8_t mix[MIX_BYTES];
	uint8_t buf[DAG_LINE_BYTES];
	unsigned i;
	uint32_t dag_line;

	mix_setup(mix, s, header_hash, nonce);
	for (i = 0; i != ACCESSES; i++) {
		dag_line = mix_dag_line(i, mix, s, full_lines);
		dagio_pread(dh, buf, 1, dag_line);
		mix_do_mix(mix, buf);
	}
	mix_finish(cmix, result, mix, s);
}


void hashimoto_light(uint8_t *cmix, uint8_t *result, const uint8_t *header_hash,
    uint64_t nonce, const uint8_t *cache, unsigned cache_bytes,
    unsigned full_lines)
{
	uint8_t s[HASH_BYTES];
	uint8_t mix[MIX_BYTES];
	uint8_t line[DAG_LINE_BYTES];
	unsigned i;
	uint32_t dag_line;

	mix_setup(mix, s, header_hash, nonce);
	for (i = 0; i != ACCESSES; i++) {
		dag_line = mix_dag_line(i, mix, s, full_lines);
		calc_dataset_range(line, dag_line, 1, cache, cache_bytes);
		mix_do_mix(mix, line);
	}
	mix_finish(cmix, result, mix, s);
}


#if 0 /* compact version */

void hashimoto(uint8_t *cmix, uint8_t *result, const uint8_t *header_hash,
    uint64_t nonce, const uint8_t *dag, unsigned full_size)
{
	unsigned n = full_size / HASH_BYTES;
	unsigned w = MIX_BYTES / WORD_BYTES;
	unsigned mixhashes = MIX_BYTES / HASH_BYTES;

	/* combine header+nonce into a 64 byte seed */
	uint8_t tmp[32 + 8];
	uint8_t s[HASH_BYTES];

	memcpy(tmp, header_hash, 32);
	write64(tmp + 32, nonce);
	KEC_512(s, tmp, sizeof(tmp));

	/* start the mix with replicated s */
	uint8_t mix[MIX_BYTES];
	unsigned i;

	for (i = 0; i != MIX_BYTES / HASH_BYTES; i++)
		memcpy(mix + i * HASH_BYTES, s, HASH_BYTES);

	/* mix in random dataset nodes */
	for (i = 0; i != ACCESSES; i++) {
		uint32_t p =
		    fnv(i ^ read32(s), read32(mix + WORD_BYTES * (i % w)))
		    % (n / mixhashes) * mixhashes;
		unsigned j;

		for (j = 0; j != w; j++) {
			uint8_t *m = mix + j * WORD_BYTES;

			write32(m, fnv(read32(m), read32(
			    dag + p * HASH_BYTES + j * WORD_BYTES)));
		}
	}

	/* compress mix */
	for (i = 0; i != w; i += 4)
		write32(cmix + i, fnv(fnv(fnv(read32(mix + 4 * i),
		    read32(mix + 4 * i + 4)),
		    read32(mix + 4 * i + 8)),
		    read32(mix + 4 * i + 12)));

	uint8_t tmp2[HASH_BYTES + CMIX_BYTES];

	memcpy(tmp2, s, sizeof(s));
	memcpy(tmp2 + sizeof(s), cmix, CMIX_BYTES);
	KEC_256(result, tmp2, sizeof(s) + CMIX_BYTES);
}

#endif
