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

#include <common.h>
#include <stats.h>
#include <taskfarm.h>

typedef enum {
	OPT_TASK_STOPPED,
	OPT_TASK_BUSY,
	OPT_TASK_WAITING
} opt_task_state_e;

typedef enum {
	QUIT_POS = 0,
	CLEAN_POS = 1,
	BUILD_POS = 2,
	TEST_POS = 3,
	BENCH_POS = 4
} opt_task_position_e;

/* A simple mapping of worker rank to pointer of what they are working on.
 * NULL means nothing (waiting or stopped).  The worker_state array will
 * confirm it.
 */
opt_work_item_t **working_on_item = NULL;
opt_task_state_e *worker_state = NULL;

/* TODO
 *
 * In more detail:
 *
 * The Master will need to:
 * - Poll all workers and find out who is on which nodes; Construct a set of
 *   active workers, who do not share nodes with anyone else in the set.
 * - Receive new tasks into the work queue.
 * - Listen for responses from workers; report these back to the calling
 *   process, which should record all results into SQLite and populate our
 *   work queue for us.
 * - Supply the next work item to the worker who just gave us their results.
 * - On receipt of signal (defined in the config), shut down cleanly.
 *
 * The Workers will need to:
 * - Check which node they are on and report this to the master. Sleep if not
 *   made part of the active set/communicator.
 * - Request a work item from the master
 * - Execute the work item; Set the oom_adj for the spawned process to 14.  It
 *   may be necessary to consider process pinning too, but we will worry about
 *   that in the future if it appears to be necessary.
 * - Record the return value and output number (if any).
 *      - Optionally record and return the runtime (this will be known to
 *        the master if we are doing this as it will have been an option in the
 *        config).  If the work item fails (timeout or other failure), return
 *        the failure fitness and an execution time of Inf.
 *      - If the work item fails (eg sigsegv), pick the default value for failure
 *        (this should correspond to a fitness score of Inf).
 *      - If the work item runs for longer than a threshold, kill it and use
 *        the default value for failure, as above.
 * - Return the recorded information with the particle UID to the master.
 * - Receive the next work item and execute it.  Repeat until no more work is
 *   given to us.
 * - On receipt of signal (defined in the config), shut down cleanly, killing
 *   the executing work item processes.
 *
 * NB (assuming OpenMPI is being used):
 * - SIGUSR1 will usually kill a bash script.  One workaround is to use
 *   "exec mpirun" in the job script rather than plain mpirun.
 * - SIGTSTP and SIGCONT signals will simply be consumed by the mpirun
 *   process, unless you use the appropriate MCA parameter, IE you have to run
 *   the job with --mca orte_forward_job_control 1, which means mpirun will
 *   receive SIGSTP and send a SIGSTOP to the running processes.  Likewise, it
 *   will receive the SIGCONT and send a SIGCONT to the running processes.
 *
 */

/*
 * Each work item is a set of strings that must be run in order one after the
 * other, IF the preceding one terminates successfully.  These are:
 *
 * 1. The clean-up command (generally a script)
 * 2. The build command (again, a script)
 * 3. The accuracy test (a script also)
 * 4. The benchmark (usually but not always a script)
 *
 * These are the same every time, so need not be repeated in the queue every
 * time.  We can get them from the opt_config_t* that we are given, which also
 * gives us useful things like the notification of termination signal we
 * should be listening for.
 *
 * The actual item we need is the FLAGS, which might be FFLAGS or CFLAGS or
 * similar.  This means it is really only ONE string, that is prepended to
 * the strings already derived from the opt_config_t* before each command is
 * run.
 *
 * They must always be run in this order; each one must end cleanly without
 * errors for the next one to run, or we will notify the listener that the
 * fitness evaluated to Inf.  If they all finish successfully, we should be
 * returning the runtime (or FLOPS if we can make use of perf) of the
 * benchmark as the fitness value, which will need to be given along with some
 * identifier for the FLAGS, which correspond to a position of a particle in PSO.
 *
 */

/*
 * TODO We will need to watch the queue size as this is going to be limited in
 * memory.  Especially on the master, we'll have the problem of the SPSO
 * particles in memory too, and so we may end up hitting some very tight
 * limits if we are not careful.
 */
