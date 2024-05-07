#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>



int MPI_FlattreeCollective(double *input, double *total, int count, MPI_Datatype datatype, MPI_Op operation, int root, MPI_Comm comm) {
 
    int rank, numprocs;
    MPI_Comm_rank(comm, &rank);  // We get the current rank
    MPI_Comm_size(comm, &numprocs);  // We get the total number of processes
    MPI_Status status;
    int error;

    if (input == NULL || total == NULL) {  // We first check if the variables are null
        perror("Error: Variables cannot be NULL");
        MPI_Abort(comm, MPI_ERR_BUFFER); 
        exit(EXIT_FAILURE);
    }

    if (datatype != MPI_DOUBLE || count != 1) {// We check the datatype and count
        perror("Error: Unsupported datatype or count");
        MPI_Abort(comm, MPI_ERR_TYPE); 
        exit(EXIT_FAILURE);
    }

   
    if (rank == root) {  // If the current rank is root's one
        *total = *input;  // We initialize the sum with the root's own value
        double partial_results[numprocs];
        double val;

        for (int i = 1; i < numprocs; i++) { // We iterate over the total number of processes 
           
            error = MPI_Recv(&val, count, datatype, MPI_ANY_SOURCE, 0, comm, &status);
            if (error != MPI_SUCCESS) return error;
            partial_results[status.MPI_SOURCE] = val; // Add the received value from the process to the sum
        }
        for (int i = 1; i < numprocs; i++) {
            *total += partial_results[i];
        }
    } else {
        
        error = MPI_Send(input, count, datatype, root, 0, comm); // Other processes send their value to the root
        if (error != MPI_SUCCESS) return error;
    }

    return MPI_SUCCESS;
}






int MPI_BinomialCollective(void *data, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
    int error, i, numprocs, rank;
    MPI_Status status;
    MPI_Comm_size(comm, &numprocs);
    MPI_Comm_rank(comm, &rank);

    int iterations = 0;
    int temp = numprocs;
    while (temp > 1) { 
        temp >>= 1; //Moves 1 bit to the right
        iterations++; //Total number of iterations, it represents logarithm in base 2 of the number of processes
    }
    if (numprocs & (numprocs - 1)) { //Additional iteration, AND  operation, if the result is different than 0000, we perfom an additional operation
        iterations++; 
    }

    for (i = 1; i <= iterations; i++) {
        int power = 1 << (i - 1); 
        if (rank < power) {
            if ((rank + power) < numprocs) {
                error = MPI_Send(data, count, datatype, (rank + power), 0, comm);
                if (error != MPI_SUCCESS) return error;
                printf("Iteration %d: Process %d sending to process %d data\n", i, rank, (rank + power));
            }
        } else {
           
            if (rank < (1 << i)) { 
                error = MPI_Recv(data, count, datatype, (rank - power), 0, comm, &status);
                if (error != MPI_SUCCESS) return error;
                printf("Iteration %d: Process %d receiving from process %d data\n", i, rank, (rank - power));
            }
        }
    }

    return MPI_SUCCESS;
}


int getinput(int rank) {
    int n;
    if (rank == 0) {
        printf("Process %d: Enter the number of intervals: (0 quits) \n", rank);
        scanf("%d", &n);
    }
   
    MPI_BinomialCollective(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    return n;
}

void compute_pi(int rank, int numprocs, int n) {
    double PI25DT = 3.141592653589793238462643;
    double pi, h, sum = 0.0, x, mypi;

    if (n != 0) {
        h = 1.0 / (double)n;
        for (int i = rank; i < n; i += numprocs) {
            x = h * ((double)i + 0.5);
            sum += 4.0 / (1.0 + x * x);
        }
        mypi = h * sum;

        MPI_FlattreeCollective(&mypi, &pi, 1, MPI_DOUBLE,MPI_SUM, 0, MPI_COMM_WORLD);

        if (rank == 0) {
            printf("Process %d: Final approximation of pi is approx. %.16f, Error: %.16f\n", rank, pi, fabs(pi - PI25DT));
        }
    }
}

int main(int argc, char *argv[]) {
    int numprocs, rank, n;
    MPI_Init(&argc, &argv);
 
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    while (1) {
        n = getinput(rank);
        if (n == 0) break;
        compute_pi(rank, numprocs, n);
    }

    MPI_Finalize();
    return 0;
}