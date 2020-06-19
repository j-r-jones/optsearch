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

#ifndef H_OPTSEARCH_COMMON_
#define H_OPTSEARCH_COMMON_
/*
 * _GNU_SOURCE is required for getopt_long()
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <libgen.h>
#include <getopt.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <features.h>

#include <mpi.h>

#include "logging.h"

/* TODO Check if this test is doing what we want */
#if _POSIX_C_SOURCE > 199506L
#include <stdbool.h>
#else
typedef enum { false = 0, true = 1 } bool;
#endif				/* Test for C99 or later */

/* The master rank */
#define MASTER 0

#define SHELL "/bin/sh"

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#define MAX_FILENAME_SIZE PATH_MAX

#define MAX_PARAM_VALUE INT_MAX

/* The number of characters in INT_MAX and DBL_MAX so we can allocate storage
 * space for strings. TODO This will be platform specific, so we should do
 * this more intelligently. */
#define CHAR_INT_MAX 11
#define CHAR_UINT_MAX 11
#define CHAR_ULONGLONG_MAX 23
#define CHAR_DBL_MAX 317

/**
 * A wrapper aound access(2) from unistd.h
 *
 * @return true if file with supplied path exists
 */
bool file_exists(const char *path);

/**
 * Takes a string and attempts to determine whether or not it points to
 * something that could be considered a valid file name.
 *
 * CURRENTLY BROKEN
 *
 * @return bool true (1) or false (0)
 */
extern bool is_valid_filename(const char *filename);

/**
 * Same as waitpid(2) but kill process group for pid after timeout secs.
 * Returns 0 for valid status in pstatus, -1 on failure of waitpid(2).
 *
 * Originally from the function of the same name in SLURM's run_script.c:
 * https://github.com/SchedMD/slurm/blob/63a06811441dd7882083c282d92ae6596ec00a8a/src/slurmd/common/run_script.c
 */
extern int waitpid_timeout(const char *name, pid_t pid, int *pstatus,
			   int timeout);

/**
 * Uses waitpid_timeout to run a command, returning the runtime of that
 * command as measured using calls to MPI_Wtime().
 *
 * @param command the command to run
 * @param exec_time where to store the execution time
 * @param timeout the timeout to use
 * @return -1 on failure, else the return value of the command
 */
int run_command(const char *command, double *exec_time, int timeout);

/**
 * Take a signal name from the small set that is valid for use with eg SLURM,
 * and convert it to the corresponding value (as per signal(7)).  Either the
 * full name, eg SIGUSR1, or the shortened form, eg USR1, are valid.
 *
 * Valid signals are SIGQUIT, SIGINT, SIGUSR1 and SIGUSR2, but you must check
 * the documentation of the scheduler you are using as these may be reserved.
 * See for example --signal in the documentation for sbatch (if using SLURM):
 * https://slurm.schedmd.com/sbatch.html
 *
 * @param signame the signal name
 * @return the signal value
 */
int opt_get_signum(char *signame);

#endif				/* include guard H_OPTSEARCH_COMMON_ */
