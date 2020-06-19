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

#ifndef H_OPTSEARCH_DATA
#define H_OPTSEARCH_DATA

#include "common.h"
#include "config.h"
#include "random.h"
#include "spso.h"

#include "sqlite3.h"

typedef enum {
	OPT_DB_NEW = 0,
	OPT_DB_ERROR = 1,
	OPT_DB_RESUME = 6
} opt_db_state_e;

int opt_db_init(char *db_name, opt_config_t * config, int search_space_size,
		spso_dimension_t ** search_space);

int opt_db_finalise(void);

int opt_db_create_schema(int num_dims, spso_dimension_t ** dims);

int opt_db_check_flag(int uid, opt_flag_t * flag);

int opt_db_store_flag(opt_flag_t * flag);

int opt_db_store_flags(opt_config_t * config);

int opt_db_store_dimension(spso_dimension_t * dim);

int opt_db_store_position(int *pos_id, spso_position_t * pos);

int opt_db_update_position_fitness(int id, spso_fitness_t fitness);

int opt_db_store_velocity(int *vel_id, spso_velocity_t * vel);

int opt_db_store_particle(spso_particle_t * p);

int opt_db_store_swarm(spso_swarm_t * swarm);

int opt_db_get_position(int pos_id, spso_position_t * position);

int opt_db_get_particle(int id, spso_particle_t * particle);

/**
 * Intended to be called after a particle has been moved or updated its
 * personal best fitness/position.
 */
int opt_db_update_particle(spso_particle_t * particle);

spso_dimension_t **opt_db_get_searchspace(int *search_space_size);

spso_swarm_t *opt_db_get_swarm(int num_dims, spso_dimension_t ** dims);

int opt_db_store_prng_iter(unsigned long long iter);

int opt_db_get_prng_iter(unsigned long long *iter);

int opt_db_store_converged(int flag);

int opt_db_get_converged(int *flag);

int opt_db_store_prng_seed(unsigned int seed);

int opt_db_get_prng_seed(unsigned int *seed);

/** Find the ID of a particular position if it exists in the database */
int opt_db_find_position(int * pos_id, spso_position_t * position);

/** Find the fitness value for a particular position */
int opt_db_get_position_fitness(int id, double *fitness);

/** Find the number of times a position has been visited */
int opt_db_get_position_visits(int id, int *visits);

/** Update the particle_history table, recording the movement of a particle in
 * the search space against a timestamp. */
int opt_db_update_particle_history(int particle_id, int new_position_id, int new_velocity_id, int best_position_id);

/** Update the global_best_history table, adding a new record for the recently
 * found current global best position in the search space (against a
 * timestamp).
 */
int opt_db_update_global_best_history(int new_position_id);

/**
 * Find the number of unique positions SPSO has searched so far.
 */
int opt_db_get_position_count(int* count);

/**
 * Get the number of times the search has not moved so far.
 */
int opt_db_get_no_move_counter(int *counter);

/**
 * Store the number of times the search has not moved so far.
 */
int opt_db_store_no_move_counter(int counter);

int opt_db_store_prev_prev_best(double fitness);
int opt_db_get_prev_prev_best(double *fitness);
int opt_db_store_prev_best(double fitness);
int opt_db_get_prev_best(double *fitness);
int opt_db_store_curr_best(spso_position_t * position, double fitness);
int opt_db_get_curr_best(spso_position_t * position, double *fitness);

/**
 * Creates an empty struct.  Uninitialised.  This is useful internally and for
 * testing.  It might get moved later to a more appropriate place.
 */
spso_position_t *opt_db_new_position(int num_dims, spso_dimension_t ** dims);

spso_velocity_t *opt_db_new_velocity(int num_dims, spso_dimension_t ** dims);

spso_particle_t *opt_db_new_particle(int uid, int num_dims,
				     spso_dimension_t ** dims);

spso_swarm_t *opt_db_new_swarm(int size, int num_dims,
			       spso_dimension_t ** dims);

#endif				/* include guard H_OPTSEARCH_DATA */
