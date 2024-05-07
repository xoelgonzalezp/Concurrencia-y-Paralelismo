#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

//This function retrieves user input for the number of intervals to use in PI approximation, if the process has rank 0,
//it prompts the user to enter an interval and sends this value to all other processes. 
//Other processes wait to receive the interval value from process 0.
int getinput(int rank, int numprocs) {
    int n,error;
    if (rank == 0) { //If the rank is 0, we ask the user to introduce the number of intervals, and we store it in "n".
        printf("Process %d: Enter the number of intervals: (0 quits) \n", rank);
        scanf("%d", &n);
        
        for (int dest = 1; dest < numprocs; dest++) { //After obtaining n, we send it to all other processes. We start at 1 to avoid sending it to process 0.
            error = MPI_Send(&n, 1, MPI_INT, dest, 0, MPI_COMM_WORLD); //We send the value of n to process with rank "dest". &n represents the memmory address of the value we
                                                               //are going to send, 1 represents the number of elements we are sending, MPI_INT represent the data type, in this
                                                               // is an integer, dest is the rank of the process to which the message is sent, 0 is the message label and 
                                                               //MPI_COMM_WORLD represents the communicator which includes all the processes in execution.  

            if (error != MPI_SUCCESS) {
                perror("Error while sending number of intervals from Process 0\n");
                MPI_Abort(MPI_COMM_WORLD, error); 
                exit(EXIT_FAILURE);
            }                               
        }
    } else {
        MPI_Status status; //We create a variable status which is used to store information about the state of the message reception.
        error = MPI_Recv(&n, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status); //If the process is not process 0, it receives the message from process 0. &n represents
                                                             //the memmory address in which the value will be stored, 1 represents the number of elements this process is going
                                                            //to receive,MPI_INT represents the data type of the value the process is going to receive, 0 represents the rank of
                                                            //the process from which the process is going to receive the message, in this case is 0 as it will be sent from process 0.
                                                            //The message label is 0, it corresponds with the label used in the sending process.MPI_COMM_WORLD represents the communicator
                                                            //that includes all processes in execution and the memmory address of the status variable, which is used to store the information
                                                            //about message reception.

        if (error != MPI_SUCCESS) {
            perror("Error while receiving number of intervals in a process\n");
            MPI_Abort(MPI_COMM_WORLD, error); 
            exit(EXIT_FAILURE);
        }
    }
    return n;
}



// This function computes the approximated value of pi using the integration method. It computes the partial results in each process
//and then computes the final approximated value in process with rank 0, and this process shows the user the final approximation value.
void compute_pi(int rank, int numprocs, int n) {
    double PI25DT = 3.141592653589793238462643;
    double pi, h, sum, x, mypi; //pi value, interval width, sum of partial areas,point of evaluation and the result of each process.
    MPI_Status status;
    pi = 0.0; 
    sum = 0.0;
    
    if (n != 0) { //if the user introduced an interval different thant zero.
        h = 1.0 / (double)n; //we compute the interval width by dividing 1.0 by the number of intervals introduced. This is made to define de size of each subinterval, the 
                             //pi integration method consists of dividing interval [0,1] in n subintervals of the same size.
                             
        for (int i = rank; i < n; i += numprocs) {   // This loop calculates the sum of the areas of the rectangles under the curve of the function
                                                     // 4 / (1 + x^2) within the subintervals assigned to each process. It initializes i to the rank value of the current process.
                                                    // Each process will start from its rank number and increment i by the number of processes (numprocs).
            x = h * ((double)i + 0.5); // Calculates the midpoint of the current subinterval (i) and multiplies it by the width of the subinterval (h). This gives the x-coordinate of the midpoint of the rectangle under the curve.
            sum += 4.0 / (1.0 + x*x); // Calculates the value of the function 4 / (1 + x^2) at the midpoint x and adds it to the sum. 
            
        }
        mypi = h * sum;// Calculates the approximation of pi by multiplying the sum of the areas of the rectangles (sum) by the width of each subinterval (h). This represents the area under the curve, which is used to approximate the value of pi.
  
        if (rank != 0) {
            int error = MPI_Send(&mypi, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD); 
            if (error != MPI_SUCCESS) {
                perror("Error while sending partial result from a process\n");
                MPI_Abort(MPI_COMM_WORLD, error);
                exit(EXIT_FAILURE);
            }
        } else {
            double partial_results[numprocs]; 
            partial_results[0] = mypi;
            printf("Process %d: partial approximation of %.16f\n", rank, mypi);

            
            for (int i = 1; i < numprocs; i++) {
                int error = MPI_Recv(&mypi, 1, MPI_DOUBLE, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
                if (error != MPI_SUCCESS) {
                perror("Error while sending partial result from a process\n");
                MPI_Abort(MPI_COMM_WORLD, error);
                exit(EXIT_FAILURE);
                }
                printf("Process %d: received from process %d partial approximation: %.16f\n", rank, status.MPI_SOURCE, mypi);
                partial_results[status.MPI_SOURCE] = mypi; 
            }

            double pi = partial_results[0];
            for (int i = 1; i < numprocs; i++) {
                pi += partial_results[i];
            }

            printf("Process %d: Final approximation of pi is approx. %.16f, Error: %.16f\n", rank, pi, fabs(pi - PI25DT)); 
        }

    }
}

int main(int argc, char *argv[]) {

    int numprocs, rank, n;
    MPI_Init(&argc, &argv); //we set MPI environment taking into account the arguments passed.
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs); //this function returns the number of proccesses in the communicator, and this value is assigned to numprocs. 
                                              //MPI_COMM_WORLD is the main communicator in an MPI environment and it stores the total number of processes that are being runned.
                                       
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // This function returns the rank (identifier) of the calling process within the MPI_COMM_WORLD communicator.

    while (1) {
        n = getinput(rank, numprocs); 

        if (n == 0) break; //If the input is 0, we stop the infinite loop.

        compute_pi(rank, numprocs, n); //This function computes the approximated value of PI using the interval given by the user. All processes will compute their part
                                       //of the approximation band will send it to Process 0, which will compute the final approximated value of PI.
    }

    MPI_Finalize(); //This function finishes MPI environment, closing all communication channels, frees resources.
    return 0;
}