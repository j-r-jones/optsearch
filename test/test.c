#include <stdlib.h>
#include <assert.h>

#include "common.h"

#ifndef BSD_SOURCE
#define _BSD_SOURCE 1
#endif
#include <string.h>
#include <math.h>

#ifndef PI
#ifdef M_PI
#define PI M_PI
#elif defined M_PIl
#define PI M_PIl
#else
#define PI 3.14159265358979323846
#endif				/* !defined PI */
#endif

#include "logging.h"
#include "config.h"
#include "taskfarm.h"
#include "random.h"
#include "data.h"
#include "spso.h"
#include "optimiser.h"

/* TODO Refactor; this is the bare minimum of what is needed.
 *
 * Tests should be divided up into separate files according to the
 * component being tested.  All functionality of each component should be
 * tested, each part in a separate function (or small number of functions).
 *
 * It might be feasible to use CUnit for simple black-box testing, as in the
 * project's example:
 * http://cunit.sourceforge.net/example.html
 */
const char *test_flags1 = "test flags 1";
const char *test_flags2 = "test flags 2 ";
const char *test_flags3 = "test flags 3 ";
const char *test_flags4 = "test flags 4 ";
const char *test_flags5 = "test flags 5 ";
const char *test_flags6 = "test flags 6 ";
const char *test_flags7 = "test flags 7 ";

/* Helper functions */
spso_velocity_t *opt_test_new_velocity(int num_dims, spso_dimension_t ** dims)
{
	int i;
	spso_velocity_t *velocity = opt_db_new_velocity(num_dims, dims);
	for (i = 0; i < num_dims; i++) {
		velocity->dimension[i] = opt_rand_int_range(dims[i]->min,
				       dims[i]->max);
	}
	return velocity;
}

spso_position_t *opt_test_new_position(int num_dims, spso_dimension_t ** dims)
{
	int i;
	spso_position_t *position = opt_db_new_position(num_dims, dims);
	for (i = 0; i < num_dims; i++) {
		position->dimension[i]=
		    opt_rand_int_range(dims[i]->min,
				       dims[i]->max);
	}
	return position;
}

spso_particle_t *opt_test_new_particle(int uid, int num_dims,
				       spso_dimension_t ** dims)
{
	spso_particle_t *part = malloc(sizeof(*part));
	part->uid = uid;
	part->velocity = *opt_db_new_velocity(num_dims, dims);
	part->position = *opt_db_new_position(num_dims, dims);
	part->previous_best = *opt_db_new_position(num_dims, dims);
	return part;
}

spso_swarm_t *opt_test_new_swarm(int size, int num_dims,
				 spso_dimension_t ** dims)
{
	int i;
	spso_swarm_t *swarm = malloc(sizeof(*swarm));
	swarm->size = size;
	swarm->particles = malloc(sizeof(*(swarm->particles)) * swarm->size);
	for (i = 0; i < swarm->size; i++) {
		swarm->particles[i] = opt_test_new_particle(i, num_dims, dims);
	}
	return swarm;
}

void add_to_fitness_queue(int particle_uid)
{
	log_debug("Received uid %d.", particle_uid);
	return;
}

spso_fitness_t gen_fitness(int num_dims, spso_position_t * position)
{
	/* 
	 * Rastrigin might be a good starting point.  Just generating random
	 * numbers won't be good enough as we'll never converge, though that would
	 * work for testing the "stop when bored".
	 *
	 * https://en.wikipedia.org/wiki/Rastrigin_function
	 */
	int i = 0;
	double fx = 0.0;
	double fx1 = 20.0;
	double fx2 = 0.0;

	assert(position != NULL);

	for (i = 0; i < num_dims; i++) {
		fx += pow((double) position->dimension[i], 2.0);
		fx2 += cos(2.0 * (double) PI * (double)position->dimension[i]);
	}
	fx = fx1 - 10.0 * fx2;
	return fx;
}

int report_fitness(const int x, double y, int visits)
{

	log_debug("Received uid %d and fitness (time) %lf.", x, y);

	/* TODO make use of the visit counter so we can pretend that some points
	 * have been visited before. */

	if (x == 12)
		opt_queue_push(34, test_flags5);
	else if (x == 13)
		opt_queue_push(78, test_flags6);
	else if (x == 78)
		opt_queue_push(90, test_flags7);
	else if (x == 90)
		opt_task_stop();

	return 0;
}

