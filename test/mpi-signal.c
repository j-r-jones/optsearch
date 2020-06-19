#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <mpi.h>

int stop;

void opt_sig_handler(int signo, siginfo_t *sinfo, void *context)
{
    int rank, size, len;
    char name[MPI_MAX_PROCESSOR_NAME] = { '\0' };
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Get_processor_name(name, &len);
    printf("Rank %d of %d running on %s received signal %d\n", rank, size, name, signo);

	stop = 1;
}

int main (int argc, char ** argv)
{
    struct sigaction act;
	stop = 0;

    MPI_Init(&argc, &argv);

	memset(&act, 0, sizeof(struct sigaction));
	sigemptyset(&act.sa_mask);
	act.sa_sigaction = opt_sig_handler;
	act.sa_flags = SA_SIGINFO;

    if (-1 == sigaction(SIGCONT, &act, NULL)) {
        /* This probably ought to be printed by log_fatal */
        perror("Unable to register signal handler with sigaction()");
		MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGUSR2, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGSTOP, &act, NULL);
    sigaction(SIGINT, &act, NULL);

	while (!stop) {
		sleep(5);
	}

    MPI_Finalize();
    return EXIT_SUCCESS;
}
