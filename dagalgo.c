/*
 * dagalgo.c - DAG algorithm selection
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "linzhi/alloc.h"

#include "dagalgo.h"


#define	ETCHASH_EPOCH	390


unsigned etchash_epoch = ETCHASH_EPOCH;


/* ----- Algorithm names --------------------------------------------------- */


const char *dagalgo_name(enum dag_algo algo)
{
	switch (algo) {
	case da_ethash:
		return "ethash";
	case da_etchash:
		return "etchash";
	default:
		fprintf(stderr, "unknown algorithm %u\n", algo);
		abort();
	}
}


int dagalgo_code(const char *name)
{
	if (!strcmp(name, "ethash"))
		return da_ethash;
	if (!strcmp(name, "etchash"))
		return da_etchash;
	return -1;
}


/* ----- ETC --------------------------------------------------------------- */


static enum dag_algo map_etc(unsigned *epoch)
{
	if (*epoch < etchash_epoch)
		return da_ethash;
	*epoch /= 2;
	return da_etchash;
}


/* ----- ETH --------------------------------------------------------------- */


static enum dag_algo map_eth(unsigned *epoch)
{
	return da_ethash;
}


/* ----- Coin selection ---------------------------------------------------- */


struct coin {
	const char *name;	/* lower case; NULL for default */
	enum dag_algo (*map)(unsigned *epoch);
};

static struct coin coins[] = {
	{ "etc",	map_etc },
	{ NULL,		map_eth },
};


enum dag_algo dagalgo_map(const char *coin, unsigned *epoch)
{
	const struct coin *c = coins;

	while (c->name && strcmp(c->name, coin))
		c++;
	return c->map(epoch);
}
