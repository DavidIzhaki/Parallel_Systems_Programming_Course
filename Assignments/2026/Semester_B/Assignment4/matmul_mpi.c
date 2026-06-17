

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "matrix.h"

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Check that all command line arguments are provided
    if (argc != 5) {
        if (rank == 0) {
            printf("Usage: mpirun -np <P> ./matmul <N> <seedA> <seedB> <maxValue>\n");
        }
        MPI_Finalize();
        return 1;
    }

    int N = atoi(argv[1]);
    uint64_t seedA = (uint64_t)atol(argv[2]);
    uint64_t seedB = (uint64_t)atol(argv[3]);
    int maxValue = atoi(argv[4]);

    IMatrix A, B, C;
    
    // All processes need to allocate space for matrix B in full
    B = imatrix_alloc(N); 

    // Process 0 allocates and initializes A and C
    if (rank == 0) {
        A = imatrix_alloc(N);
        C = imatrix_alloc(N);
        imatrix_fill_random(&A, seedA, maxValue);
        imatrix_fill_random(&B, seedB, maxValue);
    }

    // Broadcasting matrix B (its data) from rank 0 to all
    MPI_Bcast(B.data, N * N, MPI_INT, 0, MPI_COMM_WORLD);

    // Computing helper arrays for MPI_Scatterv and MPI_Gatherv only in Root
    int *sendcounts = NULL;
    int *displs = NULL;
    if (rank == 0) {
        sendcounts = (int*)malloc(size * sizeof(int));
        displs = (int*)malloc(size * sizeof(int));
        for (int i = 0; i < size; i++) {
            int r_start = (i * N) / size;
            int r_end = ((i + 1) * N) / size;
            sendcounts[i] = (r_end - r_start) * N;
            displs[i] = r_start * N;
        }
    }

    // Computing boundaries and quantities for the current process (according to the formula in the assignment)
    int my_start_row = (rank * N) / size;
    int my_end_row = ((rank + 1) * N) / size;
    int my_num_rows = my_end_row - my_start_row;
    int my_elem_count = my_num_rows * N;

    // Allocating memory for local block of A and local result of C
    int *local_A = (int*)malloc(my_elem_count * sizeof(int));
    int *local_C = (int*)malloc(my_elem_count * sizeof(int));

    // Distributing rows of A to all processes. The sender uses A.data
    int *A_data_ptr = (rank == 0) ? A.data : NULL;
    MPI_Scatterv(A_data_ptr, sendcounts, displs, MPI_INT, local_A, my_elem_count, MPI_INT, 0, MPI_COMM_WORLD);

    // Local computation of matrix multiplication - The standard triple-loop
    for (int i = 0; i < my_num_rows; i++) {
        for (int j = 0; j < N; j++) {
            int sum = 0;
            for (int k = 0; k < N; k++) {
                sum += local_A[i * N + k] * B.data[k * N + j];
            }
            local_C[i * N + j] = sum;
        }
    }

    // Gathering the computed parts of C back to C.data in process 0
    int *C_data_ptr = (rank == 0) ? C.data : NULL;
    MPI_Gatherv(local_C, my_elem_count, MPI_INT, C_data_ptr, sendcounts, displs, MPI_INT, 0, MPI_COMM_WORLD);

    // Process 0 computes Checksum, prints and frees what only it allocated
    if (rank == 0) {
        long long checksum = imatrix_checksum(&C);
        printf("checksum(C)=%lld\n", checksum);

        imatrix_free(&A);
        imatrix_free(&C);
        free(sendcounts);
        free(displs);
    }

    // Freeing memory allocated by all processes
    imatrix_free(&B);
    free(local_A);
    free(local_C);

    MPI_Finalize();
    return 0;
}