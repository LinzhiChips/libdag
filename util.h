/*
 * util.h - Utility functions
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef LIBDAG_UTIL_H
#define	LIBDAG_UTIL_H

#include <stdint.h>


void dump_hex(const char *s, const void *p, unsigned bytes);
void dump_decimal(const char *s, const void *p, unsigned bytes);

void dump_blob(const char *s, const void *p, unsigned bytes);

void hex_decode_big_endian(uint8_t *res, const char *s, unsigned bytes);

void t_start(void);
void t_print(const char *s);

#endif /* !LIBDAG_UTIL_H */
