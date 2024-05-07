#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

int getinput(int rank, int numprocs) {
    int n;
    if (rank == 0) {
        printf("Process %d: Enter the number of intervals: (0 quits) \n", rank);
        scanf("%d", &n);
    }
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD); //Collective function to send from the n value from root process(0) to all of the processes fo the communicator.
    return n;
}

void compute_pi(int rank, int numprocs, int n) {
    double PI25DT = 3.141592653589793238462643;
    double h, sum = 0.0, x, mypi, global_pi;

    if (n != 0) {
        h = 1.0 / (double)n;
        for (int i = rank; i < n; i += numprocs) {
            x = h * ((double)i + 0.5);
            sum += 4.0 / (1.0 + x*x);
        }
        mypi = h * sum;

        MPI_Reduce(&mypi, &global_pi, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD); //Collective function to recollect partial_results in global_pi(only process 0 has access to global_pi)

        if (rank == 0) {
            printf("Process %d: Final approximation of pi is approx. %.16f, Error: %.16f\n", rank, global_pi, fabs(global_pi - PI25DT)); 
        }
    }
}

int main(int argc, char *argv[]) {
    int numprocs, rank, n;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    while (1) {
        n = getinput(rank, numprocs);
        if (n == 0) break;
        compute_pi(rank, numprocs, n);
    }

    MPI_Finalize();
    return 0;
}
