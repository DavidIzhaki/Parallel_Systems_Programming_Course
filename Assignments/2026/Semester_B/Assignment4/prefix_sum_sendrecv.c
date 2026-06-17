

#include <stdio.h>
#include <mpi.h>

int main(int argc, char** argv) {
    // Initializing MPI environment
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int x = rank;          // The initial value of the process
    int prefix = x;        // The cumulative value that we'll print at the end

    // If we are not process 0, we need to receive the cumulative sum from the previous process
    if (rank > 0) {
        int received_sum;
        MPI_Recv(&received_sum, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        prefix = received_sum + x;
    }

    // If we are not the last process, we need to send the cumulative sum to the next process
    if (rank < size - 1) {
        MPI_Send(&prefix, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
    }

    // Printing the result in the exact format required
    printf("rank=%d x=%d prefix=%d\n", rank, x, prefix);

    // Finalizing MPI environment
    MPI_Finalize();
    return 0;
}