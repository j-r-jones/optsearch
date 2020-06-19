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

#include "spso.h"

typedef struct {
	spso_listener_f *listeners;
	int count;
} spso_listener_list;

static spso_position_t *spso_global_current_best;
static spso_fitness_t spso_global_current_best_fitness;
static spso_fitness_t spso_global_previous_best_fitness;
static spso_fitness_t spso_global_previous_previous_best_fitness;

static bool spso_stop_flag;

static int num_dims;		/* number of dimensions.  Would be better if a const, but we only know at runtime what this will be */
spso_dimension_t **spso_search_space;	/* this represents the search space */
spso_swarm_t *spso_swarm;
double epsilon;

const int spso_default_swarm_size = 40;

/* Listeners
 * TODO These should be better handled
 */
spso_obj_fun_t spso_fitness_function;
spso_listener_f stop_listener;
spso_listener_list global_best_update_listener;

void spso_update_global_best(spso_fitness_t fitness,
			     spso_position_t * position);

/* We failed to check if we are just not moving for a long time before.  This
 * counter is intended to fix that problem. */
static long no_movement_counter = 0;

/* The following two functions are here to facilitate listeners */
int spso_increment_no_movement_counter(void)
{
	no_movement_counter++;
	return no_movement_counter;
}

int spso_reset_no_movement_counter(void)
{
	no_movement_counter = 0;
	return no_movement_counter;
}

int spso_get_no_movement_counter(void)
{
	return no_movement_counter;
}

int
spso_register_listener(spso_listener_e listener_type, spso_listener_f listener)
{
	int rc = 0;
	switch (listener_type) {
	case SPSO_GLOBAL_BEST_UPDATE_LISTENER:
		if (global_best_update_listener.listeners == NULL) {
			global_best_update_listener.listeners = malloc(sizeof(spso_listener_f));
		} else {
			global_best_update_listener.listeners = realloc(global_best_update_listener.listeners, sizeof(spso_listener_f)*(global_best_update_listener.count+1));
		}
		global_best_update_listener.listeners[global_best_update_listener.count] = listener;
		global_best_update_listener.count++;
		break;
	case SPSO_PARTICLE_MOVE_LISTENER:
		log_error
		    ("NOT YET IMPLEMENTED: Cannot register SPSO_PARTICLE_MOVE_LISTENER");
		break;
	case SPSO_POSITION_FITNESS_LISTENER:
		log_error
		    ("NOT YET IMPLEMENTED: Cannot register SPSO_POSITION_FITNESS_LISTENER");
		break;
	case SPSO_STOP_LISTENER:
		log_error
		    ("NOT YET IMPLEMENTED: Cannot register SPSO_STOP_LISTENER");
		break;
	default:
		log_error("UNKNOWN listener type.  Cannot register listener.");
		rc = 1;
		break;
	}
	return rc;
}

int spso_notify_listeners(spso_listener_e listener_type)
{
	int rc = 0, i;
	switch (listener_type) {
	case SPSO_GLOBAL_BEST_UPDATE_LISTENER:
		log_debug("spso.c: Notifying global best update listeners");
		for (i=0; i<global_best_update_listener.count; i++) {
			global_best_update_listener.listeners[i]();
		}
		break;
	case SPSO_PARTICLE_MOVE_LISTENER:
		log_error
		    ("NOT YET IMPLEMENTED: Cannot notify SPSO_PARTICLE_MOVE_LISTENERs");
		break;
	case SPSO_POSITION_FITNESS_LISTENER:
		log_error
		    ("NOT YET IMPLEMENTED: Cannot notify SPSO_PPOSITION_FITNESS_LISTENERs");
		break;
	case SPSO_STOP_LISTENER:
		log_error
		    ("NOT YET IMPLEMENTED: Cannot notify SPSO_STOP_LISTENERs");
		break;
	default:
		log_error("UNKNOWN listener type.  Cannot notify listeners.");
		rc = 1;
		break;
	}
	return rc;
}

