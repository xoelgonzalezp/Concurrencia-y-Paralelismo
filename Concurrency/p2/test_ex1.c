#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "queue.h"

#define QUEUE_SIZE 15
#define PRODUCER_ITERATIONS 30000
#define NUM_PRODUCERS 10
#define NUM_CONSUMERS 2

int last_produced_number = 0;
int producers_finished = 0;
pthread_mutex_t mutex;

typedef struct {
    queue q;
    int thread_id;
} thread_arg;

void *producer(void *arg) {
    thread_arg *targ = (thread_arg *)arg;
  
    for (int i = 0; i < PRODUCER_ITERATIONS; i++) {
        
        int *data = malloc(sizeof(int));
        pthread_mutex_lock(&mutex);
        *data = last_produced_number++;
        pthread_mutex_unlock(&mutex);
        printf("Thread %d Iteration %d: producing %d\n", targ->thread_id,i, *data);
        q_insert(targ->q, data, targ->thread_id); 
        usleep(1000);
    }
    pthread_mutex_lock(&mutex);
    producers_finished++;
    if (producers_finished == NUM_PRODUCERS) {
        q_set_all_finished(targ->q);
    }
    pthread_mutex_unlock(&mutex);
    free(targ);
    return NULL;
}

void *consumer(void *arg) {
    thread_arg *targ = (thread_arg *)arg;

    while (1) {
        int *data = q_remove(targ->q, targ->thread_id);
        if (data == NULL) {
            if (q_finished(targ->q)) {
              
                break;
            } else {
             
                continue;
            }
        }
        printf("Thread %d: consuming %d\n", targ->thread_id, *data);
        free(data);
        usleep(1000); 
    }

    free(targ);
    return NULL;
}


// Producer Threads
pthread_t *start_producer_threads(queue q) {
    pthread_t *producers = malloc(sizeof(pthread_t) * NUM_PRODUCERS);
    if (producers == NULL) {
        perror("Failed to allocate memory for producer threads");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        thread_arg *arg = malloc(sizeof(thread_arg));
        if (arg == NULL) {
            perror("Failed to allocate memory for thread arguments");
            exit(EXIT_FAILURE);
        }
        arg->q = q;
        arg->thread_id = i + 1; 
        if (pthread_create(&producers[i], NULL, producer, arg) != 0) {
            perror("Failed to create producer thread");
            exit(EXIT_FAILURE);
        }
    }

    return producers;
}


//Consumer Threads
pthread_t *start_consumer_threads(queue q) {
    pthread_t *consumers = malloc(sizeof(pthread_t) * NUM_CONSUMERS);
    if (consumers == NULL) {
        perror("Failed to allocate memory for consumer threads");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        thread_arg *arg = malloc(sizeof(thread_arg));
        if (arg == NULL) {
            perror("Failed to allocate memory for thread arguments");
            exit(EXIT_FAILURE);
        }
        arg->q = q;
        arg->thread_id = i + 1; 
        if (pthread_create(&consumers[i], NULL, consumer, arg) != 0) {
            perror("Failed to create consumer thread");
            exit(EXIT_FAILURE);
        }
    }

    return consumers;
}

int main() {
    pthread_t *producers, *consumers;
    queue q = q_create(QUEUE_SIZE);

    pthread_mutex_init(&mutex, NULL);

    producers = start_producer_threads(q);
    consumers = start_consumer_threads(q);

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }

    q_destroy(q); 
    free(producers); 
    free(consumers); 
    pthread_mutex_destroy(&mutex);
    return 0;
}