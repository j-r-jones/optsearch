/* 
 * This file is part of OptSearch.
 *
 * OptSearch is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OptSearch is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OptSearch.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2007-2018 Jessica Jones
 */

#ifndef H_OPTSEARCH_RAND_
#define H_OPTSEARCH_RAND_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include "logging.h"

/**
 * @section random_number-generation
 * pseudo-random number generator wrapper
 *
 * The following functions allow you to use a portable, fast and good
 * pseudo-random number generator (PRNG) without changing it's use throughout
 * all of the optsearch code.
 * 
 * This is just a simple wrapper for the PRNG of our choice, which is currently
 * WELL RNG 512, originally written by F Panneton, P L'Ecuyer and M Matsumoto.
 * Further information is available
 * [from this page](http://www.iro.umontreal.ca/~panneton/WELLRNG.html)
 *
 */

typedef struct {
	uint32_t *seed;
	unsigned int size;
} opt_rand_seed_t;

/* To assist with debugging */
extern void opt_rand_print_seed(FILE * stream, opt_rand_seed_t * seed);

extern opt_rand_seed_t *opt_rand_gen_seed(void);

extern void opt_rand_free(opt_rand_seed_t * seed);

extern opt_rand_seed_t *opt_rand_get_state(void);

extern opt_rand_seed_t *opt_rand_init(void);

extern void opt_rand_init_seed(opt_rand_seed_t * seed);

extern double opt_rand_double(void);

extern double opt_rand_double_range(const double min, const double max);

extern int opt_rand_int(void);

extern int opt_rand_int_range(const int min, const int max);

#endif				/* include guard H_OPTSEARCH_RAND_ */
