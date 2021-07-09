/*
 * dagalgo.h - DAG algorithm selection
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef LIBDAG_DAGALGO_H
#define	LIBDAG_DAGALGO_H

/*
 * Coin names:
 * etc		ETC
 * ubq		UBQ
 * default	ETH, CLO, EXP, ...
 *
 * Algorithm names:
 * etchash
 * ethash
 * ubqhash
 */


enum dag_algo {
	da_ethash	= 0,	/* Ethash */
	da_etchash	= 1,	/* ETChash, ECIP-1099 */
	da_ubqhash	= 2,	/* UBQhash, UIP-1 */
	dag_algos	= 3	/* must be last */
};


extern unsigned etchash_epoch;


const char *dagalgo_name(enum dag_algo algo);

/*
 * Returns enum dag_algo, -1 if no such algorithm is known.
 */
int dagalgo_code(const char *name);

enum dag_algo dagalgo_map(const char *coin, unsigned *epoch);

#endif /* !LIBDAG_DAGALGO_H */
