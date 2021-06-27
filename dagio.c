/*
 * dagio.c - DAG file I/O
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#define _GNU_SOURCE	/* for asprintf */
#define _FILE_OFFSET_BITS 64

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "linzhi/alloc.h"

#include "dag.h"
#include "dagio.h"


/* --- old API ------------------------------------------------------------- */


void pread_dag_line(int dag_fd, uint32_t dag_line, void *buf)
{
	ssize_t got;

	got = pread(dag_fd, buf, DAG_LINE_BYTES,
	    (off_t) dag_line * DAG_LINE_BYTES);
	if (got < 0) {
		perror("pread");
		exit(1);
	}
	if (got != DAG_LINE_BYTES) {
		fprintf(stderr, "short read %u < %u\n",
		    (unsigned) got, DAG_LINE_BYTES);
		exit(1);
	}
}


/* --- new API ------------------------------------------------------------- */


#define	DAG_FDS		2
#define	LINES_PER_FILE	(MAX_DAG_FILE_BYTES / DAG_LINE_BYTES)


struct dag_handle {
	char		*name[DAG_FDS];
	int		fd[DAG_FDS];
	uint32_t	full_lines;
};


uint64_t dagio_bytes(const struct dag_handle *h)
{
	uint64_t size = 0;
	unsigned i;
	struct stat st;

	for (i = 0; i != DAG_FDS; i++)
		if (h->fd[i] >= 0) {
			if (fstat(h->fd[i], &st) < 0) {
				perror(h->name[i]);
				exit(1);
			}
			size += st.st_size;
			if (st.st_size != MAX_DAG_FILE_BYTES)
				break;
		}
	return size;
}


unsigned dagio_full_lines(const struct dag_handle *h)
{
	return h->full_lines;
}


void dagio_pread(struct dag_handle *h, void *buf, uint32_t lines,
    uint32_t dag_line)
{
	unsigned i = 0;

	assert(dag_line + lines <= h->full_lines);
	while (dag_line >= LINES_PER_FILE) {
		i++;
		dag_line -= LINES_PER_FILE;
	}
	while (lines) {
		assert(i < DAG_FDS);
		pread_dag_line(h->fd[i], dag_line, buf);
		dag_line++;
		buf += DAG_LINE_BYTES;
		if (dag_line == LINES_PER_FILE) {
			dag_line = 0;
			i++;
		}
		lines--;
	}
}


void dagio_pwrite(struct dag_handle *h, const void *buf, uint32_t lines,
    uint32_t dag_line)
{
	unsigned i = 0;
	unsigned n;
	size_t bytes;
	ssize_t wrote;

	assert(dag_line + lines <= h->full_lines);
	while (dag_line >= LINES_PER_FILE) {
		i++;
		dag_line -= LINES_PER_FILE;
	}
	while (lines) {
		assert(i < DAG_FDS);
		if (dag_line + lines < LINES_PER_FILE)
			n = lines;
		else
			n = LINES_PER_FILE - dag_line;
		bytes = (size_t) n * DAG_LINE_BYTES;
		wrote = pwrite(h->fd[i], buf, bytes,
		    (off_t) dag_line * DAG_LINE_BYTES);
		if (wrote < 0) {
			perror(h->name[i]);
			exit(1);
		}
		if ((size_t) wrote != bytes) {
			fprintf(stderr, "%s: short write (%llu < %llu)\n",
			    h->name[i], (unsigned long long) wrote,
			    (unsigned long long) bytes);
			exit(1);
		}
		dag_line += n;
		buf += bytes;
		if (dag_line == LINES_PER_FILE) {
			dag_line = 0;
			i++;
		}
		lines -= n;
	}
}


struct dag_handle *dagio_try_open(const char *name, mode_t mode,
    uint32_t full_lines)
{
	struct dag_handle *h = alloc_type(struct dag_handle);
	unsigned n, i;
	int error;

	h->full_lines = full_lines;
	n = (full_lines + LINES_PER_FILE - 1) / LINES_PER_FILE;
	for (i = 0; i != DAG_FDS; i++) {
		h->fd[i] = -1;
		h->name[i] = NULL;
	}
	for (i = 0; i != n; i++) {
		if (i) {
			if (asprintf(&h->name[i], "%s-%u", name, i) < 0)
				goto fail;
		} else {
			h->name[i] = stralloc(name);
		}
		h->fd[i] = open(h->name[i], mode, 0666);
		if (h->fd[i] < 0)
			goto fail;
	}
	return h;

fail:
	error = errno;
	dagio_close(h);
	errno = error;
	return NULL;
}


struct dag_handle *dagio_open(const char *name, mode_t mode,
    uint32_t full_lines)
{
	struct dag_handle *h;

	h = dagio_try_open(name, mode, full_lines);
	if (h)
		return h;
	perror(name);
	exit(1);
}


static void close_and_delete(struct dag_handle *h, bool del)
{
	unsigned i;

	for (i = 0; i != DAG_FDS; i++) {
		if (h->fd[i] >= 0)
			if (close(h->fd[i]) < 0)
				perror(h->name[i]);
		if (del && h->name[i])
			if (unlink(h->name[i]) < 0)
				perror(h->name[i]);
		free(h->name[i]);
	}
	free(h);
}


void dagio_close(struct dag_handle *h)
{
	close_and_delete(h, 0);
}


void dagio_close_and_delete(struct dag_handle *h)
{
	close_and_delete(h, 1);
}
