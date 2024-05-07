#include <stdio.h>
#include <sys/time.h>
#include <mpi.h>
#include <stdlib.h>

#define DEBUG 1
#define N 100


//Each process should only allocate memory for the corresponding part, each process shouldnt allocate memory for the whole matrix because
//it is not gonna use it, so it should be computed by the number of rows that process needs, in order to allocate the correct memory for that process

void initialize(float **matrix, float *vector, float **out_vector, int numprocs, int rank) {
    int i, j; 
    if (rank == 0) { // If process is process 0, this process initializes the matrix
        *matrix = malloc(sizeof(float) * N * N); //We allocate memory for a matrix of NxN elements
        *out_vector = malloc(sizeof(float) * N);    //Output vector where each process will deposit its partial results in the corresponding section, we allocate memory for N elements
        
        if (*matrix == NULL || *out_vector == NULL) {
            perror("Failed to allocate memory for matrix or output vector.\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            exit(EXIT_FAILURE);
        }
        
        for (i = 0; i < N; i++) {
            vector[i] = i; 
            (*out_vector)[i] = 0.0;
            float *row = *matrix + i * N; 
            for (j = 0; j < N; j++) {
                row[j] = i + j; 
            }
        }
    }
}


float distribute_vector(float *vector) {
    struct timeval start_communication, end_communication; //We initialize variables to measure the start and end time of the communication time while doing the collective operation
    float communication_time;

    gettimeofday(&start_communication, NULL); //With this routine, we store in the communication start variable the actual time(which stores the start of the diffusion time)
    int error = MPI_Bcast(vector, N, MPI_FLOAT, 0, MPI_COMM_WORLD);  //As input vector was initialized by process 0, we distribute this array from root process(0) to the rest of processes, in the communicator, N indicates the number of elements
                                                        // that are gonna be sent to each process, and the datatype of each element of type MPI_FLOAT
    gettimeofday(&end_communication, NULL); //We call again the routine once the collective is done, to register the end of the distribution of the input vector, and we store it in the end variable for communication time  
    if (error != MPI_SUCCESS) {
        perror("Error in MPI_Bcast.\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        exit(EXIT_FAILURE);
    }
    communication_time = (end_communication.tv_usec - start_communication.tv_usec) + 1000000 * (end_communication.tv_sec - start_communication.tv_sec); //We store in communication_time the total difference between the start and end of the distribution operation.
                                                                                                        //To do this, we first compute the difference between the end and start times in microseconds to obtain the decimal part, so we get
                                                                                                    //more precission, and we also compute the diference of this variables in seconds, to obtain the integer part, then we convert thar part
                                                                                                    //to microseconds and sum it to the first part,obtaining the decimal and integer part in microseconds, which gives us the total communication_time in microseconds

    return communication_time;
}



void distribute_rows(int numprocs, int rank, int *rows, float **partial_matrix, float **partial_out_vector) {
    int base_rows = N / numprocs; // Base rows that all processes will have as guarantee
    int remainder = N % numprocs; // We check if the number of processes is a multiple of N

    if (rank < remainder) { //Processes that are lower than the remainder will perform an additional row
        *rows = base_rows + 1;
    } else {
        *rows = base_rows; //Processes that are equal or higher than the remainder will do the base rows stablished.
    }
    *partial_matrix = malloc(sizeof(float) * N * (*rows)); 
    *partial_out_vector = malloc(sizeof(float) * (*rows)); 
    if (*partial_matrix == NULL || *partial_out_vector == NULL) {
        perror("Failed to allocate memory for partial arrays.\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        exit(EXIT_FAILURE);
    }
}



float scatter_matrix(int rank, int numprocs, int rows, float* matrix, float* partial_matrix, int** block_sizes, int** offset) {
    struct timeval start_communication, end_communication;
    float communication_time = 0.0;

    if (rank == 0) { //As we will send fragments of the matrix from process 0 to the rest of processes in the communicator, with scatterv we can declare the number of elements and the offset of the matrix from which each process will receive
                    //its number of elements, so we declare two arrays for the number of elements and the offset where we have to take from the global matrix that number of elements, then, for the number of processes we compute the number of
                    //elements each process will receive and the position from where these elements will be taken in the global matrix.
        int mv = 0;
        *block_sizes = malloc(numprocs * sizeof(int));
        *offset = malloc(numprocs * sizeof(int));
        if (*block_sizes == NULL || *offset == NULL) {
            perror("Failed to allocate memory for block_sizes or offset.\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < numprocs; i++) {
            int elements = (N / numprocs + (i < N % numprocs ? 1 : 0)) * N;
            (*block_sizes)[i] = elements;
            (*offset)[i] = mv;
            mv += elements;
        }
    }

    gettimeofday(&start_communication, NULL);
    int error = MPI_Scatterv(matrix, *block_sizes, *offset, MPI_FLOAT, partial_matrix, rows * N, MPI_FLOAT, 0, MPI_COMM_WORLD);
    gettimeofday(&end_communication, NULL);
    if (error != MPI_SUCCESS) {
        perror("Error in MPI_Scatterv.\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        exit(EXIT_FAILURE);
    }
    communication_time = (end_communication.tv_usec - start_communication.tv_usec) + 1000000 * (end_communication.tv_sec - start_communication.tv_sec);

    return communication_time;
}



float compute_product(int rows, float *partial_matrix, float *vector, float *partial_out_vector) {
    struct timeval computation_start, computation_end;
    float computation_time;

    gettimeofday(&computation_start, NULL); 

    for (int i = 0; i < rows; i++) {  //For each process based on its number of rows previously assigned, we will obtain its partial output vector, which is of size N, to do this, we perform the product of the 
                                      //partial matrix * input global vector, partial vectors will be then recollected into process 0's output vector
        float *row = partial_matrix + i * N;
        partial_out_vector[i] = 0.0; 
        for (int j = 0; j < N; j++) {
            partial_out_vector[i] += row[j] * vector[j]; //We multiply each element by the corresponding element into the input vector for that matrix row and we put the accumulated sum of that row i at position i of the partial vector 
        }
    }

    gettimeofday(&computation_end, NULL);  
    computation_time = (computation_end.tv_usec - computation_start.tv_usec) + 1000000 * (computation_end.tv_sec - computation_start.tv_sec);

    return computation_time;
}



float gather_vector(int rank, int numprocs, int *block_sizes, int *offset, float *partial_out_vector, float *out_vector, int rows) {
    struct timeval start_communication, end_communication;
    float communication_time = 0.0;

    if (rank == 0) { //As we have to gather all of the partial vectors from all processes in process 0, we do the inverse now, block_sizes now will be then number of positions based on the rows for that process that will receive root process
                     //on its output vector, this guarantees that the output global vector is equal to N, also the offset will start at 0 and will indicate where for that process its values have to be inserted in the output vector 
        int acc_offset = 0; 
        for (int i = 0; i < numprocs; i++) {
            int rows_process = block_sizes[i] / N; 
            block_sizes[i] = rows_process;
            offset[i] = acc_offset; 
            acc_offset += rows_process; 
        }
    }

    gettimeofday(&start_communication, NULL);
    int error = MPI_Gatherv(partial_out_vector, rows, MPI_FLOAT, out_vector, block_sizes, offset, MPI_FLOAT, 0, MPI_COMM_WORLD); //For each partial vector, we are gonna gather in the result vector for process 0 "rows" elements corresponding
                                                                                                           //to that partial vector, the total elements (sum of all row variables) will be the total number of rows of the original matrix
                                                                                                           //(N)
    gettimeofday(&end_communication, NULL);
    if (error != MPI_SUCCESS) {
        perror("Error in MPI_Gatherv.\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        exit(EXIT_FAILURE);
    }
    communication_time = (end_communication.tv_usec - start_communication.tv_usec) + 1000000 * (end_communication.tv_sec - start_communication.tv_sec);

    return communication_time;
}


void free_resources(int rank, float *matrix, float *out_vector, int *block_sizes, int *offset, float *partial_matrix, float *partial_out_vector) {
    if (rank == 0) {
        if (matrix != NULL) free(matrix);
        if (out_vector != NULL) free(out_vector);
        if (block_sizes != NULL) free(block_sizes);
        if (offset != NULL) free(offset);
    }
    if (partial_matrix != NULL) free(partial_matrix);
    if (partial_out_vector != NULL) free(partial_out_vector);
}



int main(int argc, char *argv[] ) {
    int i, numprocs, rank, rows;
    int *block_sizes = NULL; 
    int *offset = NULL; 
    float vector[N];
    float *matrix = NULL;
    float *out_vector = NULL;
    float *partial_matrix = NULL;
    float *partial_out_vector = NULL;
    float computation_time, communication_time;
  
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    initialize(&matrix, vector, &out_vector, numprocs, rank);    

    communication_time = distribute_vector(vector); 
    distribute_rows(numprocs, rank, &rows, &partial_matrix, &partial_out_vector);  
    communication_time += scatter_matrix(rank, numprocs, rows, matrix, partial_matrix, &block_sizes, &offset);
    computation_time = compute_product(rows, partial_matrix, vector, partial_out_vector);
    communication_time += gather_vector(rank, numprocs, block_sizes, offset, partial_out_vector, out_vector, rows);

    if (DEBUG) {
          if(rank==0){
            for(i=0;i<N;i++) {
                printf(" %f \t ",out_vector[i]);
            }
            printf("\n");
        }
    } else {
        printf("Process %d : Communication time = %lf seconds, Computation time = %lf seconds\n", rank, (double) communication_time / 1E6, (double) computation_time / 1E6);
    }

    free_resources(rank, matrix, out_vector, block_sizes, offset, partial_matrix, partial_out_vector);
    MPI_Finalize();
    return 0;
}