spso_position_t *spso_get_global_best_history(spso_fitness_t * best,
					      spso_fitness_t * prev_best,
					      spso_fitness_t * prev_prev_best)
{
	*best = spso_global_current_best_fitness;
	*prev_best = spso_global_previous_best_fitness;
	*prev_prev_best = spso_global_previous_previous_best_fitness;
	return spso_global_current_best;
}

void
spso_init_from_previous(int num_of_dimensions, spso_dimension_t ** dimensions,
			spso_swarm_t * swarm, spso_obj_fun_t objective_function,
			unsigned long long prng_iteration, double eps, int (*stop) (void),
			spso_position_t * current_best_position,
			spso_fitness_t current_best_fitness,
			spso_fitness_t previous_best_fitness,
			spso_fitness_t previous_previous_best_fitness,
			int not_moved_count)
{
	log_trace("Entered spso_init_from_previous");
	epsilon = eps;
	no_movement_counter = not_moved_count;
	/* Initialise PRNG */
	opt_rand_seed_t *seed = opt_rand_gen_seed();
	*(seed->seed) = 1294404794;	/* This is an arbitrary choice, originally used by Clerc, but as we are using a different PRNG we won't be reproducing any of his results (his search space was quite different too, as well as being Reals).  I use the same number because I needed to pick one to ensure reproducibility. It can be changed later or made random. */
	/* XXX TODO FIXME This always breaks when the iteration gets big.  SQLite3
	 * cannot handle the huge values, but OptSearch can.
	 * This is not going to be trivial to fix, and since our search is
	 * asynchronous we probably lose any advantage of doing this anyway.  Our
	 * results aren't really exactly reproducible already.
	 *
	log_trace("Initialising PRNG with iteration %llu, prng_iteration");
	opt_rand_init_naive(seed, prng_iteration);
	log_trace("Finished initialising PRNG");
	*/
	opt_rand_init_seed(seed);

	spso_stop_flag = false;
	spso_fitness_function = objective_function;

	global_best_update_listener.listeners = NULL;
	global_best_update_listener.count = 0;
	stop_listener = stop;

	if (num_of_dimensions > 0 && num_of_dimensions < spso_max_dims) {
		num_dims = num_of_dimensions;
	} else {
		/* TODO Handle more gracefully */
		log_error("ERROR TOO MANY DIMENSIONS.\n");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}

	spso_search_space = dimensions;
	if (current_best_position != NULL) {
		spso_global_current_best = current_best_position;
		spso_global_current_best_fitness = current_best_fitness;
		spso_global_previous_best_fitness = previous_best_fitness;
		spso_global_previous_previous_best_fitness =
		    previous_previous_best_fitness;
	} else {
		spso_global_current_best = spso_new_position();
		spso_global_current_best_fitness = DBL_MAX;
		spso_global_previous_best_fitness = DBL_MAX;
		spso_global_previous_previous_best_fitness = DBL_MAX;
	}

	spso_swarm = swarm;

	log_trace("Leaving spso_init_from_previous()");
}

void
spso_init(int num_of_dimensions, spso_dimension_t ** dimensions,
	  spso_obj_fun_t objective_function, double eps, int (*stop) (void))
{
	log_trace("Entered spso_init");
	epsilon = eps;
	/* Initialise PRNG */
	opt_rand_seed_t *seed = opt_rand_gen_seed();
	*(seed->seed) = 1294404794;	/* This is an arbitrary choice, originally used by Clerc, but as we are using a different PRNG we won't be reproducing any of his results (his search space was quite different too, as well as being Reals).  I use the same number because I needed to pick one to ensure reproducibility. It can be changed later or made random. */
	opt_rand_init_seed(seed);

	/* As we haven't started, we assume we won't want to stop yet */
	spso_stop_flag = false;

	spso_fitness_function = objective_function;

	global_best_update_listener.listeners = NULL;
	global_best_update_listener.count = 0;
	stop_listener = stop;

	log_debug("num_of_dimensions is %d", num_of_dimensions);

	if (num_of_dimensions > 0 && num_of_dimensions < spso_max_dims) {
		num_dims = num_of_dimensions;
	} else {
		/* TODO Handle more gracefully */
		log_error("ERROR TOO MANY DIMENSIONS.\n");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}
	spso_search_space = dimensions;
	spso_global_current_best = spso_new_position();
	spso_global_current_best_fitness = DBL_MAX;
	/* These two are initialised to -1 to try to get around the problem of
	 * converging prematurely if the very first run fails. */
	spso_global_previous_best_fitness = -1.0;
	spso_global_previous_previous_best_fitness = -1.0;

	/* This might be better as a function of the number of MPI ranks */
	spso_swarm = spso_new_swarm(num_dims + 1);
}

