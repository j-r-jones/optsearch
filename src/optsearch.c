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

#include "optsearch.h"

int main(int argc, char **argv)
{
    char procname[MPI_MAX_PROCESSOR_NAME] = {'\0'};
    int namelen = 0;
	int my_rank, num_ranks;
	long total_available_ram = 0;
	signed int opt = 0;
	int option_index = 0;
	char tmp[MAX_FILENAME_SIZE + 1] = { '\0' };
	char *outfilename = NULL;
	char *config_file = NULL;
	FILE *output = NULL;
	opt_config_t * config = NULL;

	static int verbose_flag;
	const char *short_opts = "Vhvdo:c:";
	static struct option long_opts[] = {
		/* These options set a flag. */
		{"version", no_argument, 0, 'V'},
		{"verbose", no_argument, &verbose_flag, 'v'},
		/* These options don’t set a flag.
		   We distinguish them by their indices. */
		{"help", no_argument, 0, 'h'},
		{"debug", no_argument, 0, 'd'},
		{"out", required_argument, 0, 'o'},
		{"conf", required_argument, 0, 'c'}
	};

	MPI_Init(&argc, &argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &num_ranks);
    MPI_Get_processor_name(procname, &namelen);
    procname[namelen] = '\0';
    printf("Rank %d of %d starting on processor %s\n", my_rank, num_ranks, procname);
    fflush(stdout);

    if (num_ranks < 2) {
        perror("ERROR: Cannot run with fewer than 2 MPI ranks\n");
        return EXIT_FAILURE;
    }

	log_initialise(NULL);

	if (MASTER == my_rank) {
		if (argc < 2) {
			printUsage();
			return EXIT_FAILURE;
		}

		while (true) {
			opt =
			    getopt_long(argc, argv, short_opts, long_opts,
					&option_index);

			/* Run out of options */
			if (opt == -1)
				break;

			switch (opt) {
			case 'V':
				printVersion();
				/* TODO Make sure we didn't already read int the config or
				 * something */
				MPI_Finalize();
				return EXIT_SUCCESS;
				break;
			case 'h':
				printHelp();
				/* TODO Make sure we didn't already read int the config or
				 * something */
				MPI_Finalize();
				return EXIT_SUCCESS;
				break;
			case 'v':
				turn_on_logging();
				break;
			case 'd':
				if (get_log_level() < LOG_LEVEL_DEBUG)
					set_log_level(LOG_LEVEL_DEBUG);
				break;
			case 'o':
				/* This is intended to be for the log file and recording of
				 * search result */
				if (is_valid_filename(optarg)) {
					/*
					 * This malloc and sprintf'ing is to avoid
					 * clobbering by MPI ranks.  Tmp is to avoid the
					 * problems of optarg not ending in '\0'.
					 */
					/* XXX TODO FIXME segfault on access of optarg */
					strncat(tmp, optarg, MAX_FILENAME_SIZE);
					outfilename =
					    malloc((12 +
						    strlen(tmp)) *
						   sizeof(char));
					sprintf(outfilename, "%s-%d", tmp,
						my_rank);
					//outfilename = strndup(optarg, MAX_FILENAME_SIZE);
				} else {
					log_error
					    ("Invalid output filename '%s'  specified.",
					     optarg);
					exit(EXIT_FAILURE);
				}
				break;
			case 'c':
				if (is_valid_filename(optarg)) {
					config_file =
					    strndup(optarg, MAX_FILENAME_SIZE);
				} else {
					/* Handle the error */
					log_error
					    ("Invalid config filename '%s'  specified.",
					     optarg);
					exit(EXIT_FAILURE);
				}
				/* Read in YAML config file to opt_config_t struct */
				config = opt_new_config();
				read_config(config_file, config);
				/* TODO check it worked and handle error case */
				break;
			default:	/* '?' */
				log_error("Unrecognised option, %s.", optarg);
				log_error
				    ("Usage: %s [-d ] [-o outputfile] -c <config file>",
				     argv[0]);
				return EXIT_FAILURE;
			}
		}
		errno = 0;
		if (outfilename != NULL) {
			output = fopen(outfilename, "a");
			log_set_outstream(output);
		}

		errno = 0;
		/* Check how much memory is available to us.
		 * TODO Check that this works correctly with cgroups/cpusets */
		total_available_ram =
		    sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE);
		if (errno || total_available_ram == -1) {
			log_warn
			    ("WARNING: Unable to determine available memory size.  This program may crash if the image being generated is too large.");
			/*return EXIT_FAILURE; */
		}
	}

	if (MASTER != my_rank) {
		if (config == NULL)
			config = opt_new_config();
		if (config->quit_signal == NULL)
			config->quit_signal = strdup("");
		config->clean_script = strdup("");
		config->build_script = strdup("");
		config->accuracy_test = strdup("");
		config->perf_test = strdup("");
	}

	/* This barrier is probably unnecessary */
	MPI_Barrier(MPI_COMM_WORLD);
	opt_share_config(MASTER, config);

	/* This should terminate on its own, either by converging or if a signal
	 * is received (config->quit_signal)*/
	opt_start_search(config);

	/* Clean up now we're finished */
	if (output != NULL)
		fclose(output);

	if (MASTER == my_rank) {
		free(outfilename);
		free(config_file);
	}
	opt_destroy_config(config);

	MPI_Finalize();
	return EXIT_SUCCESS;
}

