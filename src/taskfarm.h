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

#ifndef H_OPTSEARCH_TASKFARM
#define H_OPTSEARCH_TASKFARM

#include "common.h"
#include <sys/times.h>

#include "config.h"

struct opt_task_work_item_s {
	int uid;
	char *command;
	struct opt_task_work_item_s *next;
};

typedef struct opt_task_work_item_s opt_work_item_t;

typedef enum opt_header_position_e {
	OPT_HEADER_TYPE = 0,
	OPT_HEADER_UID = 1,
	OPT_HEADER_SIZE = 2,
	OPT_HEADER_SEQ = 3,
} opt_header_position;

/** Values to use for MPI_TAG.  Actual numeric values are arbitrary choices
 * given to make debugging output more legible. */
typedef enum opt_task_tag_e {
	OPT_TASK_HEADER_TAG = 6,
			     /** Headers give information on what is coming in the message to follow */
	OPT_TASK_MSG_TAG = 4,
			  /** Signifies a message (body) rather than a header is being sent/received */
} opt_task_tag_t;

/**
 * Values to use to signify the type of message being sent.  Actual numeric
 * values are arbitrary choices given to make debugging output more legible.
 */
typedef enum opt_task_msg_e {
	OPT_TASK_WORK_MSG = 9,
	OPT_TASK_RESULT_MSG = 8,
	OPT_TASK_STOP_MSG = 7,
} opt_task_msg_t;

/**
 * Initialise the task farm.
 *
 * This takes a pointer to an opt_config_t object, from which we obtain the
 * values for the commands to be run, and a function pointer to the function
 * to be called once a fitness value has been established for a particular
 * run.  This function takes a UID for the particle in PSO that provided this
 * work item, and a double representing the fitness for that point.
 *
 * @param config the opt_config_t
 * @param report_fitness a pointer to the function to call to report back the
 * fitness score
 *
 */
int opt_task_initialise(opt_config_t * config,
			int (*report_fitness) (const int, double, int));

/**
 * Signal that the taskfarm should stop waiting for more work, and stop
 * processing anything remaining in the queue.
 */
int opt_task_stop(void);

/**
 * Free used memory, kill spawned processes of workers (if any remain).
 */
int opt_task_clean_up(void);

/**
 * Adds a work item to the queue.
 *
 * This function takes the UID (usually this corresponds to the PSO particle
 * that generated this work item), the pointer to string representing the
 * command or work item itself.  This string MUST be NULL terminated.
 *
 * @param work_item a string representing the command to run
 * @param work_item_uid a UID to use when returning the fitness value
 * @return the queue size after adding this new item
 */
int opt_queue_push(const int work_item_uid, const char *work_item);

/**
 * Send a work message to a worker.
 * This should only be invoked by the master rank.
 */
int opt_task_send_to_worker(int worker, opt_task_msg_t type,
			    opt_work_item_t * item);

/**
 * Receive a work message from the master.
 * This should only be invoked by the worker ranks.
 */
opt_task_msg_t opt_task_receive_work(opt_work_item_t * item);

/**
 * Start the task farm.
 */
void opt_task_start(void);

#endif				/* include guard H_OPTSEARCH_TASKFARM */