void spso_cleanup(void)
{
	log_trace("Entered spso_cleanup");

	/* Destroy swarm, particle by particle */
	spso_destroy_swarm(spso_swarm);
	if (global_best_update_listener.count != 0) {
		free(global_best_update_listener.listeners);
		global_best_update_listener.count = 0;
		global_best_update_listener.listeners = NULL;
	}
}

void spso_stop(void)
{
	log_trace("Entered spso_stop");
	spso_stop_flag = true;
	stop_listener();	/* Tell our listener that we are stopping */
}

/**
 * Check whether the stopping criteria have been met.  Set the spso_stop_flag
 * to true if so, and return the flag.  This check looks at the
 * amount of improvement between the last 4 global best fitnesses (including
 * this one if it is an improvement over the current global best).  If less
 * than 2(e/100)*fitness in each case, then we stop.  This is the "stop if not moving
 * much" approach.  "Not much" is two standard deviations.
 *
 * The spso_stop_flag is intended to allow us to stop cleanly if a signal is
 * received telling the optimiser to stop.
 *
 * @param fitness the fitness just received (a mean value)
 */
bool spso_should_stop(spso_fitness_t fitness, spso_position_t * position)
{
	/*
	 * Stopping criteria:
	 *
	 * In SPSO, Clerc suggests two possible stopping criteria:
	 *
	 * - Where the optimum point's fitness value is known, get within a 
	 * maximum admissible error of this.
	 *
	 * - Set a maximum number of fitness evaluations.  Clerc suggests one per 
	 * particle in the swarm, which seems a bit low unless your swarm is vast.
	 *
	 * - Getting bored is probably not a viable final solution, but it's a great
	 * place to start.  This would rely on waiting for the correct signal from
	 * SLURM (or similar) to quit, and reporting where we've got to before
	 * exiting cleanly.
	 *
	 * Keep records of how the PSO progresses (so at each new optimal point,
	 * log both the value and where it happened). 
	 * If one doesn't know the minimum, then in traditional optimisation there 
	 * are two kinds of stopping criteria:
	 * a) based on the value: stop when the "increases are small"
	 * b) based on where the minimum is: stop when "not moving much".
	 *
	 * Of course we have experimental error, which we know about, so we should
	 * certainly stop when progress < experimental error.
	 * I guess, to be sure, we need some criterion such as "three consecutive
	 * updates < e.e."
	 *
	 * In our case, e comes from the baselining that we did with the reference
	 * BLAS, which showed that e is much larger than we would have expected.
	 * This is set in the config as epsilon.
	 */
	double two_sigma;

	log_trace("Assessing fitness %.6e against current best of %.6e",
		  fitness, spso_global_current_best_fitness);

	if (spso_stop_flag) {
		log_debug("Stopping, so not checking for convergence.");
		return spso_stop_flag;
	}

	if (fitness > spso_global_current_best_fitness) {
		log_debug
		    ("Fitness %.6e is greater than global current best of %.6e",
		     fitness, spso_global_current_best_fitness);
		if (no_movement_counter >= NO_MOVEMENT_THRESHOLD) {
			/* We haven't moved for a very long time */
			spso_stop();
            log_info
                ("Decided that we have converged on current best fitness of %.6e after not moving for %d iterations",
                 spso_global_current_best_fitness, no_movement_counter);
		}
		spso_increment_no_movement_counter();
		return spso_stop_flag;
	}
	
	spso_reset_no_movement_counter();
	spso_update_global_best(fitness, position);
	/* fitness is a mean value. epsilon is a % of mean representing
	 * expected/acceptable standard deviation */
 	two_sigma = (fitness * (epsilon/100)) * 2.0;

	/* fabs() shouldn't be necessary because each improvement should be a
	 * smaller fitness value than the previous one */
	if (fabs(spso_global_previous_previous_best_fitness -
	     spso_global_previous_best_fitness) < two_sigma
	    && fabs(spso_global_previous_best_fitness -
		    spso_global_current_best_fitness) < two_sigma
	    && fabs(spso_global_current_best_fitness - fitness) < two_sigma) {
		/* Should we also check for this specific particle's movement?  I
		 * think that might just cause problems. */
		spso_stop();
		log_info
		    ("Decided that we have converged on current best fitness of %.6e",
		     fitness);
	}

	return spso_stop_flag;
}

