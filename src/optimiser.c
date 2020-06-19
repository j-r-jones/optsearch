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

#include "optimiser.h"

int opt_report_fitness(const int uid, double fitness, int visits);

/* TODO This should be set by the user in the config file */
#define OPT_DB_NAME "optsearch.sqlite"

opt_config_t *opt_config;
opt_dimension_t **search_space;
int search_space_size;

/* This is a clumsy way to avoid calling opt_stop_search twice */
bool already_stopped = false;

/*
 * TODO
 * - Handle period state recording (similar to checkpointing)
 */

void opt_clean_up(void)
{
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	opt_task_clean_up();
	if (rank == MASTER) {
		spso_cleanup();
	}
}

void opt_sig_handler(int signo, siginfo_t *sinfo, void *context)
{
	log_info("Received signal %d, stopping search.", signo);
	opt_stop_search();
}

bool opt_check_flag(opt_flag_t * flag)
{
	/* 
	 * Check a single flag, and return true if it is okay to use as part of
	 * the search space.
	 *
	 * There are two problems we face:
	 *   - Some ranges have invalid max values, and we need to do a search to
	 *     find the real max.
	 *   - Need to discard flags that fail instantly for some reason.
	 *
	 * Should be fine to just do a quick:
	 *  retval = run_command(command, &time, timeout);
	 * with the build script.  This will be less language-specific than
	 * compiling a "Hello, World!" program, but is liable to be a lot slower.
	 * If too slow, we 
	 *
	 * Timeout can probably be something like 20 seconds at most with "Hello,
	 * World!", but who knows with the build_script, where we will be building
	 * an entire library.
	 */
	char *flag_string = NULL;
	char *command = NULL;
	int size, retval;
    bool rt = true;
	int timeout = opt_config->timeout;
	double time = 0.0;
    const char *version_check_format = "%s --version";
	const char *format = "%s %s helloworld.f -o helloworld";	/* TODO This assumes C compiler */
	const char *clean = "rm -f helloworld";	/* TODO This assumes *nix */
	int value;
	opt_dimension_t *dim = opt_convert_flag(flag);
	dim->uid = flag->uid;
	value = (dim->max - dim->min) / 2 + dim->min;	/* Pop us in the middle */
	flag_string = convert_dimension_to_string(dim, value);

	if (!strcmp("", flag_string)) {
		free(dim);
		free(flag_string);
		return false;
	}
	log_trace
	    ("optimiser.c: Cleaning before compiling helloworld command to test flag");

	retval = run_command(clean, &time, timeout);

	log_trace("optimiser.c: Compiling helloworld to test flag");

    /*
     * Make sure we're using the right compiler version.
     *
     * TODO This should check the output against the version in the config
     * file, and error if they do not match.
     */
	size =
	    strlen(opt_config->compiler) + strlen(version_check_format) + 1;
	command = realloc(command, size);
	sprintf(command, version_check_format, opt_config->compiler);
	log_debug("optimiser.c: Generated command is: '%s'", command);
	retval = run_command(command, &time, timeout);

	size =
	    strlen(opt_config->compiler) + strlen(format) + strlen(flag_string) + 1;
	command = realloc(command, size);
	sprintf(command, format, opt_config->compiler, flag_string);
	log_debug("optimiser.c: Generated command is: '%s'", command);
	retval = run_command(command, &time, timeout);

	/* TODO This doesn't seem to be correctly determining whether or not we
	 * have exited correctly */
	if (retval == 1) {
		/* FAILURE */
		/* Discard this flag, do not use it to make a dimension that we will
		 * actually use. */
        log_debug("optimiser.c: Flag (%s) does NOT seem to work and should be discarded", flag_string);
		rt = false;
	} else {
        log_debug("optimiser.c: Flag (%s) seems to be okay", flag_string);
        rt = true;
    }
	free(dim);
	free(flag_string);
	free(command);
	return rt;
}