int test_taskfarm(int rank)
{
	opt_config_t * config = NULL;

	log_info("Starting taskfarm test");
	config = opt_new_config();

	if (rank == MASTER) {

		config->compiler = strdup("gcc");
		config->compiler_version = strdup("6.3.0");

		config->quit_signal = strdup("SIGUSR1");

		config->clean_script = strdup("./clean-script.sh");
		config->build_script = strdup("./build-script.sh");
		config->accuracy_test = strdup("./success-script.sh");
		config->timeout = 5;

		config->perf_test = strdup("./perf-script.sh");
		config->epsilon = 2.0;
		config->benchmark_timeout = 10;
		config->benchmark_repeats = 8;
		config->compiler_flags = NULL;	/* The task farm doesn't use this itself */
	}

	opt_share_config(MASTER, config);

	opt_task_initialise(config, &report_fitness);
	if (rank == MASTER) {
		opt_queue_push(11, test_flags1);
		opt_queue_push(12, test_flags2);
		opt_queue_push(13, test_flags3);
	}

	opt_task_start();

	MPI_Barrier(MPI_COMM_WORLD);

	opt_destroy_config(config);

	log_info(" ======= Finished taskfarm test ======= ");

	return 1;
}

int test_data(int rank)
{
	int i, pos_id = 0, num_dims, num_dbdims;
	double fitness;
	opt_config_t config;
	spso_dimension_t **dim = NULL;
	spso_dimension_t **dbdim = NULL;
	spso_position_t *pos = NULL;
	spso_position_t *dbpos = NULL;
	spso_velocity_t *vel = NULL; /* TODO This variable isn't used */
	spso_velocity_t *dbvel = NULL; /* TODO This variable isn't used */
	spso_particle_t *part = NULL;
	spso_particle_t *dbpart = NULL;
	spso_swarm_t *swarm;
	unsigned int dbseed, seed = 4686536;
	int flag = 1;
	int dbflag = 0;
	double prevprev, prev, curr;
	spso_position_t *currpos, *dbcurrpos;
	double dbprevprev, dbprev, dbcurr;

	if (rank != MASTER)
		return 1;

	log_trace("Starting SQLite (data) test");

	/* Easiest way to get some flags is to just read in a config file */
	assert(read_config("./test-config.yml", &config) == 1);

	num_dims = config.num_flags;
	dim = malloc(sizeof(*dim) * num_dims);
	for (i = 0; i < num_dims; i++) {
		log_debug("Creating dimension from flag %d: '%s'",
			  config.compiler_flags[i]->uid,
			  config.compiler_flags[i]->name);
		dim[i] = opt_convert_flag(config.compiler_flags[i]);
		log_debug("Dim %d is '%s'", dim[i]->uid, dim[i]->name);
	}

	assert(opt_db_init("test.sqlite", &config, num_dims, dim) ==
	       OPT_DB_NEW);
	assert(opt_db_store_flags(&config) == 0);
	for (i = 0; i < config.num_flags; i++) {
		assert(opt_db_check_flag(i, config.compiler_flags[i]) == 0);
		assert(config.compiler_flags[i]->uid == i);
		//assert(opt_db_store_dimension(dim[i]) == 0); // This now happens in
		//opt_db_init
	}

	dbdim = opt_db_get_searchspace(&num_dbdims);
	assert(dbdim != NULL);
	assert(num_dbdims == num_dims);

	for (i = 0; i < num_dims; i++) {
		assert(dim[i]->uid == dbdim[i]->uid);
		assert(!strcmp(dim[i]->name, dbdim[i]->name));
		assert(dim[i]->min == dbdim[i]->min);
		assert(dim[i]->max == dbdim[i]->max);
	}

	pos = opt_test_new_position(num_dims, dim);
	assert(opt_db_store_position(&pos_id, pos) == 0);

	fitness = gen_fitness(num_dims, pos);
	assert(opt_db_update_position_fitness(pos_id, fitness) == 0);

	dbpos = opt_db_new_position(num_dims, dim);
	assert(dbpos != NULL);

	assert(opt_db_get_position(pos_id, dbpos) == 0);
	for (i = 0; i < num_dims; i++) {
		assert(pos->dimension[i] == dbpos->dimension[i]);
	}

	part = opt_test_new_particle(0, num_dims, dim);
	part->previous_best_fitness = gen_fitness(num_dims, &(part->position));
	assert(opt_db_store_particle(part) == 0);

	dbpart = opt_test_new_particle(0, num_dims, dim);
	assert(opt_db_get_particle(part->uid, dbpart) == 0);

	for (i = 0; i < num_dims; i++) {
		assert(part->position.dimension[i] == dbpart->position.dimension[i]);
		assert(part->previous_best.dimension[i]== dbpart->previous_best.dimension[i]);
		log_debug("Test particle best fitness: %lf Retrieved particle best fitness: %lf",
		     part->previous_best_fitness, dbpart->previous_best_fitness);
		assert(part->previous_best_fitness == dbpart->previous_best_fitness);
	}

	/*
	 * TODO Create a swarm of particles and try to store and retrieve the swarm
	 * of two. This is trickier as we've already modified the database by
	 * adding a particle.
	 */
	swarm = opt_db_get_swarm(num_dbdims, dbdim);
	assert(swarm != NULL);
	/* TODO check more thoroughly what was returned.  It should be all the
	 * particles we added to the database so far. */
	assert(swarm->size == 1);
	for (i = 0; i < num_dims; i++) {
		assert(part->position.dimension[i] ==
		       swarm->particles[0]->position.dimension[i]);
		assert(part->previous_best.dimension[i] ==
		       swarm->particles[0]->previous_best.dimension[i]);
		log_debug
		    ("Test particle best fitness: %lf Particle in retrieved swarm best fitness: %lf",
		     part->previous_best_fitness, swarm->particles[0]->previous_best_fitness);
		assert(part->previous_best_fitness == swarm->particles[0]->previous_best_fitness);
	}

	assert(opt_db_store_prng_seed(seed) == 0);
	assert(opt_db_store_converged(flag) == 0);

	assert(opt_db_get_prng_seed(&dbseed) == 0);
	assert(seed == dbseed);
	assert(opt_db_get_converged(&dbflag) == 0);
	assert(flag == dbflag);

	curr = 2.71828;
	prev = 3.14159;
	prevprev = 2 * prev;
	currpos = opt_test_new_position(config.num_flags, dim);

	assert(opt_db_store_prev_prev_best(prevprev) == 0);
	assert(opt_db_store_prev_best(prev) == 0);
	assert(opt_db_store_curr_best(currpos, curr) == 0);

	assert(opt_db_get_prev_prev_best(&dbprevprev) == 0);
	log_debug("prevprev is: %lf\ndbprevprev is: %lf");
	assert(prevprev == dbprevprev);
	assert(opt_db_get_prev_best(&dbprev) == 0);
	assert(prev == dbprev);

	/* Compare positions dbcurrpos and currpos */
	dbcurrpos = opt_test_new_position(num_dims, dim);
	assert(opt_db_get_curr_best(dbcurrpos, &dbcurr) == 0);
	assert(curr == dbcurr);
	assert(dbcurrpos != NULL);
	for (i = 0; i < num_dims; i++) {
		assert(currpos->dimension[i] == dbcurrpos->dimension[i]);
	}

	assert(opt_db_finalise() == 0);
	log_trace("Finished running database tests");

	/* Clean up */
	free(dim);
	free(dbdim);
	free(pos);
	free(dbpos);
	free(part);
	free(dbpart);
	spso_destroy_swarm(swarm);
	opt_clean_config(&config);

	return 1;
}