bool spso_is_stopping(void)
{
	return spso_stop_flag;
}

/** These two constants are from Clerc's work on what happens PSO stagnation */
const double sigma = 1.193; /** AKA a bit beyond the current position */
const double omega = 0.721;

/**
 * Velocity update function (where velocity is really a displacement).  This
 * uses a geometric function, with a centre of gravity relative to the current
 * position of the particle. The definition from Clerc is as follows:
 * 
 * - let \f$x_{i}\f$ be the position of particle \f$i\f$ (so \f$x_{i}(t)\f$ is the position of
 *   particle \f$i\f$ at time \f$t\f$)
 * - Let \f$G_{i}\f$ be the centre of gravity of three points:
 *     - \f$x_{i}\f$
 *     - a point a "bit beyond" the previous best (relative to \f$x_{i}\f$) \f$l\f$
 *     - the current best position in this neighbourhood, \f$p\f$
 *   This makes \f$G_{i} = x_{i} + \sigma * ((p + l - 2*x_{i})/3)\f$
 * - Choose a random point \f$x'_{i}\f$ within the Hypersphere \f$H_{i}\f$ with centre \f$G_{i}\f$ and radius \f$abs(G_{i} - x_{i})\f$
 * - Calculate the new position:
 *      \f$x_{i}(t+1) = \omega * v_{i}(t) + x'_{i}\f$
 * - New velocity is:
 *      \f$v_{i}(t+1) = \omega * v_{i}(t) + x'_{i} - x_{i}(t)\f$
 *
 * Note that, to keep things simple, I am assuming a single neighbourhood
 * of all particles.  This might be called Global in the literature.
 *
 */
