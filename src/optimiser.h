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

#ifndef H_OPTSEARCH_OPTIMISER_
#define H_OPTSEARCH_OPTIMISER_

#include "common.h"
#include "config.h"
#include "taskfarm.h"
#include "spso.h"
#include "data.h"

/**
 * This is intended to be used to feed our queue for the MPI task farm.
 *
 * It drives our optimisation/search algorithms and provides an interface
 * between these and the task farm.  It is a translation/glue layer providing
 * helper functions that allow us to do things such as map between the
 * compiler flags and the dimensions of the search space.
 *
 * By default, the search is done via a simple standard particle swarm
 * optimiser:
 * https://en.wikipedia.org/wiki/Particle_swarm_optimization
 *
 */

typedef spso_particle_t opt_particle_t;	/* Alias for convenience */
typedef spso_dimension_t opt_dimension_t;	/* Alias for convenience */

/**
 * Begin the search.
 *
 * This initialises SPSO and starts the task farm.
 * @see opt_init(opt_config_t * config)
 */
void opt_start_search(opt_config_t * config);

/**
 * Stop searching. This empties the task farm queue, and reports our current
 * best found position and its fitness score.
 */
void opt_stop_search(void);

void opt_clean_up(void);

/**
 * Initialise the optimiser using data derived from the supplied config.
 *
 * Initialises SPSO with dimensions created from flags in an
 * opt_config_t to spso_dimension_t structs.  Also initialises the task farm
 * so that it can be used to provide the fitness evaluations.
 *
 * @see opt_convert_flag
 * @see spso_init
 * @see opt_task_initialise
 * @param conf the opt_config_t providing the compiler flags
 */
void opt_init(opt_config_t * config);

/**
 * Free the memory pointed to by the supplied dimension.  This also frees the
 * memory used to store the dimension's name.
 *
 * @param dim the spso_dimension_t to free()
 */
void opt_destroy_dimension(spso_dimension_t * dim);

/**
 * Convert from opt_flag_t to opt_dimension_t.  The returned dimension is
 * simply the result of a malloc, and must be freed with free().  The name of
 * the dimension is created via a strcpy, so this should be freed too.  To
 * make life easier, you can just call opt_destroy_dimension(dimension).
 *
 * Note that opt_dimension_t is a typedef of spso_dimension_t.
 *
 * @param flag the opt_flag_t to convert
 * @return the corresponding opt_dimension_t
 */
opt_dimension_t *opt_convert_flag(opt_flag_t * flag);

opt_flag_t *opt_get_flag(int flag_uid);

/**
 * Take a particle, and use its current position to build a string
 * containing the set of compiler options/flags that point in the search space
 * represents.
 *
 * The string is created via malloc and must be freed after use.
 *
 * @param particle the opt_particle_t
 * @return the string
 * @see opt_position_to_string(spso_position_t *)
 */
char *build_compiler_options(opt_particle_t * particle);

/**
 * Build a string of compiler flags representing a particular position in the
 * search space.
 *
 * @param position the position in the search space
 * @return the flag string
 * @see convert_dimension_to_string(opt_dimension_t *, int)
 */
char *opt_position_to_string(spso_position_t * position);

/**
 * Take a particular dimension and return its string representation.  This is
 * assumed to be a single dimension component of a set of coordinates
 * representing a compiler flag.  The value for that dimension is also
 * required.
 */
char *convert_dimension_to_string(opt_dimension_t * dim, int value);

#endif				/* include guard H_OPTSEARCH_OPTIMISER_ */