void opt_start_search(opt_config_t * opt_config)
{
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	opt_init(opt_config);

	/* This populates the work queue initially */
	if (MASTER == rank) {
		spso_start_search();
	}
	/* This should not return while the search is on-going, so we don't need a
	 * loop. */
	opt_task_start();

	opt_stop_search();
}

int opt_checkpoint(void)
{
	spso_fitness_t fitness = 0.0;
	spso_fitness_t prev_fitness = 0.0;
	spso_fitness_t prev_prev_fitness = 0.0;
	spso_position_t *pos = NULL;
	int no_moves_count = 0;
	unsigned long long prng_iter = 0;
	char *flags = NULL;

	no_moves_count = spso_get_no_movement_counter();

	pos = spso_get_global_best_history(&fitness, &prev_fitness,
					 &prev_prev_fitness);
	log_debug
	    ("optimiser.c: Got fitnesses:\nBest:\n%.6e;\nPrevious best:\n%.6e;\nPrevious-previous best:\n%.6e\n",
	     fitness, prev_fitness, prev_prev_fitness);
		flags = opt_position_to_string(pos);
		log_debug("optimiser.c: Got flags: '%s'", flags);

	if (pos == NULL) {
		log_debug("optimiser.c: Current best position is NULL");
	}

	/* Record in database */
	opt_db_store_prev_prev_best(prev_prev_fitness);
	opt_db_store_prev_best(prev_fitness);
	opt_db_store_curr_best(pos, fitness);
	opt_db_store_no_move_counter(no_moves_count);

	prng_iter = opt_rand_get_iteration();
	log_trace("Storing PRNG_ITER %llu", prng_iter);
	opt_db_store_prng_iter(prng_iter);

	/*
	 * This causes SQLite to segfault internally.  The call shouldn't be
	 * necessary really, since all the particles have had their positions
	 * stored each time they are updated.  Hence it is easiest to leave it
	 * out.
	 *
	swarm = spso_get_swarm();
	opt_db_store_swarm(swarm);
	*/

	if (flags != NULL) {
		log_info("Best answer so far:\n\tFitness: %.6e\n\tFlags:\n%s\n", fitness,
			 flags);
		printf("Best answer so far:\n\tFitness: %.6e\n\tFlags:\n%s\n", fitness,
			   flags);
		fflush(stderr);
		fflush(stdout);
		free(flags);
		flags = NULL;
	} else {
		log_error("Didn't get any flags for the current best position.");
	}
	return 0;
}

void opt_stop_search(void)
{
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	log_trace("optimiser.c: Stopping the search (optimiser.c)");

	opt_task_stop();

	if (MASTER != rank)
		return;

	if (!already_stopped) {
		already_stopped = true;
		spso_stop();
	}

	opt_checkpoint();

	opt_clean_up();
}