opt_config_t *config = NULL;
int (*update_fitness) (int, double, int);

int queue_size;
opt_work_item_t *queue_front;
opt_work_item_t *queue_back;

bool stop_work = false;

static int msg_sequence;

int opt_get_msg_seq(void)
{
	return ++msg_sequence;
}

int opt_task_initialise(opt_config_t * conf,
			int (*report_fitness) (const int, double, int))
{
	int i, my_rank, size;

	log_trace("taskfarm.c: Entered opt_task_initialise in taskfarm.c");

	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	msg_sequence = 0;
	config = conf;

	if (MASTER == my_rank) {
		queue_size = 0;
		queue_front = NULL;
		queue_back = NULL;
		log_trace("taskfarm.c: size is %d", size);
		working_on_item = malloc(size * sizeof(*working_on_item));
		worker_state = malloc(size * sizeof(*worker_state));
		for (i = 0; i < size; i++) {
			working_on_item[i] = NULL;
			worker_state[i] = OPT_TASK_WAITING;
		}
		update_fitness = report_fitness;
	} else {
		/* I'm doing this on purpose so that we have an error if a worker
		 * calls it by mistake, which should never happen */
		update_fitness = NULL;
	}

	return 0;
}

opt_work_item_t *opt_queue_pop(void)
{
	log_trace("taskfarm.c: Entered opt_queue_pop in taskfarm.c");
	opt_work_item_t *item = queue_front;
	if (item != NULL) {
		queue_front = item->next;
		item->next = NULL;
		queue_size--;
	}
	return item;
}

int opt_queue_push(const int work_item_uid, const char *work_item)
{
	int my_rank;
	opt_work_item_t *work = NULL;
	log_trace("taskfarm.c: Entered opt_queue_push in taskfarm.c");

	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	/* We should only get called if we are rank 0, so the check should be
	 * unnecessary */
	if (MASTER == my_rank) {
		/* TODO We should be checking that malloc and realloc are successful */
		errno = 0;
		work = malloc(sizeof(*work));
		if (work == NULL) {
			if (errno == ENOMEM) {
				log_error("Out of memory.");
			}
			log_fatal
			    ("Unable to allocate memory for addition of work item to queue.");
			MPI_Abort(MPI_COMM_WORLD, -1);
		}
		work->command = strdup(work_item);	/* This also needs a check that malloc was successful */
		work->uid = work_item_uid;
		work->next = NULL;

		if (queue_front == NULL) {
			queue_front = work;
		}
		if (queue_back != NULL) {
			queue_back->next = work;
		}
		queue_back = work;
		queue_size++;

		return queue_size;
	}
	return 0;
}

int opt_task_stop(void)
{
	/* Signal that the task farm should stop work */
	stop_work = true;
	return 0;
}

int opt_task_clean_up(void)
{
	int my_rank, size, i;
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	log_trace("taskfarm.c: Entered opt_task_clean_up in taskfarm.c");
	opt_work_item_t *item = NULL;

	/*
	 * TODO notify workers if we hadn't been intending to stop before.
	 */

	if (!stop_work) {
		log_warn
		    ("Have not been told to stop, but the clean_up function has been called");
		opt_task_stop();
	}

	/*
	 * FIXME Calling free seems to cause a double free later, as
	 * MPI_Finalize() appears to struggle to work out what needs to be free'd
	 * and frees things incorrectly.  It doesn't affect all our allocated
	 * memory, but does affect a lot of it.  What is and is not affected is
	 * unpredictable, and seems to change with the version of OpenMPI we are
	 * using, so I am just commenting out this entire section.  It causes too
	 * many problems otherwise.
	 */
	/*
	   if (MASTER == my_rank) {
		   item = queue_front;
		   while (item != NULL) {
		   if (item->command != NULL)
			   free(item->command);
			   item->command = NULL;
			   free(item);
			   item = NULL;
			   item = opt_queue_pop();
		   }
		   queue_front = NULL;
		   MPI_Comm_size(MPI_COMM_WORLD, &size);
		   for (i = 1; i < size; i++) {
			   working_on_item[i] = NULL;
		   }
		   //free(working_on_item);
		   working_on_item = NULL;
		   free(worker_state);
	   }
	 */

	return 0;		/* TODO Return something more useful */
}

