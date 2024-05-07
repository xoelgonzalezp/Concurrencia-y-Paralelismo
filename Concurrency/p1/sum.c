
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"


struct nums {
    long *increase;
    long *decrease;
    long total;
    long sum;
    int size;
    pthread_mutex_t *mutexes_increase;
    pthread_mutex_t *mutexes_decrease;
};

struct iterations {
    int i_thread0; // Iterations thread type 0 (increase_decrease)
    int i_thread1; // Iterations thread type 1 (increment shift)
    int i_thread2; // Iterations thread type 2 (decrement shift)
    pthread_mutex_t mutex;
};

struct args {
    int thread_num;
    struct nums *nums;
    struct iterations *iterations;
};

struct thread_info {
    pthread_t id;
    struct args *args;
};


// Thread type 0
void *decrease_increase(void *ptr) {
    struct args *args = ptr;
    struct nums *n = args->nums;
    struct iterations *iters = args->iterations;
    int index_inc, index_dec;

    srand(time(NULL) ^ args->thread_num);

    while (1) {
        pthread_mutex_lock(&iters->mutex);
        if (iters->i_thread0 <= 0) {
            pthread_mutex_unlock(&iters->mutex);
            break;
        }
        iters->i_thread0--;
        long total_increase = 0, total_decrease = 0;
        index_inc = rand() % n->size; // Random positions selected on each array
        index_dec = rand() % n->size;

        pthread_mutex_lock(&n->mutexes_increase[index_inc]);
        pthread_mutex_lock(&n->mutexes_decrease[index_dec]);

        n->decrease[index_dec]--; // We make a decrement in the selected position
        n->increase[index_inc]++; // We make an increment in the selected position


        pthread_mutex_unlock(&n->mutexes_increase[index_inc]);
        pthread_mutex_unlock(&n->mutexes_decrease[index_dec]);

        for (int i = 0; i < n->size; i++) {
            total_increase += n->increase[i];
            total_decrease += n->decrease[i];
        }

        long sum_iter =total_increase + total_decrease;
        if (sum_iter == n->sum) {
            printf("Thread %d Type 0 : increment in position %d, decrement in position %d, increment: %ld decrement: %ld, total: %ld\n",
                   args->thread_num, index_inc, index_dec, n->increase[index_inc], n->decrease[index_dec],
                   sum_iter); // In each iteration it is specified the thread, the position of the increment array in which the increment is done,
            // the position of the decrement array where the decrement is done, the current values for those positions and
            // the sum of the values of both arrays, it must be (total * array_size) at any given time
        }else{
            printf("Error: Total must be %ld, obtained %ld ",n->sum,sum_iter);
        }
        pthread_mutex_unlock(&iters->mutex);
        usleep(1000);
    }
    return NULL;
}

// Thread type 1
void *increment_shift(void *ptr) {
    struct args *args = ptr;
    struct nums *n = args->nums;
    struct iterations *iterations = args->iterations;
    int index_inc, index_dec;

    srand(time(NULL) ^ args->thread_num);

    while (1) {
        pthread_mutex_lock(&iterations->mutex);
        if (iterations->i_thread1 <= 0) {
            pthread_mutex_unlock(&iterations->mutex);
            break;
        }
        iterations->i_thread1--;
        long total_increase = 0, total_decrease = 0;

        index_dec = rand() % n->size; // Random positions selected on each array
        index_inc = rand() % n->size;

        pthread_mutex_lock(&n->mutexes_increase[index_inc]);
        if (index_inc != index_dec) {
            pthread_mutex_lock(&n->mutexes_increase[index_dec]);
        }

        n->increase[index_dec]--;
        n->increase[index_inc]++;

        pthread_mutex_unlock(&n->mutexes_increase[index_inc]);
        if (index_inc != index_dec) {
            pthread_mutex_unlock(&n->mutexes_increase[index_dec]);
        }

        for (int i = 0; i < n->size; i++) {
            total_increase += n->increase[i];
            total_decrease += n->decrease[i];
        }

        long sum_iter =total_increase + total_decrease;
        if (sum_iter == n->sum) {
            printf("Thread %d Type 1 : increment in position %d, decrement in position %d, increment: %ld, decrement: %ld,  total: %ld \n",
                   args->thread_num, index_inc, index_dec, n->increase[index_inc], n->increase[index_dec], sum_iter);
        }else{
            printf("Error: Total must be %ld, obtained %ld ",n->sum,sum_iter);
        }

        pthread_mutex_unlock(&iterations->mutex);
        usleep(1000);
    }
    return NULL;
}