void opt_add_to_fitness_queue(int particle_uid)
{
	char *flag_string = NULL;
	spso_particle_t *particle = NULL;
	int rank, rc;
	int position_id = -1;
	int visits = 0;
	double fitness = 0.0;
	log_trace
	    ("optimiser.c: Entered opt_add_to_fitness_queue with particle UID %d",
	     particle_uid);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if (MASTER != rank) {
		log_fatal
		    ("opt_add_to_fitness_queue() should only be called by the master rank!");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}

	if (already_stopped) {
		log_debug("Ignoring request to add particle to fitness queue, as we are stopping.");
		return;
	}

	log_trace("optimiser.c: Adding particle UID %d to fitness queue.",
		  particle_uid);
	particle = spso_get_particle(particle_uid);
	if (particle == NULL) {
		log_error
		    ("Optimiser.c: Received null particle when attempting to add next search position task farm queue.");
		return;
	}

	/* Check if position is in the database already.  If so, just report
	 * the already recorded fitness to SPSO rather than adding it to the queue
	 * for evaluation. */
	rc = opt_db_find_position(&position_id, &particle->position);
	if (!rc && position_id >= 0) {
		/* This position is already in the database.  We'll assume that
		 * another particle is worrying about it. */
		opt_db_get_position_fitness(position_id, &fitness);
		opt_db_get_position_visits(position_id, &visits);
		/*
		 * If we're resuming from a previous run, then either
		 * the value in the DB will be NULL, which always
		 * comes back as 0, or it may be DBL_MAX.  In this case, the number of
		 * times the position has been visited should not exceed 1, although
		 * it is possible for more than one particle to be initialised to the
		 * same spot.
		 *
		 * The isnormal() check should deal with the fitness == 0.0 case for us.
		 */
		if (visits > 1 && isnormal(fitness) && fitness < DBL_MAX) {
			/* As a side effect, updating the particle in the DB should increment
			 * the position counter for us when the fitness is reported back. */
			opt_report_fitness(particle_uid, fitness, visits);
			return;
		}
	}
	/* 
	 * Record results in database
	 *
	 * This happens when the particle reports a fitness result for the
	 * position it is moved to, so is probably unnecessary.  Calling it here
	 * too ends in duplication in the particle_history table.
    opt_db_update_particle(particle);
	 */

	/*
	 * Convert position to a command and add it to the task farm queue
	 */
	flag_string = build_compiler_options(particle);
	opt_queue_push(particle_uid, flag_string);
}

int opt_report_fitness(const int uid, double fitness, int visits)
{
	int rank, pos_id, known_positions = 0;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	log_trace("optimiser.c: Optimiser.c: Received fitness %e for particle %d",
	     fitness, uid);
	if (MASTER != rank) {
		log_fatal
		    ("opt_report_fitness() should only be called by the master rank!");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}

	if (already_stopped) {
		log_debug("Ignoring reported fitness as we are stopping.");
		return 0;
	}

	opt_db_get_position_count(&known_positions);

	/* 
	 * Report result back to SPSO for the relevant particle, so it can update
	 * the velocity, etc.  It should as a side-effect call
	 * opt_add_to_fitness_queue.
	 */
	spso_particle_t *particle = spso_update_particle(uid, fitness, visits, known_positions);

	if (particle == NULL) {
		log_error
		    ("Optimiser.c: Received null particle when attempting to update with fitness information.");
		return 0;
	}

	/* 
	 * Record results in database
	 */
	opt_db_update_particle(particle);
	opt_db_store_position(&pos_id, &particle->position);
	opt_db_update_position_fitness(pos_id, fitness);
	
    opt_checkpoint();

	return 1;
}

/* TODO
 * - First pass: Attempt to make search space as small as possible.
 *      - Cut down number of dimensions to search in via hill climbing?
 *      This could be for those that have dependencies, so we can turn several
 *      flags into one on/off flag for PSO.
 *      - Perhaps a binary search for range flags, to find the best value?
 */
