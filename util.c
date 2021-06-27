/*
 * util.c - Utility functions
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "util.h"


/* ----- Debug dumps --------------------------------------------------------*/


/*
 * Decimal dump, for easy compatbility with printing results from ethash.py
 */

static void dump(const char *s, const void *p, unsigned bytes,
    const char *fmt, unsigned cols)
{
	unsigned i;

	if (s)
		printf("--- %s (%u words) ---\n", s, bytes / 4);
	for (i = 0; i != bytes; i += 4)
		printf(fmt, *(const uint32_t *) (p + i),
		    ((i / 4) % cols) == cols - 1 ? '\n' : ' ');
	if ((bytes / 4) % cols)
		putchar('\n');
}


void dump_hex(const char *s, const void *p, unsigned bytes)
{
	dump(s, p, bytes, "%08x%c", 8);
}


void dump_decimal(const char *s, const void *p, unsigned bytes)
{
	dump(s, p, bytes, "%10u%c", 4);
}


void dump_blob(const char *s, const void *p, unsigned bytes)
{
	unsigned i;

	if (s)
		printf("--- %s (%u bytes) ---\n", s, bytes);
	for (i = 0; i != bytes; i++)
		printf("%02x%s", *(const uint8_t *) p++,
		    (i & 15) == 15 ? "\n" : (i & 7) == 7 ? "  " : " ");
	if (bytes & 15)
		putchar('\n');
}


/* ----- Hex decoding ------------------------------------------------------ */


void hex_decode_big_endian(uint8_t *res, const char *s, unsigned bytes)
{
	const char *t;
	unsigned v;
	bool first = 1;

	if (!strncmp(s, "0x", 2))
		s += 2;
	if (strlen(s) != bytes * 2) {
		fprintf(stderr,
		    "expected %u instead of %u characters: \"%s\"\n",
		    bytes * 2, (unsigned) strlen(s), s);
		exit(1);
	}
	for (t = s; *t; t++) {
		v = *t <= '9' ? *t - '0' :
		    10 + (*t < 'a' ? *t - 'A' : *t - 'a');
		if (v > 15) {
			fprintf(stderr, "non-digit '%c' in \"%s\"\n", *t, s);
			exit(1);
		}
		if (first) {
			*res = v << 4;
		} else {
			*res++ |= v;
		}
		first = !first;
	}
}


/* ----- Performance measurements ------------------------------------------ */


static struct timeval t0;


void t_start(void)
{
	gettimeofday(&t0, NULL);
}


void t_print(const char *s)
{
	struct timeval t;

	gettimeofday(&t, NULL);
	printf("%s: %g s\n", s,
	    t.tv_sec - t0.tv_sec + 1e-6 * (t.tv_usec - t0.tv_usec));
}