int stop_function(void)
{
	log_debug("Stop called");
	return 0;
}

int test_spso(int rank)
{
	if (rank != MASTER)
		return 0;
	int i, j;
	int limit = 128;
	double epsilon = 4.5;
	spso_fitness_t fitness, prev_fitness, prev_prev_fitness;
	int num_dims = 7;
	spso_dimension_t **dims = malloc(sizeof(*dims) * num_dims);
	spso_position_t *pos = NULL;
	spso_swarm_t *swarm;
	spso_particle_t *particle;
	char *flags = NULL;
	int visits = 0;
	int known_positions = 0;

	log_debug("Starting SPSO test");

	dims[0] = spso_new_dimension(1, -1, 4096, 0, "foo");
	dims[1] = spso_new_dimension(2, -1, 80, 0, "bar");
	dims[2] = spso_new_dimension(3, -2, INT_MAX-1, 0, "baz");
	dims[3] = spso_new_dimension(4, -1, INT_MAX-3, 0, "quuz");
	dims[4] = spso_new_dimension(5,  0, INT_MAX-1, 0, "quux");
	dims[5] = spso_new_dimension(6,  0, INT_MAX-2, 0, "fobble");
	dims[6] = spso_new_dimension(7, -3, 512, 0, "werp");

	/* This initialises the swarm for us */
	spso_init(num_dims, dims, &add_to_fitness_queue, epsilon,
		  stop_function);
	swarm = spso_get_swarm();

	for (j = 0; j < swarm->size; j++) {
		particle = spso_get_particle(j);
		assert(particle != NULL);
		log_debug("test_spso(): Particle uid is %d", particle->uid);
	}

	spso_start_search();

	/*
	 * A naive test, since it is really difficult to make asynchronous.
	 */
	for (i = 0; i < limit; i++) {
		for (j = 0; j < swarm->size; j++) {
			particle = spso_get_particle(j);
			log_debug("test_spso(): Particle uid is %d",
				  particle->uid);
			fitness = gen_fitness(num_dims, &particle->position);
			spso_update_particle(j, fitness, visits, known_positions);
			/* Then we would now add the particle to the queue, rather than
			 * spso doing it, since someone needs to translate from position
			 * to a string representing the compiler flags for the taskfarm.
			 */
			if (spso_is_stopping())
				break;
		}
		known_positions++;
	}
	if (!spso_is_stopping()) {
		log_debug("SPSO did not converge within %d iterations", limit);
		spso_stop();
	}

	/* Report where we got to */
	pos =
	    spso_get_global_best_history(&fitness, &prev_fitness,
					 &prev_prev_fitness);
	printf("test.c: Got fitnesses:\nBest:\n%e;\nPrevious best:\n%e;\nPrevious-previous best:\n%e\n",
	     fitness, prev_fitness, prev_prev_fitness);
	printf("test.c: Converged on position:\n[");
	for (i = 0; i < num_dims - 1; i++) {
		printf("%d, ", pos->dimension[i]);
	}
	printf("%d]\n", pos->dimension[num_dims - 1]);

	spso_cleanup();

	for (i = 0; i < num_dims; i++) {
		free(dims[i]);
	}
	/*free(dims); */
	free(pos);
	free(flags);

	/* TODO Test resuming from a previous search, including that we get the
	 * same search repeated if we rewind to a previous point */

	return 1;
}

