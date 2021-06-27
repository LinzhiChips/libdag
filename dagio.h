/*
 * dagio.h - DAG file I/O
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef LIBDAG_DAGIO_H
#define LIBDAG_DAGIO_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>


#define	MAX_DAG_FILE_BYTES	0xffffff80	/* 2^32 - 2^7 bytes */


struct dag_handle;


void pread_dag_line(int dag_fd, uint32_t dag_line, void *buf);

uint64_t dagio_bytes(const struct dag_handle *h);
unsigned dagio_full_lines(const struct dag_handle *h);

void dagio_pread(struct dag_handle *h, void *buf, uint32_t lines,
    uint32_t dag_line);
void dagio_pwrite(struct dag_handle *h, const void *buf, uint32_t lines,
    uint32_t dag_line);

struct dag_handle *dagio_try_open(const char *name, mode_t mode,
    uint32_t full_lines);
struct dag_handle *dagio_open(const char *name, mode_t mode,
    uint32_t full_lines);
void dagio_close(struct dag_handle *h);
void dagio_close_and_delete(struct dag_handle *h);

#endif /* !LIBDAG_DAGIO_H */