int spso_compute_velocity(spso_particle_t * x, int visits, int known_positions)
{
	int dim = 0;
	spso_particle_t *x_dash = spso_new_particle(-1);	/* A random point in the Hypersphere */
	spso_position_t *G = spso_new_position();	/* Centre of gravity, and of the Hypersphere */
	int H_radius = 0;		/* Radius of the Hypersphere */
	double sumsq = 0.0;
	double dsquare = 0.0;
	unsigned int count = 0;
	int max, min;
	double gtemp = 0.0, *xtemp, *vtemp, dampen;
	double mintemp, maxtemp;
	int max_visits = 0;

	log_trace
	    ("spso_compute_velocity: calculating new velocity and position for particle %d",
	     x->uid);

	if (visits > 0)
		log_debug("spso.c: This particle (%d) was at a position that had already been visited %d times", x->uid, visits);

	/* JHD's suggestion, rather than hard-coding a number (I was using 10).
	 * If we visit a point more than 2*max(1, #visits/#distinct points),
	 * so initially 3 (i.e. >2), then increasing as we get more repeated
	 * visits, you should make a random jump. I would probably suggest that
	 * this 'random' should take account of the discrete structure of the
	 * search space, e.g. change each value with probability 1/2.
	 * */
	if (visits > 0 && visits/known_positions > 1)
		max_visits = 2 * visits/known_positions;
	else
		max_visits = 2;

	if (visits > max_visits && opt_rand_int_range(0,1)) {
		log_warn("Just visited a frequently (>%d times) visited position.  Moving to a random point.", max_visits);
		for (dim = 0; dim < num_dims; dim++) {
			x->position.dimension[dim] = opt_rand_int_range(spso_search_space[dim]->min, spso_search_space[dim]->max);
			x->velocity.dimension[dim] = opt_rand_int_range(spso_search_space[dim]->min, spso_search_space[dim]->max);
		}
	} else {

		xtemp = calloc(num_dims, sizeof(double));
		vtemp = calloc(num_dims, sizeof(double));

		bool is_best = true;
		for (dim = 0; dim < num_dims; dim++) {
			if (x->previous_best.dimension[dim] != spso_global_current_best->dimension[dim]) {
				is_best = false;
				break;
			}
		}

		for (dim = 0; dim < num_dims; dim++) {
			/* Initialise xtemp and vtemp */
			xtemp[dim] = 0.0;
			vtemp[dim] = 0.0;
			/*
			 * Note that if l, p and x are the same, that is, the position of
			 * particle x and the current global and x's previous best positions,
			 * then G (gtemp) will be the position of x.  This will make sumsq zero and
			 * H_radius zero, and we will fail to move.
			 *
			 * There should probably be some special treatment when the positions
			 * of x and G are the same.  Perhaps a random G ought to be picked
			 * then?  Or we could revert to simply moving x randomly.
			 */
			/* gtemp is a double, because sigma is a double and because an int
			 * could overflow here.  We can't use a long because of sigma; we
			 * would lose bits. */
			if (is_best) {
				/* Equation 3.13 in Clerc's publication */
				gtemp = (double)x->position.dimension[dim]
					+ (sigma *
					 ((double)x->previous_best.dimension[dim]
					  - (double)x->position.dimension[dim]) / 2.0);
			} else {
				/* Equation 3.10 in Clerc's publication */
				gtemp = (double)x->position.dimension[dim]
					+ (sigma *
					   (((double)spso_global_current_best->dimension[dim]
						 + (double)x->previous_best.dimension[dim]
						 - (2.0 * (double)x->position.dimension[dim])) / 3.0));
			}
			if (fpclassify(gtemp) != FP_NORMAL && fpclassify(gtemp) != FP_ZERO) {
				log_error("spso.c particle %d, dim %d: gtemp is not normal or non-zero!", x->uid, dim);
			}
			/* Check that gtemp is within bounds for this dimension.  Adjust
			 * if not. */
			if (gtemp > (double)spso_search_space[dim]->max) {
				log_warn("dim: %d. gtemp fell out of the search space (above max)", dim);
				gtemp = (double) spso_search_space[dim]->max;
			} else if (gtemp < (double)spso_search_space[dim]->min) {
				log_warn("dim: %d. gtemp fell out of the search space (below min)", dim);
				gtemp = (double) spso_search_space[dim]->min;
			}
			sumsq +=
				((gtemp - (double)x->position.dimension[dim]) *
				(gtemp - (double)x->position.dimension[dim]));
			log_debug
				("dim: %d. gtemp is: %.6e; sumsq is: %.6e; x is %d (%.2e)",
				 dim, gtemp, sumsq, x->position.dimension[dim],
				 (double)x->position.dimension[dim]);
			G->dimension[dim]= (int)gtemp;
		}

		/* H_radius could mean that, if G is on the boundary in any dimension, our
		 * hypersphere could extend beyond the search space. To deal with this, we
		 * clamp the search for x_dash later to be within the intersection of the
		 * search space and the hypersphere H. */
		H_radius = (int)sqrt(sumsq);

		log_debug
			("spso_compute_velocity: For particle %d, H_radius is %d (sumsq is %.6e)",
			 x->uid, H_radius, sumsq);

		/* Choose a suitable x_dash (x') */
		//do {
		/* TODO FIXME WE SEEM TO GET STUCK IN THIS LOOP FOREVER */
		/* - Use a hypercube instead for guessing a point in H, then check it
		 *  really is in H and guess a new one until we have one that is in H --
		 *  this is naive and inefficient but easy to do:
		 */
		dsquare = 0.0;
		for (dim = 0; dim < num_dims; dim++) {
			maxtemp = (double) G->dimension[dim] + (double) H_radius;
			mintemp = (double) G->dimension[dim] - (double) H_radius;
			if (maxtemp > spso_search_space[dim]->max) {
				max = spso_search_space[dim]->max;
			} else {
				max = (int) maxtemp;
			}
			if (mintemp < spso_search_space[dim]->min) {
				min = spso_search_space[dim]->min;
			} else {
				min = (int) mintemp;
			}
			x_dash->position.dimension[dim] = opt_rand_int_range(min, max);
			dsquare +=
				pow(((double)x_dash->position.dimension[dim]) -
				((double)G->dimension[dim]), 2.0);
		}
		log_trace("dsquare is %.6e (sumsq is %.6e)", dsquare, sumsq);
		//} while (dsquare > sumsq);    /* if we overshoot hypersphere, try again  */

		for (dim = 0; dim < num_dims; dim++) {

			/* Now Calculate the new velocity (Equation 3.11 in Clerc) */
			/* v(t+1) = omega * v(t) + x_dash - x; */
			vtemp[dim] =
				(omega * (double)x->velocity.dimension[dim]) +
				(double)x_dash->position.dimension[dim] -
				(double)x->position.dimension[dim];

			log_trace
				("spso.c: Particle %d: Before recalculating, velocity is %d and coordinate is %d (dimension is %d)",
				 x->uid, x->velocity.dimension[dim],
				 x->position.dimension[dim], dim);

			/* - Now calculate the new position x: */
			xtemp[dim] =
				(omega * (double)x->velocity.dimension[dim]) +
				(double)x_dash->position.dimension[dim];

			/*
			 * Check bounds.  Use periodic boundary conditions to keep particle
			 * within the space.
			 *
			 * https://en.wikipedia.org/wiki/Periodic_boundary_conditions
			 *
			 * Idea stolen from molecular dynamics.
			 */
			dampen = opt_rand_double() / (((double)INT_MAX) + 1.0);	/* JHD's suggestion to get within [0,1) */
			log_debug("spso.c: Particle %d: Using dampening factor %lf",
				  x->uid, dampen);
			count = 0;
			while ((double)spso_search_space[dim]->max < xtemp[dim]
				   || ((double)spso_search_space[dim]->min > xtemp[dim])) {
				/* This is arbitrary */
				if (count > 64) {
					log_warn("Looping forever seems likely, using random values instead.");
					xtemp[dim] = (double) opt_rand_int_range(spso_search_space[dim]->min, spso_search_space[dim]->max);
					vtemp[dim] = (double) opt_rand_int_range(spso_search_space[dim]->min, spso_search_space[dim]->max);
					break;
				}
				count++;
				if ((double)spso_search_space[dim]->max < xtemp[dim]) {
					xtemp[dim] = ((double)spso_search_space[dim]->min)
						+ fmod(xtemp[dim] - ((double)spso_search_space[dim]->max), ((double)spso_search_space[dim]->max)
								- ((double)spso_search_space[dim]->min));
				} else if ((double)spso_search_space[dim]->min > xtemp[dim]) {
					xtemp[dim] = (double)spso_search_space[dim]->max
						- fmod((double)spso_search_space[dim]->min
								- xtemp[dim], (double)spso_search_space[dim]->max) - ((double)spso_search_space[dim]->min);
				}
				/* Apply the dampening factor */
				xtemp[dim] = xtemp[dim] * dampen;
				/* Should we be dampening the displacement too, and should this
				 * really apply to all dimensions?  I cannot see it working very
				 * well if it was applied that way. */
				vtemp[dim] = xtemp[dim] - (double)x->position.dimension[dim];
			}
			log_debug
				("spso.c: Particle %d struck Hypersphere surface %u times",
				 x->uid, count);

		/* Finally, convert back to int */
		x->position.dimension[dim] = (int)xtemp[dim];
		x->velocity.dimension[dim] = (int)vtemp[dim];
		log_trace("spso.c: Particle %d: xtemp: %.2e, vtemp: %.2e",
			  x->uid, xtemp[dim], vtemp[dim]);
		log_debug
			("spso.c: Particle %d: Dimension with uid %d (%s) value changed to to %d and velocity to %d (dimension min: %d, max: %d)",
			 x->uid,
			 spso_search_space[dim]->uid,
			 spso_search_space[dim]->name,
			 x->position.dimension[dim],
			 x->velocity.dimension[dim],
			 spso_search_space[dim]->min,
			 spso_search_space[dim]->max);
		}

		/* Clean up */
		spso_destroy_particle(x_dash);
		free(xtemp);
		free(vtemp);
	}


	return 1;
}