opt_task_msg_t opt_task_receive_work(opt_work_item_t * item)
{
	int rc = 0;
	int header[4] = { 0 };
	MPI_Status status;

	rc = MPI_Recv(header, 4, MPI_INT, MASTER, OPT_TASK_HEADER_TAG,
		      MPI_COMM_WORLD, &status);
	if (rc != MPI_SUCCESS) {
		log_fatal
		    ("Receiving header from master was unsuccessful.  Received code: %d from MPI_Recv",
		     rc);
		MPI_Abort(MPI_COMM_WORLD, rc);
	}
	log_trace("taskfarm.c: Received header (seq #%d) from master",
		  header[OPT_HEADER_SEQ]);
	if (header[OPT_HEADER_TYPE] == OPT_TASK_STOP_MSG) {
		log_debug("taskfarm.c: Told to stop work by master (msg #%d)",
			  header[OPT_HEADER_SEQ]);
		stop_work = true;
	} else {
		log_debug
		    ("taskfarm.c: Receiving message of size %d (seq #%d) from master",
		     header[OPT_HEADER_SIZE], header[OPT_HEADER_SEQ]);
		item->uid = header[OPT_HEADER_UID];
		item->command = realloc(item->command, header[OPT_HEADER_SIZE]);
		/* Receive actual message */
		rc = MPI_Recv(item->command, header[OPT_HEADER_SIZE], MPI_CHAR,
			      MASTER, OPT_TASK_MSG_TAG, MPI_COMM_WORLD,
			      &status);
		if (rc != MPI_SUCCESS) {
			log_fatal
			    ("Receiving message #%d from master was unsuccessful.  Received code: %d from MPI_Recv",
			     header[OPT_HEADER_SEQ], rc);
			MPI_Abort(MPI_COMM_WORLD, rc);
		}
		log_debug
		    ("taskfarm.c: Received message (seq #%d): '%s' from master",
		     header[OPT_HEADER_SEQ], item->command);
	}
	return header[OPT_HEADER_TYPE];
}

int opt_task_send_to_worker(int worker, opt_task_msg_t type,
			    opt_work_item_t * item)
{
	int rc = 0;
	int seq;
	int header[4] = { 0 };
	char *buffer = NULL;

	if (item == NULL && type != OPT_TASK_STOP_MSG) {
		log_error("Cannot send NULL work item to workers");
		return 1;
	}

	if (working_on_item[worker] != NULL) {
		log_debug
		    ("taskfarm.c: Worker %d was already working on something, which is about to be lost (particle uid was %d)",
		     worker, working_on_item[worker]->uid);
	}

	header[OPT_HEADER_TYPE] = type;
	seq = opt_get_msg_seq();
	header[OPT_HEADER_SEQ] = seq;

	if (type == OPT_TASK_WORK_MSG) {
		header[OPT_HEADER_UID] = item->uid;
		header[OPT_HEADER_SIZE] = strlen(item->command) + 1;	/* +1 for '\0' */
		buffer = item->command;
	} else {
		header[OPT_HEADER_SIZE] = 1;
	}

	log_trace
	    ("taskfarm.c: Sending header (%d) for message of type %d to worker rank %d",
	     seq, type, worker);
	/* Send the header for the message */
	rc = MPI_Send(header, 4, MPI_INT, worker, OPT_TASK_HEADER_TAG,
		      MPI_COMM_WORLD);
	if (rc != MPI_SUCCESS) {
		log_fatal
		    ("Sending header (%d) to worker %d was unsuccessful.  Received code: %d from MPI_Send",
		     seq, worker, rc);
		MPI_Abort(MPI_COMM_WORLD, rc);
	}

	if (type != OPT_TASK_STOP_MSG) {
		log_trace
		    ("taskfarm.c: Sending message (%d) of size %d and type %d to worker rank %d",
		     seq, header[OPT_HEADER_SIZE], type, worker);
		rc = MPI_Send(buffer, header[OPT_HEADER_SIZE], MPI_CHAR, worker,
			      OPT_TASK_MSG_TAG, MPI_COMM_WORLD);
		/* Probably ought to treat this the same way as the header, and abort if
		 * it fails. */
		/* Keep track of which particle this worker is working on */
		working_on_item[worker] = item;
		worker_state[worker] = OPT_TASK_BUSY;
	} else {
		working_on_item[worker] = NULL;
		worker_state[worker] = OPT_TASK_STOPPED;
	}

	return rc;
}

