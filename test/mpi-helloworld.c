#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>

int main (int argc, char ** argv)
{
    int rank, size, len;
    char name[MPI_MAX_PROCESSOR_NAME] = { '\0' };
    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Get_processor_name(name, &len);

    printf("Hello from rank %d of %d running on %s\n", rank, size, name);

    MPI_Finalize();
    return EXIT_SUCCESS;
}