/**
 * Create a new spso_position_t object, but do not initialise it.
 */
spso_position_t *spso_new_position()
{
	int dim, value;
	/* Should we initialise the position at all? */
	spso_position_t *position =
	    (spso_position_t *) malloc(sizeof(*position));
	position->dimension = malloc(sizeof(*position->dimension) * num_dims);
	for (dim = 0; dim < num_dims; dim++) {
		/* start at a random point*/
		value = opt_rand_int_range(spso_search_space[dim]->min, spso_search_space[dim]->max);
		position->dimension[dim] = value;
	}
	return position;
}

/**
 * Destroy (free) the spso_position_t provided.
 */
void spso_destroy_position(spso_position_t * position)
{
	free(position->dimension);
	position->dimension = NULL;
	free(position);
}

/**
 * Destroy (free) the spso_particle_t provided.
 */
void spso_destroy_particle(spso_particle_t * particle)
{
	/* This causes a segfault :(
	 * because somewhere else, particle->position has already been free'd
	 *
	free(particle);
	*/
}

void spso_initialise_particle(spso_particle_t * particle)
{
	int i;
	int *previous = particle->previous_best.dimension;
	int *velocity = particle->velocity.dimension;

	log_trace("spso_initialise_particle(): Initialising particle %d",
		  particle->uid);

	particle->previous_best_fitness = DBL_MAX;

	for (i = 0; i < num_dims; i++) {
		particle->position.dimension[i] = opt_rand_int_range(spso_search_space[i]->min, spso_search_space[i]->max);
		previous[i] = opt_rand_int_range(spso_search_space[i]->min, spso_search_space[i]->max);
		velocity[i] = 0; /* Suggested to avoid premature convergence, but shown to not always work */
	}
}