int opt_recv_fitness_from_worker(int *worker, double *fitness)
{
	int rc = 0;
	opt_work_item_t *item = NULL;
	MPI_Status status;

	rc = MPI_Recv(fitness, 1, MPI_DOUBLE, MPI_ANY_SOURCE, OPT_TASK_MSG_TAG,
		      MPI_COMM_WORLD, &status);
	if (rc != MPI_SUCCESS) {
		log_fatal
		    ("Receiving message from worker was unsuccessful.  Received code: %d from MPI_Recv",
		     rc);
		MPI_Abort(MPI_COMM_WORLD, rc);
	}
	log_debug("taskfarm.c: Received message from worker %d",
		  status.MPI_SOURCE);
	*worker = status.MPI_SOURCE;
	/* TODO Should we not check we are in bounds? */
	item = working_on_item[*worker];
	if (item == NULL) {
		/* Should never get here */
		log_error
		    ("Received fitness %lf from worker %d, but this worker doesn't appear to be working on anything!",
		     fitness, *worker);
	} else {
		/* Tell our listener, who should act accordingly (eg adding the next
		 * position to our work queue, or telling us to stop work). */
		if (fitness == NULL) {
			log_error
			    ("Not sure what is going on, but we got a NULL fitness from worker %d for particle %d",
			     worker, item->uid);
		}
		log_trace("taskfarm.c: Updating particle %d with fitness %lf",
			  item->uid, *fitness);
		update_fitness(item->uid, *fitness, false);

		/* Clean up now we're finished with this item */
		free(item->command);
		free(item);
	}

	working_on_item[*worker] = NULL;	/* No work right now */

	/* This is kind of redundant */
	return rc;
}

int opt_task_get_next_idle_worker(void)
{
	int i;
	int num_workers;
	MPI_Comm_size(MPI_COMM_WORLD, &num_workers);
	/* Master does not do work itself */
	num_workers--;
	for (i = 1; i <= num_workers; i++) {
		if (working_on_item[i] == NULL
		    && worker_state[i] == OPT_TASK_WAITING) {
			return i;
		}
	}
	return 0;
}