int test_prng(int rank)
{
	/* TODO */
	/* Two tests are needed, possibly three.  Firstly just starting and
	 * generating a bunch of random numbers.  Secondly, returning to a point
	 * in the middle of that sequence, and checking that we get the same bunch
	 * (ie to replicate restoring state from a previous run -- reseed the same
	 * way and run forward to the iteration we'd got to)
	 */
	int i;
	int num;
	int repeats = 1024;
	int min = -2;
	int max = INT_MAX-2;
	uint32_t *inner_seed;
	int seed_size;
	opt_rand_seed_t *seed = NULL;

	log_trace("Starting PRNG test");

	seed = opt_rand_gen_seed();

	assert(seed != NULL);

	/* Store information for later test */
	seed_size = seed->size;
	inner_seed = (uint32_t *) malloc(sizeof(uint32_t) * seed_size);
	memcpy(inner_seed, seed->seed, seed->size * sizeof(uint32_t));

	opt_rand_print_seed(stderr, seed);
	opt_rand_init_seed(seed);

	/* It'd be great to test how random it is, but that's not within the scope
	 * of a simple unit test.  This call is thus just to check we don't
	 * segfault or something. */
	num = opt_rand_int();

	for (i = 0; i < repeats; i++) {
		num = opt_rand_int_range(min, max);
		assert(num <= max);
		assert(num >= min);
	}

	log_trace("Calling free on seed");
	opt_rand_free(seed);

	/* Start again, and check we are in the same place in the stream */
	log_trace("Starting again with a new seed");
	seed = malloc(sizeof(*seed));
	seed->seed = inner_seed;
	seed->size = seed_size;
	opt_rand_print_seed(stderr, seed);

	log_trace("Calling free on seed (clone)");
	opt_rand_free(seed);
	//free(inner_seed);

	return 1;
}