/**
 * Create a new spso_particle_t object and initialise it to be at a random
 * point in the search space.
 */
spso_particle_t *spso_new_particle(int uid)
{
	spso_particle_t *particle =
	    (spso_particle_t *) malloc(sizeof(*particle));
	particle->uid = uid;
	particle->velocity.dimension = malloc(sizeof(*particle->velocity.dimension) * num_dims);
	particle->position.dimension = malloc(sizeof(*particle->position.dimension) * num_dims);
	particle->previous_best.dimension = malloc(sizeof(*particle->previous_best.dimension) * num_dims);
	spso_initialise_particle(particle);
	return particle;
}

spso_swarm_t *spso_get_swarm()
{
	return spso_swarm;
}

/**
 * Return a pointer to the spso_particle_t identified by the provided UID.
 *
 * @param uid the UID of the particle
 * @return NULL if the UID does not correspond to a particle in the swarm
 */
spso_particle_t *spso_get_particle(int uid)
{
	/* These are just array indices so we index our particles from 0 */
	if (uid >= spso_swarm->size || uid < 0) {
		/* Out of bounds */
		log_error("No particle with uid %d in swarm", uid);
		return NULL;
	}
	return spso_swarm->particles[uid];
}

void spso_update_global_best(spso_fitness_t fitness, spso_position_t * position)
{
	int dim;
	/*
	 * TODO Neighbourhood update
	 *
	 * Updating the global best is complicated by the neighbourhood update
	 * mechanism, by the fact that different fitness function calls will
	 * take different amounts of time to return, and we may end up with a
	 * race condition if we are not careful in how particles are
	 * informed/updated when the current global best changes.
	 *
	 * At the moment, the neighbourhood is the entire swarm, so setting the
	 * global best is as good as it gets.
	 *
	 * What if this position is equivalent?
	 */
	if (position == NULL) {
		log_error("Can't update if position is null");
	}
	if (spso_global_current_best_fitness > fitness) {
		log_debug("Updating global best fitness to %lf", fitness);
		spso_global_previous_previous_best_fitness =
		    spso_global_previous_best_fitness;
		spso_global_previous_best_fitness =
		    spso_global_current_best_fitness;
		spso_global_current_best_fitness = fitness;
		for (dim = 0; dim < num_dims; dim++) {
			spso_global_current_best->dimension[dim] = position->dimension[dim];
		}
		spso_notify_listeners(SPSO_GLOBAL_BEST_UPDATE_LISTENER);
	}
}