void master(void)
{
	int flag = 0;
	int num_workers, worker;
	int my_rank;
	opt_work_item_t *item = NULL;
	double fitness;
	bool all_finished = false;

	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	log_trace("taskfarm.c: Entered master function.");

	MPI_Comm_size(MPI_COMM_WORLD, &num_workers);
	worker_state = malloc(sizeof(*worker_state) * num_workers);
	/* Master does not do work itself */
	num_workers--;
	/*
	 * This check should be superfluous, and we ought to error if it fails.
	 */
	if (MASTER == my_rank) {
		stop_work = false;
		/* Initialise workers with work from queue */
		log_debug("taskfarm.c: Sending work items to %d workers",
			  num_workers);
		worker = 1;	/* Don't want root to send to itself */
		item = opt_queue_pop();
		while (item != NULL && worker <= num_workers) {
			/* I hope all the commands are null-terminated */
			opt_task_send_to_worker(worker, OPT_TASK_WORK_MSG,
						item);
			item = opt_queue_pop();
			worker++;
		}
		log_trace
		    ("taskfarm.c: Finished sending first batch of work items to workers");

		/*
		 * General work loop.  I could not think of a good way to do this
		 * without using an MPI_Probe of some variety
		 */
		worker = 0;
		while (!all_finished) {
			/* This trace message is too noisy */
			//log_trace("taskfarm.c: probing for a message from workers (MPI_ANY_TAG)");
			MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
				   &flag, MPI_STATUS_IGNORE);
			if (flag) {
				opt_recv_fitness_from_worker(&worker, &fitness);
				log_debug
				    ("taskfarm.c: Received fitness score %lf from rank %d",
				     fitness, worker);
			} else {
				log_trace
				    ("taskfarm.c: No workers are trying to send to master.");
				/* Deal with workers left idle when queue was empty (if that
				 * happened), if queue is now not empty */
				worker = opt_task_get_next_idle_worker();
				if (worker != 0) {
					log_trace
					    ("taskfarm.c: Found idle worker %d",
					     worker);
				} else {
					log_trace
					    ("taskfarm.c: No idle workers; sleeping");
					sleep(10);
				}
			}

			if (stop_work) {
				if (worker != 0) {
					log_trace
					    ("taskfarm.c: Telling worker %d to stop.",
					     worker);
					opt_task_send_to_worker(worker,
								OPT_TASK_STOP_MSG,
								NULL);
				}
				/* This could be a problem if a particular worker never comes
				 * back to us.  Hopefully being sent the quit_signal as
				 * defined in the config will mean this doesn't matter */
				all_finished = true;
				for (worker = 1; worker <= num_workers;
				     worker++) {
					if (worker_state[worker] !=
					    OPT_TASK_STOPPED) {
						all_finished = false;
						log_trace
						    ("taskfarm.c: worker %d is still not stopped",
						     worker);
					}
				}
				if (all_finished) {
					log_trace
					    ("taskfarm.c: All workers are idle and we have been told to stop.  Stopping.");
					break;
				} else {
					log_trace
					    ("taskfarm.c: Should be stopping, but have not heard from all workers yet.");
					continue;
				}
			} else if (item != NULL && worker != 0) {
				log_trace
				    ("taskfarm.c: Sending next work item to worker %d",
				     worker);
				opt_task_send_to_worker(worker,
							OPT_TASK_WORK_MSG,
							item);
				item = opt_queue_pop();
			} else if (item == NULL) {
				if (worker != 0) {
					worker_state[worker] = OPT_TASK_WAITING;
					log_trace
					    ("taskfarm.c: Worker %d is starving",
					     worker);
				}
				/* Sleep briefly, then see if there is anything in the queue yet */
				log_trace
				    ("taskfarm.c: Queue is empty, sleeping before checking it again.");
				sleep(10);
				item = opt_queue_pop();
			}
		}

		opt_task_clean_up();
	} else {
		log_error
		    ("Not the master process, yet the master() function was called.");
	}
}

int prologue(const char * format, char * flags, double * time)
{
	int retval = 1;
	char * command = NULL;
	int size = strlen(flags) + strlen(format) + 1; /* The +1 is for '\0' */

	if (!stop_work) {
		command = realloc(command, size + strlen(config->clean_script));
		sprintf(command, format, flags, config->clean_script);
		log_debug("taskfarm.c: Clean command is %s.", command);
		retval = run_command(command, time, config->timeout);
		if (retval == 0) {
			command = realloc(command, size + strlen(config->build_script));
			sprintf(command, format, flags, config->build_script);
			log_debug("taskfarm.c: Build command is %s.", command);
			retval = run_command(command, time, config->timeout);
			if (retval == 0) {
				command = realloc(command, size + strlen(config->accuracy_test));
				sprintf(command, format, flags, config->accuracy_test);
				log_debug("taskfarm.c: Test command is %s.", command);
				retval = run_command(command, time, config->timeout);
			}
		}

		if (retval) {
			*time = DBL_MAX;
		}
	}

	return retval;
}

int benchmark(const char * format, char * flags, double * time)
{
	int retval = 1;
	int i;
	double stdev = 0.0;
	double perc = 0.0;
	double value = 0.0;
	double * values = NULL;
	char * command = NULL;
	int size = strlen(flags) + strlen(format) + strlen(config->perf_test) + 1; /* The +1 is for '\0' */
	if (config->benchmark_repeats == 0) {
		/* Assume that if nothing was set in the config, the user intended to
		 * run the test once. */
		config->benchmark_repeats = 1;
	}
	values = calloc(sizeof(*values), config->benchmark_repeats);

	if (!stop_work) {
		command = realloc(command, size);
		sprintf(command, format, flags, config->perf_test);
		log_debug("taskfarm.c: Benchmark command is %s.", command);

		for (i=0; i < config->benchmark_repeats; i++) {
			/*
			 * This one MUST be repeated either config->benchmark_repeat
			 * times, OR if 4 runs have a standard deviation <= config->epsilon,
			 * then we should report the mean time, not the time of just one run.
			 */
			retval = run_command(command, &value, config->benchmark_timeout);
			if (retval) {
				log_error("Error encountered attempting to run the benchmark.");
				*time = DBL_MAX;
				break;
			} else {
				values[i] = value;
				stdev = standard_deviation(i, values);
				perc = percent_of_values(config->epsilon, i, values);
				log_debug("converted %e percent to %e", config->epsilon, perc);
				if (stdev > perc) {
					/* Then all is NOT well */
					log_error("Error (%e) outside of permitted range (%e)%.", stdev, config->epsilon);
					*time = DBL_MAX;
					break;
				}
				if (i > 4 && stdev <= perc) {
					/* We don't need to keep going */
					log_debug("In 4 runs of the benchmark, deviation has stayed at or below expected error.");
					break;
				}
			}
		}
		*time = mean(i, values);
	}

	return retval;
}