void printVersion()
{
	const char *version = "OptSearch 0.1a";
	printf("%s\n", version);
	printf("Copyright (C) 2007 Free Software Foundation, Inc.\n");
	printf
	    ("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n");
	printf
	    ("This is free software: you are free to change and redistribute it.\n");
	printf("There is NO WARRANTY, to the extent permitted by law.\n");
}

void printUsage()
{
	/* TODO This should be a bit more detailed */
	printf("Usage: optsearch [-d ] [-o outputfile] -c <config file>\n");
}

void printHelp()
{
	/*printf("Please report bugs to: optsearch@Jessica Jones.org\n");
	   printf("pkg home page: <http://github.com/Jessica Jones/optsearch>\n"); */
	printUsage();
}

/* TODO
 * Listen for a particular signal, and checkpoint & gracefully shut down,
 * printing out our best found solution(s) so far.
 *
 * If Torque:
 * Suggest that the user make use of the signal resource (requested with -l
 * as with walltime, etc).  From the Torque documentation:
 *
 *   signal=<signal>@<# of seconds> : will ask Torque to send the signal
 *   <signal> to the running job <# of seconds> before it reaches the walltime
 *   limit. This option is useful only for programs that can do checkpointing.
 * 
 * If SLURM:
 * SLURM has the option "--signal", which works in the same way.
 *
 * It seems that a lot of established applications with support for
 * checkpointing use this method to determine when they should checkpoint and
 * stop.
 *
 * We need to pick a signal.  Possibly SIGTERM, which is by default sent a
 * few moments before SIGKILL, but the delay is unlikely to be long enough
 * by default.  Something like SIGQUIT is a better idea.  At this point,
 * with the top500 list being 96% Linux, we probably don't have to worry
 * about BSD or AIX signals, at least not yet.
 *
 */

/* TODO
 * How big will our SQLite DB get, if we're storing the information about all
 * our particles/searches at each time step?
 * Is it impractical?  Can we make sure that there is the option of writing
 * the data to, say, /dev/shm and then copying it to somewhere more permanent
 * when we do a formal checkpoint?  The checkpoint itself would then be
 * a case of duplicating the SQLite DB to somewhere else.
 *
 * Alaric, who used SQLite at Rainstor as the metadata storage for their
 * rather larger, distributed RDBMS, thinks that my proposed abuse will work
 * quite well with SQLite.  Even with one huge table, it should be fine.
 *
 * Back of the envelope calculations by both him and JHD suggest that space
 * won't be a big problem, but we can always prune a bit during the run if
 * need be.  Perhaps making that an option (or not pruning could be the
 * option) for the user would be a good thing.
 */