spso_particle_t *spso_update_particle(int particle_id, spso_fitness_t fitness, int visits, int known_positions)
{
	int dim;
	spso_particle_t *particle = spso_get_particle(particle_id);
	log_trace("spso_update_particle(): Updating particle %d", particle_id);
	if (particle != NULL && particle->uid >= 0) {
		if (particle->previous_best_fitness > fitness) {
			particle->previous_best_fitness = fitness;
			for (dim = 0; dim < num_dims; dim++) {
				particle->previous_best.dimension[dim] =
				    particle->position.dimension[dim];
			}
		}
		if (!spso_should_stop(fitness, &particle->position)) {
			/* Move the particle to the next point */
			spso_compute_velocity(particle, visits, known_positions);
			log_trace("spso.c: Adding particle %d to fitness queue",
				  particle->uid);
			spso_fitness_function(particle->uid);
		}
	}
	log_trace("spso.c: Finished updating particle %d", particle->uid);
	return particle;
}

spso_swarm_t *spso_new_swarm(int swarm_size)
{
	int i;
	spso_swarm_t *swarm = malloc(sizeof(*swarm));
	log_trace("spso_new_swarm: Creating swarm of size %d", swarm_size);
	swarm->particles = malloc(sizeof(*(swarm->particles)) * swarm_size);
	swarm->size = swarm_size;
	for (i = 0; i < swarm->size; i++) {
		swarm->particles[i] = spso_new_particle(i);
	}
	return swarm;
}

void spso_destroy_swarm(spso_swarm_t * swarm)
{
	int i;
	for (i = 0; i < swarm->size; i++) {
		spso_destroy_particle(swarm->particles[i]);
		swarm->particles[i] = NULL;
	}
	//free(swarm);
}

void spso_start_search(void)
{
	int i;
	log_trace("spso: starting search.");
	for (i = 0; i < spso_swarm->size; i++) {
		spso_fitness_function(spso_swarm->particles[i]->uid);
	}
}

spso_dimension_t *spso_new_dimension(int uid, int min, int max, int value,
				     const char *name)
{
	spso_dimension_t *dim = malloc(sizeof(*dim));
	dim->uid = uid;
	dim->max = max;
	dim->min = min;
	if (name != NULL)
		dim->name = strdup(name);
	else {
		log_warn("No name supplied. Creating dimension with empty name");
		dim->name = strdup("");
	}
	return dim;
}

int spso_get_search_space_size(void)
{
	return num_dims;
}

spso_dimension_t **spso_get_search_space(void)
{
	return spso_search_space;
}
