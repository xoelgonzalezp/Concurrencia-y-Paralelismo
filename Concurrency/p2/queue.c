#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "queue.h"

typedef struct _queue {
    int size;
    int used;
    int first;
    int all_finished;
    void **data;
    pthread_mutex_t *mutex;
    pthread_cond_t *can_produce;
    pthread_cond_t *can_consume;
} _queue;

queue q_create(int size) {
    queue q = malloc(sizeof(_queue));
    q->size = size;
    q->used = 0;
    q->first = 0;
    q->all_finished = 0;
    q->data = malloc(size * sizeof(void *));
    q->mutex = malloc(sizeof(pthread_mutex_t));
    q->can_produce = malloc(sizeof(pthread_cond_t));
    q->can_consume = malloc(sizeof(pthread_cond_t));
    pthread_mutex_init(q->mutex, NULL);
    pthread_cond_init(q->can_produce, NULL);
    pthread_cond_init(q->can_consume, NULL);
    return q;
}


int q_elements(queue q) {
    int elements = q->used;
    return elements;
}

int q_insert(queue q, void *elem, int thread) {
    pthread_mutex_lock(q->mutex);

    if (q->used == q->size) {

        if(thread != -1){
        printf("Full queue. Thread %d waiting.\n", thread);
        }

    }
    while (q->used == q->size) {
        pthread_cond_wait(q->can_produce, q->mutex);
    }

    int index = (q->first + q->used) % q->size; 
    q->data[index] = elem; 
    q->used++; 

    if(q->used==1){
        pthread_cond_broadcast(q->can_consume);
    } 
    pthread_mutex_unlock(q->mutex); 

    return 1; 
}

void q_set_all_finished(queue q) {
    pthread_mutex_lock(q->mutex);
    q->all_finished = 1;
    pthread_cond_broadcast(q->can_consume);
    pthread_mutex_unlock(q->mutex);
}

int q_finished(queue q){ return q->all_finished;}

void *q_remove(queue q, int thread) {
    pthread_mutex_lock(q->mutex);
    if (q->used == 0 && !q->all_finished) {

        if(thread != -1){
            
            printf("Empty queue. Thread %d waiting.\n", thread);

        }
  
    }
    while (q->used == 0 && !q->all_finished) {
        
        pthread_cond_wait(q->can_consume, q->mutex);
    }
    if (q->used == 0 && q->all_finished) {
        if(thread != -1){
            printf("Thread %d: Finishing execution\n",thread);
        }
        pthread_mutex_unlock(q->mutex);
        return NULL;
    }
   
    void *elem = q->data[q->first];
    q->first = (q->first + 1) % q->size;
    q->used--;
    if(q->used== q->size - 1){
    pthread_cond_broadcast(q->can_produce); 
    }
    pthread_mutex_unlock(q->mutex);
    return elem;
}

void q_destroy(queue q) {
    pthread_mutex_destroy(q->mutex);
    pthread_cond_destroy(q->can_produce);
    pthread_cond_destroy(q->can_consume);
    free(q->data);
    free(q->mutex);
    free(q->can_produce);
    free(q->can_consume);
    free(q);
}