void opt_init(opt_config_t * conf)
{
	int i, rc, rank;
	int count = 0;
	int no_move_count = 0;
	spso_swarm_t *swarm;
	spso_dimension_t *dim = NULL;
	unsigned long long  prng_iter;
	int signum = 9;
    struct sigaction act;
	spso_position_t *curr_best_position = NULL;
	spso_fitness_t curr_best_fitness, prev_best_fitness,
	    prev_prev_best_fitness;

	if (conf == NULL) {
		log_fatal("Cannot initialise optimiser from a NULL config");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}
	opt_config = conf;
	
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	if (rank == MASTER) {
		search_space = NULL;
		search_space_size = 0;
		count = 0;
		/* Create dimensions from the flags */
		for (i = 0; i < opt_config->num_flags; i++) {
			/* Only add flags that work */
			/* TODO We will be repeating this if we start up from a previous run,
			 * but hopefully it does not take too long and always produces the
			 * same result.  Otherwise we might fail with SQL errors and/or
			 * segfaults.
			 */
			/* TODO Check that ranges are valid? Might be best to make a second
			 * tool to do this. */
            // TODO Commented out this IF for now as we are not using the right
            // compiler version in our tests.
			//if (opt_check_flag(opt_config->compiler_flags[i])) {
				search_space_size++;
				dim = opt_convert_flag(opt_config->compiler_flags[i]);
				log_trace
					("optimiser.c: Dimension name pointer is at %p",
					 dim->name);
				search_space =
					realloc(search_space,
						sizeof(spso_dimension_t *) *
						search_space_size);
				search_space[count] = dim;
				log_trace
					("optimiser.c: Dimension name pointer is at %p",
					 search_space[count]->name);
				log_debug
					("optimiser.c: %d: Converted dimension %d (%s)",
					 count, search_space[count]->uid,
					 search_space[count]->name);
				count++;
			//}
			/* TODO search_space is likely to occupy a lot more memory than is
			 * actually being used.  Realloc might be more appropriate.*/
		}
		log_info("optimiser.c: From %d flags, now have %d dimensions",
			  opt_config->num_flags, search_space_size);
	}

	opt_task_initialise(opt_config, &opt_report_fitness);

	log_info("optimiser.c: Got quit signal: '%s'", opt_config->quit_signal);

	signum = opt_get_signum(opt_config->quit_signal);
	memset(&act, 0, sizeof(struct sigaction));
	sigemptyset(&act.sa_mask);
	act.sa_sigaction = opt_sig_handler;
	act.sa_flags = SA_SIGINFO;

    if (-1 == sigaction(signum, &act, NULL)) {
        log_fatal("Unable to register signal handler for %s with sigaction()", opt_config->quit_signal);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
	/* Workaround because SLURM is inconsistent in obeying --signal to sbatch,
	 * but is hard-coded to send SIGCONT before SIGTERM */
    if (-1 == sigaction(SIGCONT, &act, NULL)) {
        /* This probably ought to be printed by log_fatal */
        log_warn("Unable to register signal handler for SIGCONT with sigaction()");
    }
    if (-1 == sigaction(SIGINT, &act, NULL)) {
        /* This probably ought to be printed by log_fatal */
        log_warn("Unable to register signal handler SIGINT with sigaction()");
    }

	if (MASTER == rank) {
		log_debug("search_space_size (number of dimensions) is %d", search_space_size);
		if (search_space_size == 0) {
			log_error("Search space has no dimensions.  The most likely cause is not being able to test the compiler flags are valid.  Is the helloworld.f missing?");
			MPI_Abort(MPI_COMM_WORLD, -1);
		}
		rc = opt_db_init(OPT_DB_NAME, opt_config, search_space_size, search_space);
		if (rc == OPT_DB_NEW) {
			spso_init(search_space_size, search_space,
				  &opt_add_to_fitness_queue, opt_config->epsilon,
				  &opt_task_stop);
		} else if (rc == OPT_DB_RESUME) {
			/* Retrieve data from the database and use to initialise spso and the
			 * prng */
			swarm = opt_db_get_swarm(search_space_size, search_space);
			opt_db_get_prng_iter(&prng_iter);
			opt_db_get_prev_prev_best(&prev_prev_best_fitness);
			opt_db_get_prev_best(&prev_best_fitness);
			curr_best_position =
				opt_db_new_position(search_space_size, search_space);
			opt_db_get_curr_best(curr_best_position, &curr_best_fitness);

			opt_db_get_no_move_counter(&no_move_count);

			spso_init_from_previous(search_space_size, search_space, swarm,
						&opt_add_to_fitness_queue, prng_iter,
						opt_config->epsilon, &opt_task_stop,
						curr_best_position, curr_best_fitness,
						prev_best_fitness,
						prev_prev_best_fitness, no_move_count);
		} else {
			log_fatal
				("Error initialising database.  Perhaps the file exists, but is not writeable?");
			MPI_Abort(MPI_COMM_WORLD, rc);
		}

		spso_register_listener(SPSO_GLOBAL_BEST_UPDATE_LISTENER, &opt_checkpoint);
	}

	log_trace("optimiser.c: Finished opt_init");
}

void opt_destroy_dimension(spso_dimension_t * dim)
{
	if (dim == NULL)
		return;
	if (dim->name != NULL) {
		free(dim->name);
		dim->name = NULL;
	}
	free(dim);
}

opt_dimension_t *opt_convert_flag(opt_flag_t * flag)
{
	opt_dimension_t *dim = NULL;
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if (MASTER != rank) {
		log_fatal
		    ("opt_convert_flag() should only be called by the master rank!");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}

	if (flag == NULL) {
		log_error("Cannot convert NULL flag to dimension");
		return NULL;
	}

	dim = malloc(sizeof(*dim));
	if (dim == NULL) {
		log_fatal
		    ("Unable to allocate memory to store dimensions for SPSO.");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}

	log_trace("optimiser.c: Creating dimension for flag with uid '%d' (%s)",
		  flag->uid, flag->name);

	dim->name = strdup(flag->name);
	dim->uid = flag->uid;

	switch (flag->type) {
	case OPT_RANGE_FLAG:
		dim->min = flag->data.range.min;
		dim->max = flag->data.range.max + 1;
		break;
	case OPT_LIST_FLAG:
		dim->min = 0;
		dim->max = flag->data.list.size;
		break;
	case OPT_ONOFF_FLAG:
		dim->min = 0;
		dim->max = 1 + 1;
		break;
	default:
		log_error
		    ("Unrecognised flag type, could not convert to a dimension for the search space.");
		opt_destroy_dimension(dim);
		return NULL;
	}

	log_trace
	    ("optimiser.c: Returning dimension with UID %d (name %s) for flag with uid '%d' (%s)",
	     dim->uid, dim->name, flag->uid, flag->name);
	log_trace("optimiser.c: Dimension name pointer is at %p", dim->name);
	return dim;
}

opt_flag_t *opt_get_flag(int flag_uid)
{
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if (MASTER != rank) {
		log_fatal
		    ("opt_get_flag() should only be called by the master rank!");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}
	log_trace("optimiser.c: Getting flag with uid %d", flag_uid);
	if (opt_config == NULL) {
		log_error("Attempted to get flag from NULL opt_config.");
		return NULL;
	}
	if (flag_uid < 0 || flag_uid > opt_config->num_flags) {
		log_error("Flag UID '%d' is out of range.", flag_uid);
		return NULL;
	}
	return opt_config->compiler_flags[flag_uid];
}

char *convert_dimension_to_string(opt_dimension_t * dim, int value)
{
	/* this might be gcc specific due to the flag_format */
	const char *EMPTY = "";
	char *string = NULL;
	int flag_size = 0;
	char *flag_format;
	int extra_chars;	/* The additional number of chars added by the formatting */
	opt_flag_t *flag = NULL;

	log_trace("optimiser.c: Converting dimension with UID %d and value %d",
		  dim->uid, value);
	flag = opt_get_flag(dim->uid);

	if (flag == NULL) {
		log_error
		    ("Unrecognised flag UID, could not convert dimension to corresponding compiler flag.");
		return NULL;
	}

	log_debug
	    ("optimiser.c: flag->uid: %d, flag->name: %s, flag->prefix: %s",
	     flag->uid, flag->name, flag->prefix);

	switch (flag->type) {
	case OPT_RANGE_FLAG:
		if (value > flag->data.range.max
		    || value < flag->data.range.min) {
			log_debug
			    ("optimiser.c: dim value (%d) is out of range",
			     value);
			string = strdup(EMPTY);
			break;
		}
		if (flag->data.range.separator == NULL) {
			log_error
			    ("Check your YAML.  This flag is invalid (%s) as it has no separator.",
			     flag->name);
			string = strdup(EMPTY);
			break;
		}
		flag_format = "%s%s%s%d";
		extra_chars = strlen(flag_format);
		flag_size =
		    strlen(flag->name) + strlen(flag->prefix) +
		    strlen(flag->data.range.separator) + CHAR_INT_MAX;
		string = realloc(string, flag_size + extra_chars + 1);	/* +1 for NULL char */
		sprintf(string, flag_format, flag->prefix, flag->name,
			flag->data.range.separator, value);
		break;
	case OPT_LIST_FLAG:
		if (value >= flag->data.list.size || value < 0) {
			log_debug
			    ("optimiser.c: dim->value (%d) is out of range",
			     value);
			string = strdup(EMPTY);
			break;
		}
		flag_format = "%s%s%s%s";
		extra_chars = strlen(flag_format);
		flag_size =
		    strlen(flag->name) + strlen(flag->prefix) +
		    strlen(flag->data.list.separator) +
		    strlen(flag->data.list.values[value]);
		string = realloc(string, flag_size + extra_chars + 1);	/* +1 for NULL char */
		sprintf(string, flag_format, flag->prefix, flag->name,
			flag->data.list.separator,
			flag->data.list.values[value]);
		break;
	case OPT_ONOFF_FLAG:
		flag_format = "%s%s";
		extra_chars = strlen(flag_format);
		/* TODO Need to check if prefix is bigger than neg_prefix, but
		 * with GCC it isn't. Also the format might be a bit different */
		if (value == 1) {
			flag_size = strlen(flag->name) + strlen(flag->prefix);
			string = realloc(string, flag_size + extra_chars + 1);	/* +1 for NULL char */
			sprintf(string, flag_format, flag->prefix, flag->name);
		} else if (value == 0) {
			flag_size =
			    strlen(flag->name) +
			    strlen(flag->data.onoff.neg_prefix);
			string = realloc(string, flag_size + extra_chars + 1);	/* +1 for NULL char */
			sprintf(string, flag_format,
				flag->data.onoff.neg_prefix, flag->name);
		} else {
			log_debug
			    ("optimiser.c: dim->value (%d) is out of range",
			     value);
			string = strdup(EMPTY);
		}
		break;
	default:
		log_error
		    ("Unrecognised flag type, could not convert dimension to compiler flag.");
		return NULL;
	}
	log_debug("optimiser.c: Flag string generated is: \"%s\"", string);

	return string;
}

char *opt_position_to_string(spso_position_t * position)
{
	int i;
	char *options = NULL;
	char *next_option = NULL;
	int options_size = 0;
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if (MASTER != rank) {
		log_fatal
		    ("opt_position_to_string should only be called by the master rank!");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}

	log_trace("optimiser.c: Converting position to string.");

	if (position == NULL) {
		log_error("position is null! Cannot convert to string");
		return NULL;
	}

	options = strdup("");	/* strlen will segfault on a NULL */
	for (i = 0; i < search_space_size; i++) {
		next_option =
		    convert_dimension_to_string(search_space[i], position->dimension[i]);
		if (strcmp(next_option, "")) {
			options_size =
			    strlen(options) + strlen(next_option) + 2;
			options = realloc(options, options_size);
			strcat(options, " ");
			strcat(options, next_option);
		} else {
			log_debug
			    ("optimiser.c: Skipping dimension %d (%s) as no string was produced",
			     search_space[i]->uid,
			     search_space[i]->name);
		}
		free(next_option);
	}
	log_debug("optimiser.c: Generated string: %s", options);
	return options;
}

char *build_compiler_options(opt_particle_t * particle)
{
	char *options;
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if (MASTER != rank) {
		log_fatal
		    ("build_compiler_options should only be called by the master rank!");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}

	if (particle == NULL) {
		log_debug
		    ("optimiser.c: Not building compiler options for NULL particle");
		return strdup("");
	}

	log_trace("optimiser.c: Building compiler options for particle %d",
		  particle->uid);
	options = opt_position_to_string(&particle->position);

	return options;
}
