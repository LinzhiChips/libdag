/*
 * mdag.c - Memory-based DAG (for development)
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "dag.h"
#include "mdag.h"


static int fd = -1;
static void *addr;
static size_t length;


void mdag_write(const char *path, const void *dag, unsigned full_lines)
{
	FILE *file;

	file = fopen(path, "w");
	if (!file) {
		perror(path);
		exit(1);
	}
	if (fwrite(dag, (size_t) full_lines * DAG_LINE_BYTES, 1, file) != 1) {
		perror(path);
		exit(1);
	}
	if (fclose(file) < 0) {
		perror(path);
		exit(1);
	}
}


const void *mdag_open(const char *path, unsigned *full_lines)
{
	struct stat st;

	assert(fd == -1);
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror(path);
		exit(1);
	}
	if (fstat(fd, &st) < 0) {
		perror(path);
		exit(1);
	}
	if (st.st_size % DAG_LINE_BYTES) {
		fprintf(stderr,
		    "DAG (%llu) size must be multiple of %u bytes\n",
		    (unsigned long long) st.st_size, DAG_LINE_BYTES);
		exit(1);
	}
	length = st.st_size;
	*full_lines = length / DAG_LINE_BYTES;
	addr = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED) {
		perror(path);
		exit(1);
	}
	return addr;
}


void mdag_close(void)
{
	if (munmap(addr, length) < 0) {
		perror("munmap");
		exit(1);
	}
	if (close(fd) < 0) {
		perror("close");
		exit(1);
	}
	fd = -1;
}
