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

#include "common.h"

bool file_exists(const char *filename)
{
	if (access(filename, F_OK) != -1) {
		return true;
	}
	return false;
}

/* XXX TODO FIXME
 * Annoyingly, this is not working, especially on some file systems, and is
 * giving a false negative, so I have made it a no-op until I can get around
 * to fixing it.
 */
bool is_valid_filename(const char *filename)
{
	/* Use dirname(3), realpath(3) and stat(2) to check.
	 * Ie call dirname(filename), realpath(filename) and then stat to check
	 * that the euid of this process can create files in that directory */
	int retval = false;
	char *tmp = NULL;
	char *dirpath = NULL;
	char *directory = NULL;
	struct stat dirstat;
	uid_t euid = geteuid();
	gid_t egid = getegid();

	log_debug("This process is running with EUID = '%u' and EGID = '%u'.",
		  euid, egid);

	if (filename == NULL || strlen(filename) < 1) {
		return false;
	}
	/* TODO FIXME */
	return true;

	tmp = strdup(filename);
	directory = dirname(tmp);
	log_debug("Dirname = '%s'.", directory);
	dirpath = realpath(directory, NULL);

	/* TODO Would be nice to issue a warning if we're about to overwrite a
	 * file. */
	if (!errno && dirpath != NULL) {
		stat(dirpath, &dirstat);
		if (errno) {
			log_error("Unable to stat output file path.");
			retval = false;
		} else if (S_ISDIR(dirstat.st_mode)) {
			if (dirstat.st_uid == euid
			    && (dirstat.st_mode & S_IWUSR)
			    && (dirstat.st_mode & S_IXUSR)) {
				log_debug
				    ("Process owner is directory owner and has write access");
				retval = true;
			} else if (dirstat.st_gid == egid
				   && (dirstat.st_mode & S_IWGRP)
				   && (dirstat.st_mode & S_IXGRP)) {
				log_debug
				    ("Process group owner is directory group owner and has write access");
				retval = true;
			} else if ((S_IWOTH & dirstat.st_mode)
				   && (S_IXOTH & dirstat.st_mode)) {
				log_debug("World has write access");
				retval = true;
			} else {
				log_error
				    ("You don't seem to have permission to write to that path.");
				retval = false;
			}
		} else {
			log_error("This path seems unlikely to work.");
			retval = false;
		}
	}

	/* Clean up */
	free(tmp);
	free(dirpath);
	return retval;
}

/**
 * Same as waitpid(2) but kill process group for pid after timeout secs.
 * Returns 0 for valid status in pstatus, -1 on failure of waitpid(2).
 *
 * Originally from the function of the same name in SLURM's run_script.c:
 * https://github.com/SchedMD/slurm/blob/63a06811441dd7882083c282d92ae6596ec00a8a/src/slurmd/common/run_script.c
 */
int waitpid_timeout(const char *name, pid_t pid, int *pstatus, int timeout)
{
	int timeout_ms = 1000 * timeout;	/* timeout in ms */
	const int max_delay = 1000;	/* max delay between waitpid calls */
	int delay = 10;		/* initial delay */
	int rc;
	int options = WNOHANG;
	/*
	 * TODO Use setrlimit or GNU's prlimit to set sensible memory restrictions
	 * (which should be something user defined) on child processes.
	 */

	log_trace("Entered waitpid_timeout function.");

	if (timeout <= 0)
		options = 0;

    errno = 0;
	do {
        rc = waitpid(pid, pstatus, options);
		if (rc < 0) {
			if (errno == EINTR) {
				continue;
            } else if (errno == ECHILD) {
                log_error("waitpid: Process specified by PID %d does not exist or is not a child of this process (or child process sigaction is set to SIG_IGN)", pid);
            } else if (errno == EINVAL) {
                log_error("waitpid: Invalid options given to waitpid");
            }
			return (-1);
		} else if (timeout_ms <= 0) {
			log_info("%s%stimeout after %ds: killing pgid %d",
				 name != NULL ? name : "",
				 name != NULL ? ": " : "", timeout, pid);
			killpg(pid, SIGKILL);
			options = 0;
		} else {
			(void)poll(NULL, 0, delay);
			timeout_ms -= delay;
			delay = MIN(timeout_ms, MIN(max_delay, delay * 2));
		}
	} while (rc <= 0);

	killpg(pid, SIGKILL);	/* kill children too */
	return (0);
}


