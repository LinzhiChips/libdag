/*
 * mdag.h - Memory-based DAG (for development)
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef LIBDAG_MDAG_H
#define	LIBDAG_MDAG_H

void mdag_write(const char *path, const void *dag, unsigned full_lines);
const void *mdag_open(const char *path, unsigned *full_lines);
void mdag_close(void);

#endif /* !LIBDAG_MDAG_H */