int test_config(void)
{
	int i;
	opt_config_t * config = opt_new_config();

	log_info("Starting config test");

	assert(read_config("./test-config.yml", config) == 1);

	log_trace("Finished reading in config.  Checking top-level values..");

	assert(strcmp("SIGUSR1", config->quit_signal) == 0);
	assert(strcmp("./clean-script.sh", config->clean_script) == 0);
	assert(strcmp("./build-script.sh", config->build_script) == 0);
	assert(strcmp("./success-script.sh", config->accuracy_test) == 0);
	assert(120 == config->timeout);

	assert(strcmp("./perf-script.sh", config->perf_test) == 0);
	assert(240 == config->benchmark_timeout);
	assert(6 == config->benchmark_repeats);
	assert(10.0 == config->epsilon);	/* TODO This is not how you should test equivalence with doubles */

	log_trace("Checking compiler section values..");
	assert(strcmp("gcc", config->compiler) == 0);
	assert(strcmp("4.9.2", config->compiler_version) == 0);

	log_trace("Checking flags..");

	assert(6 == config->num_flags);
	for (i = 0; i < config->num_flags; i++) {
		assert(config->compiler_flags[i] != NULL);
	}

	/* First flag is -f(no-)expensive-optimizations */
	assert(strcmp(config->compiler_flags[0]->name, "expensive-optimizations")
	       == 0);
	assert(config->compiler_flags[0]->type == OPT_ONOFF_FLAG);
	assert(strcmp(config->compiler_flags[0]->prefix, "-f") == 0);
	assert(strcmp(config->compiler_flags[0]->data.onoff.neg_prefix, "-fno-")
	       == 0);

	assert(strcmp(config->compiler_flags[1]->name, "forward-propagate") ==
	       0);
	assert(config->compiler_flags[1]->type == OPT_ONOFF_FLAG);
	assert(strcmp(config->compiler_flags[1]->prefix, "-f") == 0);
	assert(strcmp(config->compiler_flags[1]->data.onoff.neg_prefix, "-fno-")
	       == 0);

	assert(strcmp(config->compiler_flags[2]->name, "inline-functions") == 0);
	assert(config->compiler_flags[2]->type == OPT_ONOFF_FLAG);
	assert(strcmp(config->compiler_flags[2]->prefix, "-f") == 0);
	assert(strcmp(config->compiler_flags[2]->data.onoff.neg_prefix, "-fno-")
	       == 0);

	assert(strcmp(config->compiler_flags[3]->name, "stack-reuse") == 0);
	assert(config->compiler_flags[3]->type == OPT_LIST_FLAG);
	assert(strcmp(config->compiler_flags[3]->prefix, "-f") == 0);
	assert(strcmp(config->compiler_flags[3]->data.list.separator, "=") == 0);
	assert(config->compiler_flags[3]->data.list.size == 3);
	assert(strcmp(config->compiler_flags[3]->data.list.values[0], "all") ==
	       0);
	assert(strcmp
	       (config->compiler_flags[3]->data.list.values[1],
		"named_vars") == 0);
	assert(strcmp(config->compiler_flags[3]->data.list.values[2], "none") ==
	       0);

	assert(strcmp(config->compiler_flags[4]->name, "fp-contract") == 0);
	assert(config->compiler_flags[4]->type == OPT_LIST_FLAG);
	assert(strcmp(config->compiler_flags[4]->prefix, "-f") == 0);
	assert(strcmp(config->compiler_flags[4]->data.list.separator, "=") == 0);
	assert(config->compiler_flags[4]->data.list.size == 2);
	assert(strcmp(config->compiler_flags[4]->data.list.values[0], "off") ==
	       0);
	assert(strcmp(config->compiler_flags[4]->data.list.values[1], "fast") ==
	       0);

	assert(strcmp(config->compiler_flags[5]->name, "tree-parallelize-loops")
	       == 0);
	assert(config->compiler_flags[5]->type == OPT_RANGE_FLAG);
	assert(strcmp(config->compiler_flags[5]->prefix, "-f") == 0);
	assert(strcmp(config->compiler_flags[5]->data.range.separator, "=") ==
	       0);
	assert(config->compiler_flags[5]->data.range.min == 0);
	assert(config->compiler_flags[5]->data.range.max == 256);

	opt_destroy_config(config);

	return 1;
}

