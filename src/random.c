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

#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <float.h>
#include <inttypes.h>
/* XXX This is Linux-specific, and only present in kernels newer than 3.17
 * #include <sys/random.h>
 * Would be nice to be able to use it, rather than reading from /dev/urandom
 * explicitly.
 */

#include "WELL512a.h"
#include "random.h"

/* Set by WELL512a */
const unsigned int SEED_SIZE = R;

/* This function is provided for debugging */
void opt_rand_print_seed(FILE * stream, opt_rand_seed_t * seed)
{
	unsigned int i;
	if (seed == NULL) {
		log_error("Seed is NULL.");
		return;
	}
	if (seed->seed == NULL) {
		log_error("Seed contains NULL array!!");
		return;
	}
	log_info("Printing seed of size %d:", seed->size);
	for (i = 0; i < seed->size; i++) {
		/* printf format macro for uint32_t */
		fprintf(stream, "%" PRIu32 "", seed->seed[i]);
	}
	fprintf(stream, "\n");
}

opt_rand_seed_t *opt_rand_gen_seed(void)
{
	/*
	 * Generate a SEED_SIZE array of 32-bit integers, to use when
	 * initialising our PRNG.
	 *
	 * NB These are coming from /dev/urandom which could be Linux-specific
	 */
	size_t ret;
	opt_rand_seed_t *seed = malloc(sizeof(*seed));
	seed->size = SEED_SIZE;
	seed->seed = (uint32_t *) malloc(sizeof(uint32_t) * seed->size);
	FILE *fh = fopen("/dev/urandom", "rb");
	if (fh == NULL) {
		/* TODO -- make use of errno */
		log_error("Unable to initialise from /dev/urandom.");
		/* TODO Initialise to the default as a compromise */
		opt_rand_free(seed);
		fclose(fh);
		return NULL;
	}
	ret = fread(seed->seed, sizeof(uint32_t), seed->size, fh);
	/* TODO This is different if seed->size is 1 */
	if (ret < seed->size) {
		/* TODO -- make use of errno */
		log_error("Unable to initialise from /dev/urandom.");
		/* TODO
		 * Initialise to the default as a compromise.  Who knows what we got
		 * if we managed a partial read of /dev/urandom? */
		opt_rand_free(seed);
		seed = NULL;
	}
	fclose(fh);
	return seed;
}

opt_rand_seed_t *opt_rand_init(void)
{
	opt_rand_seed_t *seed = opt_rand_gen_seed();
	opt_rand_init_seed(seed);

	return seed;
}

void opt_rand_free(opt_rand_seed_t * seed)
{
	/* TODO Check for errors */
	if (seed != NULL) {
		if (seed->seed != NULL) {
			free(seed->seed);
			seed->seed = NULL;
		}
		free(seed);
	}
}

void opt_rand_init_seed(opt_rand_seed_t * seed)
{
	/*
	 * This cast may not be safe if unsigned int is not the same
	 * size as uint32_t.
	 */
	if (sizeof(uint32_t) != sizeof(unsigned int)) {
		log_warn
		    ("Unsigned integers (used by our PRNG) on this system are not 32-bit, as assumed by the code.  Strange things may happen.");
	}
	InitWELLRNG512a((unsigned int *)seed->seed);
}

int opt_rand_int(void)
{
	/* The compiler seems to be able to convert between signed/unsigned int,
	 * but not uint32_t and anything else, so we go via unsigned int here to
	 * help the compiler convert correctly for us. */
	unsigned int foo;
	foo = (unsigned int)WELLRNG512a_int();
	return (int)foo;
}

/**
 * @see opt_rand_int(void) for 
 */
int opt_rand_int_range(int min, int max)
{
	/*
	 * Treat as conversion from one range to another, attempting to preserve
	 * distribution.
	 */
	long tmp;		/* This is used because GCC handles type conversion for us if we use doubles and ints */
	long tmp1, tmp2;
	int x, rand;
	if (max == min) {
		return min;
	}
	if (max < min) {
		/* Assume user error */
		log_warn
		    ("random.c: In call to opt_rand_int_range(), provided max and min appear to be reversed");
		long t = max;
		max = min;
		min = t;
	}
	rand = opt_rand_int();
	/*
	 * This isn't quite uniform as min-max is not an exact multiple of
	 * min-max+1, so there will be a very slight bias.
	 */
	tmp1 = (long)max - abs(min) + 1.0;
	if (tmp1 == 0) {
		/* TODO FIXME This introduces a bias */
		tmp1 = abs(max)/2;
		if (rand >= min && rand <= max) {
			log_trace("random.c: Using plain %d (interval was [%d,%d]).", rand, min, max);
			return rand;
		}
	}
	tmp2 = rand % tmp1;
	tmp = (long)min + abs(tmp2);
	if (tmp > INT_MAX || tmp < INT_MIN) {
		log_warn("Scaled value is out of range for an int");
	}
	x = (int)tmp;
	log_trace("random.c: Scaled %d to %d (interval was [%d,%d]).", rand, x,
		  min, max);
	if (x < min || x > max) {
		log_error
		    ("Generated a value outside of the requested range :(");
	}
	return x;
}

double opt_rand_double(void)
{
	return WELLRNG512a_double();
}

double opt_rand_double_range(double min, double max)
{
	/* FIXME This does not work, so it should not be used anywhere until it is
	 * fixed */
	double rand, x;
	long double a, b, c;
	if (max == min) {
		return min;
	}
	if (max < min) {
		/* Assume user error */
		log_warn
		    ("random.c: In call to opt_rand_double_range(), provided max and min appear to be reversed");
		double t = max;
		max = min;
		min = t;
	}
	rand = opt_rand_double();
	//log_debug("min: %e, max: %e, rand: %e", min, max, random);
	a = max - fabs(min) + 1.0;
	b = fmod(a, rand);
	c = min + fabs(b);
	x = (double)c;
	log_trace("Scaled %.6e to %.6e (interval was [%.6e,%.6e]).", rand, x,
		  min, max);
	return x;
}

opt_rand_seed_t *opt_rand_get_state(void)
{
	opt_rand_seed_t *seed = malloc(sizeof(*seed));
	seed->size = SEED_SIZE;
	seed->seed = WELLRNG512a_get_state();
	return seed;
}