void worker(void)
{
	int my_rank, len;
    char processorname[MPI_MAX_PROCESSOR_NAME] = { '\0' };

	/* Each command should be run as:
	 *      FLAGS="-fblah -ffoo -fquux" compile.sh
	 * It is up to the user to ensure that the scripts run correctly in this
	 * way and are able to give a useful return value.
	 */
	const char *const command_format = "FLAGS=\"%s\" %s";

	int retval = -1;
	double time = 0.0;
	opt_work_item_t item;
	item.command = NULL;

	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	if (MASTER == my_rank) {
		log_error
		    ("Not the worker process, yet the worker() function was called.");
		return;
	}
    MPI_Get_processor_name(processorname, &len);
    processorname[len] = '\0';
	log_trace("taskfarm.c: Entered worker function on node %s.", processorname);

	/* Minimal tasks for workers:
	 * - Receive first work item from master, and the base set of things to
	 *   do:
	 *   - This is a set of commands to be run *in order* and corresponds to
	 *   the clean, build and two tests.  If we were happy to all read from the
	 *   file at once, it wouldn't matter, but we shouldn't do that probably;
	 *   the parallel file system will suffer.
	 * - Request work from master;
	 *   - This is just a large string with the FLAGS in it, which allows
	 *   setting an environment variable before each command is run.
	 *   NB We don't know how big this is, so we may need to use MPI_Mprobe
	 *   here too.
	 * - Use fork/execve to run each command.
	 * - Monitor spawned process; note exit code.
	 * - On successful exit of all commands, return to master with results of
	 *   execution of benchmark (ie execution time).
	 * - If any command is unsuccessful, report failure (execution time of Inf)
	 * - Clean up when signal is received, or if NULL work item is received
	 */

	/* Receive the first work item */
	log_trace("taskfarm.c: Worker probing for first work item");
	opt_task_receive_work(&item);
	log_debug("taskfarm.c: Received first work item from master");

	/* Do the work */
	while (!stop_work) {
		retval = prologue(command_format, item.command, &time);
		if (retval == 0) {
			/* Run the benchmark */
			retval = benchmark(command_format, item.command, &time);
		}

        if (retval != 0) {
            log_info("One of our commands appears to have failed (non-zero exit status).");
            time = DBL_MAX;
        }

		/* 
		 * Report results to MASTER and receive the next work item.
		 * Repeat until no work remains.
		 *
		 * Ideally the tag will be the UID of the particle position that we
		 * were evaluating the fitness of for SPSO.
		 */
		if (stop_work) {
			log_trace
			    ("taskfarm.c: Told to stop work while running tests. Dropping results.");
			break;
		}
		log_trace("taskfarm.c: Sending fitness %lf back to master",
			  time);
		/* TODO
		 * If this is a blocking send, we could have a deadlock here if we
		 * are sent a STOP message from the master before we can MPI_Recv */
		MPI_Send(&time, 1, MPI_DOUBLE, MASTER, OPT_TASK_MSG_TAG,
			 MPI_COMM_WORLD);

		log_trace("taskfarm.c: Waiting for next work item from master");
		opt_task_receive_work(&item);
	}
	free(item.command);
	item.command = NULL;
	opt_task_clean_up();
}

void opt_task_start(void)
{
	int my_rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	if (config == NULL) {
		log_fatal("Cannot start task farm without config");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}
	if (size < 2) {
		log_fatal("Cannot start task farm at least one worker (so a minimum of 2 MPI ranks)");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}

	if (MASTER == my_rank) {
		master();
	} else {
		worker();
	}
}