// Thread type 2
void *decrement_shift(void *ptr) {
    struct args *args = ptr;
    struct nums *n = args->nums;
    struct iterations *iterations = args->iterations;
    int index_inc, index_dec;

    srand(time(NULL) ^ args->thread_num);

    while (1) {
        pthread_mutex_lock(&iterations->mutex);
        if (iterations->i_thread2 <= 0) {
            pthread_mutex_unlock(&iterations->mutex);
            break;
        }
        iterations->i_thread2--;
        long total_increase = 0, total_decrease = 0;

        index_dec = rand() % n->size; // Random positions selected on each array
        index_inc = rand() % n->size;

        pthread_mutex_lock(&n->mutexes_decrease[index_inc]);
        if (index_inc != index_dec) {
            pthread_mutex_lock(&n->mutexes_decrease[index_dec]);
        }

        n->decrease[index_dec]--;
        n->decrease[index_inc]++;

        pthread_mutex_unlock(&n->mutexes_decrease[index_inc]);
        if (index_inc != index_dec) {
            pthread_mutex_unlock(&n->mutexes_decrease[index_dec]);
        }

        for (int i = 0; i < n->size; i++) {
            total_increase += n->increase[i];
            total_decrease += n->decrease[i];
        }

        long sum_iter =total_increase + total_decrease;
        if (sum_iter == n->sum) {
            printf("Thread %d Type 2 : increment in position %d, decrement in position %d, increment: %ld, decrement: %ld,  total: %ld \n",
                   args->thread_num, index_inc, index_dec, n->decrease[index_inc], n->decrease[index_dec], sum_iter);
        }else{
            printf("Error: Total must be %ld, obtained %ld ",n->sum,sum_iter);
        }

        pthread_mutex_unlock(&iterations->mutex);
        usleep(1000);
    }
    return NULL;
}

// Thread type 0
struct thread_info *start_threads(struct options opt, struct nums *nums, struct iterations *iters) {

    int i;
    struct thread_info *threads = malloc(sizeof(struct thread_info) * opt.num_threads);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    for (i = 0; i < opt.num_threads; i++) {
        threads[i].args = malloc(sizeof(struct args));
        threads[i].args->thread_num = i;
        threads[i].args->nums = nums;
        threads[i].args->iterations = iters;

        if (0 != pthread_create(&threads[i].id, NULL, decrease_increase, threads[i].args)) {
            printf("Could not create thread #%d\n", i);
            exit(1);
        }
    }

    return threads;
}

// Thread type 1
// Each thread has access to its respective arguments stored in the stack of the main thread
void start_threads_increment_shift(struct options opt, struct nums *nums, struct iterations *iters, struct thread_info *threads, struct args *argsArray) {
    int i;
    for (i = 0; i < opt.num_threads; i++) {
        argsArray[i].thread_num = i;
        argsArray[i].nums = nums;
        argsArray[i].iterations = iters;
        threads[i].args = &argsArray[i];

        if (0 != pthread_create(&threads[i].id, NULL, increment_shift, &argsArray[i])) {
            printf("Could not create shift thread #%d\n", i);
            exit(1);
        }
    }
}


// Thread type 2
struct thread_info *start_threads_decrement_shift(struct options opt, struct nums *nums, struct iterations *iters) {
    int i;
    struct thread_info *threads = malloc(sizeof(struct thread_info) * opt.num_threads);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    for (i = 0; i < opt.num_threads; i++) {
        threads[i].args = malloc(sizeof(struct args));
        threads[i].args->thread_num = i;
        threads[i].args->nums = nums;
        threads[i].args->iterations = iters;

        if (0 != pthread_create(&threads[i].id, NULL, decrement_shift, threads[i].args)) {
            printf("Could not create thread #%d\n", i);
            exit(1);
        }
    }

