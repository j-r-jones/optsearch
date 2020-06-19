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

/**
 * By default, this is a standard particle swarm optimiser.  See:
 * https://hal.archives-ouvertes.fr/file/index/docid/764996/filename/SPSO_descriptions.pdf
 *
 * Later I hope to replace it with the asynchronous parallel version, and also
 * to investigate parameters that will make it run more optimally in our
 * environment and for the types of problems we are looking at.
 *
 * Extracted from the PDF, the algorithm is essentially:
 *
 * Particles are just candidate solutions.  They are a collection of things:
 * - a current set of values to test (position in the search space)
 * - a fitness of those values
 * - a displacement, called the velocity, used to compute the next position in
 *   the search space
 * - a personal best position and its fitness
 *
 * Note that there is also:
 *
 * - a neighbourhood (an area of the swarm that is updated by this particle
 *   and from which this particle receives updates).  This may be the entire
 *   swarm.  That option seems to be the easiest option to implement.
 * - a global best currently known position for that neighbourhood or for the
 *   swarm as a whole (this is not obvious to me at this time if there are
 *   multiple neighbourhoods)
 * - a topology within the neighbourhood describing how particles within that
 *   neighbourhood communicate to update one another.  There are various ways
 *   this can be done.  A ring was used originally but since 2006 has been
 *   abandoned in favour of a more robust, partly random method. (A ring
 *   topology is easy to implement.)
 *   See also: http://clerc.maurice.free.fr/pso/random_topology.pdf
 *
 * Initialise the swarm:
 * - For each particle:
 *   - Choose a random position within the search space
 *   - Pick a random velocity
 *
 * On each iteration:
 * - Compute the new velocity (displacement) by combining the following
 *   elements:
 *   - current position
 *   - personal previous best position
 *   - previous best in the neighbourhood (area of the swarm that informs this
 *   particle)
 * - Apply velocity to current position to move particle to next position
 * - Apply a confinement method to ensure that we stay within the search space
 *   - If within the search space, then we can compute the new fitness of this
 *   position.  If outside, no re-evaluation.  This is the simplest approach.
 *   - Compare fitness of current position with that of personal best.
 *   Replace personal best if the current position is better.  Update fitness
 *   of personal best too.
 *
 * Stop iterations when:
 *  - In cases where the fitness value for the optimum point is known, a
 *  maximum admissible error is set.  When the absolute difference between
 *  this known fitness on the optimum point and the best one found so far is
 *  smaller than this error, we stop searching.
 *  - With a maximum number of fitness evaluations set at the start, we hit
 *  this threshold.  In the standard PSO the swarm size is constant so this is
 *  often considered the maximum number of iterations.  However that assumes
 *  swarm size is calculated from the number of dimensions, which we are not
 *  doing.
 *
 * Note that we don't know what the best size for the swarm should be.  SPSO
 * 2011 suggested a swarm size of 40, or setting something "around" a value
 * like this.  Previous methods of determining the size from the number of
 * dimensions gave a far from optimal swarm size.  Research in this area is
 * on-going and this is not a rabbit hole I want to fall into right now.
 *
 * I to use a swarm size of (number of dimensions + 1), which
 * intuitively feels like a good size, though it may prove too small if the
 * number of MPI ranks used by the task farm is greater than this.
 */
#ifndef H_OPTSEARCH_SPSO_
#define H_OPTSEARCH_SPSO_

#include "common.h"
#include "random.h"

/* The maximum number of dimensions to allow in our search space.
 * This choice is arbitrary and should be based on something, like available
 * system memory where MPI rank 1 is running. */
#define spso_max_dims 4096

/* The number of times we can perform a fitness evaluation and not move before
 * deciding that the search must have converged and we aren't going to find
 * anything better.
 * This may need reviewing in future as it is an arbitrary choice. */
#define NO_MOVEMENT_THRESHOLD 200

extern const int spso_default_swarm_size;
extern const double sigma;	/* "a bit beyond" */
extern const double omega;	/* inertia */

typedef double spso_fitness_t;

typedef struct {
	/* Dimensions have a min and max value, and may have a default value in
	 * the context we're using it in.  I doubt we care about the default, but
	 * we do care about the min and max values. */
	int max;
	int min;
	int uid;		/* should match the UID of the flag that this dimension corresponds to */
	char *name;		/* eg loop-tile-size; this is really for debugging and should match the flag */
} spso_dimension_t;

typedef struct {
	spso_dimension_t ** dimensions;
	int size;
} spso_search_space_t;

/* A velocity has the same number of components as
 * there are dimensions, and is really a displacement. */
typedef struct {
	int *dimension;
} spso_velocity_t;

/* A position is a set of values for each dimension, similar to the velocity */
typedef struct {
	int *dimension;
} spso_position_t;

typedef struct {
	/* Particles have a current best position (theirs), a current position and a
	 * current velocity; each particle is part of a neighbourhood, which is
	 * sub-swarm within the main swarm.  This last is not kept track of within
	 * the particle itself; the swarm has a list of particles instead. */
	int uid;
	spso_position_t position;
	spso_velocity_t velocity;
	spso_position_t previous_best;
	spso_fitness_t previous_best_fitness;
} spso_particle_t;