int test_optimiser(int rank)
{
	int i;
	opt_config_t * config = opt_new_config();
	opt_flag_t *flag = NULL;
	opt_dimension_t **dim = NULL;

	log_info("Starting optimiser test");
	/*
	 * TODO
	 * Most of these tests are really over the string functions such as
	 * convert_dimension_to_string(opt_dimension_t * dim).  This is
	 * complicated by needing to call opt_init first, and make sure that all
	 * the test data is prepared.  This can really only happen on the master
	 * rank.
	 */

	if (MASTER == rank) {
		assert(read_config("./test-config.yml", config) == 1);
	}

	fflush(stderr);
	MPI_Barrier(MPI_COMM_WORLD);
	opt_share_config(MASTER, config);
	
	assert(strcmp("SIGUSR1", config->quit_signal) == 0);
	assert(strcmp("./clean-script.sh", config->clean_script) == 0);
	assert(strcmp("./build-script.sh", config->build_script) == 0);
	assert(strcmp("./success-script.sh", config->accuracy_test) == 0);
	assert(120 == config->timeout);

	assert(strcmp("./perf-script.sh", config->perf_test) == 0);
	assert(240 == config->benchmark_timeout);
	assert(6 == config->benchmark_repeats);
	assert(10.0 == config->epsilon);	/* TODO This is not how you should test equivalence with doubles */

	log_trace("Checking compiler section values..");
	assert(strcmp("gcc", config->compiler) == 0);
	assert(strcmp("4.9.2", config->compiler_version) == 0);

	/*
	 * We don't really want to run this, but rather check all the helper
	 * functions.  It is difficult to avoid running it though.
	 */
	opt_start_search(config);

	if (MASTER == rank) {
		log_trace("Checking flags..");
		dim = malloc(sizeof(*dim) * config->num_flags);

		assert(6 == config->num_flags);
		for (i = 0; i < config->num_flags; i++) {
			assert(config->compiler_flags[i] != NULL);
			assert(config->compiler_flags[i]->uid == i);
			flag = opt_get_flag(i);
			assert(flag != NULL);
			assert(flag->uid == i);
			dim[i] = opt_convert_flag(config->compiler_flags[i]);
			log_debug
			    ("dim[%d]->uid is %d ; config.compiler_flags[%d]->uid is %d",
			     i, dim[i]->uid, i, config->compiler_flags[i]->uid);
			assert(dim[i]->uid == config->compiler_flags[i]->uid);
		}

		flag = opt_get_flag(4);
		assert(strcmp(flag->name, "fp-contract") == 0);
		assert(flag->type == OPT_LIST_FLAG);
		assert(strcmp(flag->prefix, "-f") == 0);
		assert(strcmp(flag->data.list.separator, "=") == 0);
		assert(flag->data.list.size == 2);
		assert(strcmp(flag->data.list.values[0], "off") == 0);
		assert(strcmp(flag->data.list.values[1], "fast") == 0);
	}

	MPI_Barrier(MPI_COMM_WORLD);

	if (MASTER == rank) {
		opt_stop_search();
		for (i = 0; i < config->num_flags; i++) {
			free(dim[i]);
		}
	}
	opt_destroy_config(config);

	return 1;
}