int waitid_timeout(const char *name, id_t pid, siginfo_t * info, int timeout)
{
	int timeout_ms = 1000 * timeout;	/* timeout in ms */
	const int max_delay = 1000;	/* max delay between waitpid calls */
	int delay = 10;		/* initial delay */
	int rc;
	int options = WEXITED | WNOHANG;
    idtype_t type = P_PID;

	log_trace("Entered waitid_timeout function.");

	if (timeout <= 0)
		options = 0;

    errno = 0;
	info->si_pid = 0;
    info->si_status = 0;
    info->si_signo = SIGCHLD;
	do {
        rc = waitid(type, pid, info, options);
		log_trace("waitid returned %d: info->si_pid is %d, info->si_status is %d", rc, info->si_pid, info->si_status);
		if (rc < 0) {
			if (errno == EINTR) {
				continue;
            } else if (errno == ECHILD) {
                log_error("waitid: Process specified by PID %d does not exist, is not a child of this process, or its disposition is set to SIG_IGN", pid);
            } else if (errno == EINVAL) {
                log_error("waitid: Invalid options given to waitid");
            }
			return (-1);
        } else if (rc == 0 && info->si_pid != 0) {
			if (info->si_code == CLD_EXITED) {
				log_debug("Looks like child process exited");
				break;
			}
			if (info->si_code == CLD_KILLED || info->si_code == CLD_DUMPED) {
				log_debug("Child process killed by signal %d", info->si_status);
                return (-1);
            }
		}
		if (timeout_ms <= 0) {
			log_info("%s%stimeout after %ds: killing pgid %d",
				 name != NULL ? name : "",
				 name != NULL ? ": " : "", timeout, pid);
			killpg(pid, SIGKILL);
            /* This allows the calling function to know that the child exited
             * because it was killed and not for some other reason. */
            info->si_code = CLD_KILLED;
            info->si_status = SIGKILL;
			options = 0;
			break;
		} else {
			(void)poll(NULL, 0, delay);
			timeout_ms -= delay;
			delay = MIN(timeout_ms, MIN(max_delay, delay * 2));
		}
	} while (rc <= 0);

	killpg(pid, SIGKILL);	/* kill children too */
	return (0);
}

/* The way that we are timing execution isn't the best */
int run_command(const char *command, double *time, int timeout)
{
	pid_t cpid;
	double start_time, end_time;
    siginfo_t info;

	log_trace("Entered run_command function.");
	
	if (command == NULL) {
		log_error("Cannot run NULL command");
		return (-1);
	}
	log_trace("run_command(): Attempting to fork child to run command %s with timeout %d", command, timeout);

	/* TODO
	 * - Kill child if we receive the quit_signal.
	 *      Less worrying as this process will get reaped along with any
	 *      spawned children, but it would be nice to finish more cleanly.
	 */
	if ((cpid = vfork()) < 0) {
		log_fatal("Unable to fork child process");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}
	/* TODO
	 * - Set child process's oom_adj score to 14 (the highest score
	 *   possible is 15) so that it is likely to be reaped first if we run
	 *   out of memory. (Manually, this is done by updating the value in
	 *   /proc/$cpid/oom_adj)
	 */
	start_time = MPI_Wtime();
	if (0 == cpid) {
		/* We are the child */
		setpgid(cpid, 0);
		errno = 0;
		/* exec the command */
		/* This should not return  if successful */
		execl(SHELL, "sh", "-c", command, NULL);
		log_error("Error running command: %s", strerror(errno));
	}
	/* waitid_timeout kills child process group if time limit exceeded
	 *      ** This is important! **
	 * Unfortunately, processes are quite likely to overrun or fail to
	 * terminate if there is a compiler optimisation bug.
	 */
	if (waitid_timeout(command, cpid, &info, timeout) < 0) {
		*time = DBL_MAX;
		return (-1);
	}
	end_time = MPI_Wtime();
	*time = end_time - start_time;

	log_debug("run_command: Got status %d and duration %lf", info.si_status, *time);

	if (info.si_code == CLD_EXITED) {
		return info.si_status;
	} else if (info.si_code == CLD_KILLED) {
		log_info("Spawned child process was terminated by signal");
	} else if (info.si_code == CLD_DUMPED) {
		log_info("Spawned child process was terminated by signal and dumped core");
	} else if (info.si_code == CLD_STOPPED) {
        log_info("Spawned child process was stopped by a signal");
    } else if (info.si_code == CLD_TRAPPED) {
        log_info("Spawned child process was traced and trapped");
    }
	return -1;
}

int opt_get_signum(char *signame)
{
	if (signame == NULL) {
		log_fatal("NULL signal name provided in configuration");
		MPI_Abort(MPI_COMM_WORLD, -1);
	}
	if (!strcmp("SIGQUIT", signame) || !strcmp("QUIT", signame))
		return SIGQUIT;
	if (!strcmp("SIGUSR1", signame) || !strcmp("USR1", signame))
		return SIGUSR1;
	if (!strcmp("SIGUSR2", signame) || !strcmp("USR2", signame))
		return SIGUSR2;
	if (!strcmp("SIGINT", signame) || !strcmp("INT", signame))
		return SIGINT;
	if (!strcmp("SIGSTOP", signame) || !strcmp("STOP", signame))
		return SIGSTOP;
	if (!strcmp("SIGCONT", signame) || !strcmp("CONT", signame))
		return SIGCONT;

	log_fatal
	    ("Invalid signal name provided.  Can only use SIGQUIT, SIGINT, SIGUSR1 or SIGUSR2.");
	MPI_Abort(MPI_COMM_WORLD, -1);

	return -1;		/* To avoid compiler warnings.  We never get here */
}