typedef struct {
	/* Swarms have a current best position, a previous best position, a
	 * fitness associated with each of these, a size and a collection of particles */
	/* None of these are actually used:
	   spso_position_t current_best;
	   spso_fitness_t current_best_fitness;
	   spso_position_t previous_best;
	   spso_position_t previous_best_fitness;
	 */
	int size;
	spso_particle_t **particles;
} spso_swarm_t;

typedef enum {
	SPSO_GLOBAL_BEST_UPDATE_LISTENER,
	SPSO_PARTICLE_MOVE_LISTENER,
	SPSO_POSITION_FITNESS_LISTENER,
	SPSO_STOP_LISTENER
} spso_listener_e;

typedef int (*spso_listener_f) (void);

/**
 * Only partially implemented.  Ideally, you would be able to register
 * different types of listener through one or two functions.
 */
int
spso_register_listener(spso_listener_e listener_type, spso_listener_f listener);

/**
 * The function to call when a fitness is reported back for a particular
 * position, eg by the MPI task farm.
 */
typedef void (*spso_particle_update_listener_f) (int particle_uid,
						 spso_fitness_t fitness);

/**
 * Our objective function or fitness function pointer, passed to our
 * initialisation function.
 *
 * This takes a UID to denote the particle, and a position in the search
 * space. It adds the position to a queue of work for a task farm or similar
 * to churn through.
 *
 * Example usage:
 * spso_fitness_function(particle_uid, &spso_update_particle);
 *
 * @param particle_uid the uid of the particle this position is from
 * @param position the position within the search space
 */
typedef void (*spso_obj_fun_t) (int particle_uid);

/**
 * Create and initialise the swarm of particles and set up the PRNG.
 * The size of the swarm is chosen based on the number of dimensions +1,
 * although this could perhaps be more intelligently chosen depending on the
 * number of MPI ranks our task farm has.  No ranks should be starved of work.
 *
 * @param num_of_dimensions the number of dimensions in the search space
 * @param dimensions the dimensions of the search space
 * @param objective_function the function used to measure fitness of each
 * @param epsilon the experimental error
 * point in the search space
 */
void spso_init(int num_of_dimensions, spso_dimension_t ** dimensions,
	       spso_obj_fun_t objective_function, double epsilon,
	       int (*stop_listener) (void));

/**
 * Initialise from a previous run.  It is assumed that a swarm will have been
 * assembled elsewhere from data recorded about a previous run.  This is how
 * we initialise when resuming a search.  In such cases, this function should
 * be called instead of spso_init().
 *
 * @param num_of_dimensions the number of search dimensions
 * @param swarm the spso swarm to use
 * @param objective_function the function used to measure fitness of each
 * @param epsilon the experimental error
 * @param not_moved_for the counter keeping track of how long since we last updated the global best
 */
void
spso_init_from_previous(int num_of_dimensions, spso_dimension_t ** dimensions,
			spso_swarm_t * swarm, spso_obj_fun_t objective_function,
			double epsilon, int (*stop) (void),
			spso_position_t * current_best_position,
			spso_fitness_t current_best_fitness,
			spso_fitness_t previous_best_fitness,
			spso_fitness_t previous_previous_best_fitness,
			int not_moved_for);

/**
 * Free memory allocated during initialisation.
 */
void spso_cleanup(void);

/**
 * Return the current global best position, also updating the supplied fitness
 * with the value of the fitness calculated at that position.  Likewise, the
 * two previous best fitnesses are supplied.
 */
spso_position_t *spso_get_global_best_history(spso_fitness_t * best,
					      spso_fitness_t * prev_best,
					      spso_fitness_t * prev_prev_best);

/**
 * Initialise the swarm by placing each particle at a random point within the
 * search space.
 */
void spso_initialise_swarm(spso_swarm_t * swarm);

/**
 * Compute a new velocity for particle and update the particle's current position.
 *
 * @param visits the number of times this position has been assessed before
 * @param particle the particle to compute a velocity for
 * @param known_positions the number of positions particles have been sent to so far
 */
int spso_compute_velocity(spso_particle_t * particle, int visits, int known_positions);

/**
 * Create a new spso_particle_t and initialise its position to a random point
 * within the search space.
 */
spso_particle_t *spso_new_particle(int uid);

spso_swarm_t *spso_new_swarm(int swarm_size);

spso_swarm_t *spso_get_swarm(void);

int spso_get_search_space_size(void);
spso_dimension_t **spso_get_search_space(void);

void spso_destroy_swarm(spso_swarm_t * swarm);

void spso_destroy_particle(spso_particle_t * particle);

spso_position_t *spso_new_position(void);

spso_particle_t *spso_get_particle(int id);

spso_particle_t *spso_update_particle(int particle_id, spso_fitness_t fitness, int visits, int known_positions);

/**
 * Set the flag that tells SPSO to stop searching and report the best
 * found position so far.  This does not take effect immediately as the MPI
 * task farm takes a while to report back all the results.
 */
void spso_stop(void);

/**
 * Indicate whether the stop flag has been set to true, eg by the stopping
 * criteria being met.
 */
bool spso_is_stopping(void);

/**
 * Loop through all the initialised particles and call the fitness function on
 * each one to start the search.
 */
void spso_start_search(void);

spso_dimension_t *spso_new_dimension(int uid, int min, int max, int value,
				     const char *name);


/**
 * Return the number of iterations with no movement so far.
 */
int spso_get_no_movement_counter(void);

#endif				/* include guard H_OPTSEARCH_SPSO_ */