    return threads;
}

void print_totals(struct nums *nums) {
    long total_increase = 0, total_decrease = 0;
    for (int i = 0; i < nums->size; i++) { // We iterate over each array and compute the sum with counters
        total_increase += nums->increase[i];
        total_decrease += nums->decrease[i];
    }
    printf("Final: increased %ld, decreased %ld, total: %ld \n", total_increase, total_decrease, total_increase+total_decrease); // We compute the total result
}

void wait(struct options opt, struct nums *nums, struct thread_info *threads0, struct thread_info *threads1,struct thread_info *threads2) {

    for (int i = 0; i < opt.num_threads; i++) {
        pthread_join(threads0[i].id, NULL);
    }
    for (int i = 0; i < opt.num_threads; i++) {
        pthread_join(threads1[i].id, NULL);
    }

    for (int i = 0; i < opt.num_threads; i++) {
        pthread_join(threads2[i].id, NULL);
    }

    print_totals(nums);

    for (int i = 0; i < opt.num_threads; i++) { //Free array of arguments for increase_decrease threads
        free(threads0[i].args);
    }

    for (int i = 0; i < opt.num_threads; i++) { //Free array of arguments for decrement_shift threads
        free(threads2[i].args);
    }

    free(threads0); //Free array of increase_decrease threads
    free(threads2); //Free array of decrement_shift threads

}

void initialize(struct options opt, struct nums *nums) {

    // Increase and decrease arrays initialization
    // Size of both arrays is given by parameter -s
    nums->increase = calloc(opt.size, sizeof(long));
    nums->decrease = calloc(opt.size, sizeof(long));

    nums->total = opt.iterations;

    for (int i = 0; i < opt.size; i++) {
        nums->decrease[i] = nums->total; // Each position of the decrease array is initialized to total, which is the number of iterations
    }

    nums->size = opt.size;
    nums->sum = nums ->total * nums ->size; //At each iteration and on the final result it is required to obtain total * array_size

    // We allocate memory for the array of mutexes used for the increase and decrease arrays
    nums->mutexes_increase = malloc(opt.size * sizeof(pthread_mutex_t));
    nums->mutexes_decrease = malloc(opt.size * sizeof(pthread_mutex_t));

    for (int i = 0; i < opt.size; i++) { // We initialize the increase and decrease mutexes at position i on each array
        pthread_mutex_init(&nums->mutexes_increase[i], NULL);
        pthread_mutex_init(&nums->mutexes_decrease[i], NULL);
    }

}

void free_resources(struct options opt, struct nums *nums){

    free(nums->increase); //Free array of increase counters
    free(nums->decrease); //Free array of decrease counters

    for (int i = 0; i < opt.size; i++) { //We destroy on each array the mutex on each position
        pthread_mutex_destroy(&nums->mutexes_increase[i]);
        pthread_mutex_destroy(&nums->mutexes_decrease[i]);
    }

    free(nums->mutexes_increase);
    free(nums->mutexes_decrease);

}

int main(int argc, char **argv) {
    struct options opt;
    struct nums nums;
    struct iterations iterations;
    struct thread_info *threads0,*threads2;

    // Default values for the options
    opt.num_threads  = 4;
    opt.iterations   = 100;
    opt.size         = 10;

    struct args args_array1[opt.num_threads];
    struct thread_info threads1[opt.num_threads];

    read_options(argc, argv, &opt);
    initialize(opt, &nums);

    iterations.i_thread0 = opt.iterations;
    iterations.i_thread1 = opt.iterations;
    iterations.i_thread2 = opt.iterations;

    pthread_mutex_init(&iterations.mutex, NULL);
    // parts 1-3
    threads0 = start_threads(opt, &nums, &iterations);
    // part 4
    start_threads_increment_shift(opt, &nums, &iterations, threads1, args_array1);
    // part 5
    threads2 = start_threads_decrement_shift(opt,&nums,&iterations);

    wait(opt, &nums, threads0, threads1,threads2);

    free_resources(opt, &nums);

    pthread_mutex_destroy(&iterations.mutex);

    return 0;
}