int test_runcommand_from_config(int rank)
{
	int status = -1;
	int timeout = 0;
	double time;
    char * flags = "FLAGS=\"-ffoo -fbaz -fno-bar\"";
    char * command = NULL;
    int len = 0;
    opt_config_t config;

    if (MASTER == rank) {
        assert(read_config("./test-config.yml", &config) == 1);
        len =  strlen(config.clean_script) +1;
        command = config.clean_script;
        timeout = config.timeout;
    }

    MPI_Bcast(&timeout, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

    MPI_Bcast(&len, 1, MPI_INT, MASTER, MPI_COMM_WORLD);
    if (MASTER != rank) {
        command = realloc(command, sizeof(char) * len);
    }

    MPI_Bcast(command, len, MPI_CHAR, MASTER, MPI_COMM_WORLD);
    if (MASTER != rank) {
        log_trace("Running command \"%s\"", command);
        status = run_command(command, &time, timeout);
        assert(0 == status);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (MASTER == rank) {
        len = (strlen(flags) + strlen(config.clean_script)) +1;
        command = NULL;
    }
    MPI_Bcast(&len, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

    command = realloc(command, sizeof(char) * len);
    if (MASTER == rank) {
        sprintf(command, "%s %s", flags, config.clean_script);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);

    MPI_Bcast(command, len, MPI_CHAR, MASTER, MPI_COMM_WORLD);
    if (MASTER != rank) {
        log_trace("Running command \"%s\"", command);
        status = run_command(command, &time, timeout);
        assert(0 == status);
    }

    free(command);
    command = NULL;

    return 1;
}

int test_runcommand(void)
{
	const char *success_cmd = "FLAGS=\"test_runcommand 1\" ./success-script.sh";
	const char *fail_cmd = "FLAGS=\"test_runcommand 2\" ./fail-script.sh";
	const char *timeout_cmd = "FLAGS=\"test_runcommandi 3\" ./sleepy-script.sh";
	const char *mpi_cmd = "FLAGS=\"test_runcommand 4\" ./mpi-script.sh";
	int status = -1;
	int timeout = 5;
	double time;

    log_trace("Running success script");
	status = run_command(success_cmd, &time, timeout);
	assert(0 == status);

    log_trace("Running failure script");
	status = run_command(fail_cmd, &time, timeout);
	assert(1 == status);

    log_trace("Running timeout script");
	status = run_command(timeout_cmd, &time, timeout);
	assert(0 != status);

    log_trace("Running MPI helloworld invoking script");
	status = run_command(mpi_cmd, &time, timeout);
	assert(0 == status);

	return 1;
}

int main(int argc, char **argv)
{
	int rank, len;
    char name[MPI_MAX_PROCESSOR_NAME] = {'\0'};

	MPI_Init(&argc, &argv);

    MPI_Get_processor_name(name, &len);
    if (len < MPI_MAX_PROCESSOR_NAME) {
        name[len+1] = '\0';
	}
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	log_initialise(NULL);
	turn_on_logging();
	log_info("Starting tests on host %s", name);
	fflush(stdout);
	fflush(stderr);
	MPI_Barrier(MPI_COMM_WORLD);

	if (MASTER == rank) {
		assert(test_config() == 1);
	}
	fflush(stdout);
	fflush(stderr);
	MPI_Barrier(MPI_COMM_WORLD);

	/*
	if (MASTER == rank) {
		assert(test_runcommand() == 1);
	}
	fflush(stdout);
	fflush(stderr);
	MPI_Barrier(MPI_COMM_WORLD);
    
    assert(test_runcommand_from_config(rank) == 1);
	fflush(stdout);
	fflush(stderr);
	MPI_Barrier(MPI_COMM_WORLD);

	if (MASTER == rank) {
		assert(test_data(rank) == 1);
	}
	fflush(stdout);
	fflush(stderr);
	MPI_Barrier(MPI_COMM_WORLD);
	*/

	/*
	 * TODO FIXME We will struggle to get repeatable results because our hopes
	 * of resuming from a previous PRNG are in vain at present.  This partly
	 * due to the asynchronous SPSO.  I can't see anyway to fix this.
	 */
	/*
	if (MASTER == rank) {
		assert(test_prng(rank) == 1);
	}
	fflush(stdout);
	fflush(stderr);
	MPI_Barrier(MPI_COMM_WORLD);
	*/

	/*
	if (MASTER == rank) {
		assert(test_spso(rank) == 1);
	}
	fflush(stdout);
	fflush(stderr);
	MPI_Barrier(MPI_COMM_WORLD);
	*/

	assert(test_taskfarm(rank) == 1);
	fflush(stdout);
	fflush(stderr);
	MPI_Barrier(MPI_COMM_WORLD);

	assert(test_optimiser(rank) == 1);
	fflush(stdout);
	fflush(stderr);
	MPI_Barrier(MPI_COMM_WORLD);

	MPI_Finalize();
	return EXIT_SUCCESS;
}
