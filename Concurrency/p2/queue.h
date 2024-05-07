#ifndef __QUEUE_H__
#define __QUEUE_H__

typedef struct _queue *queue;

queue q_create(int size);          
int   q_elements(queue q);          
int   q_insert(queue q, void *elem, int thread); 
void *q_remove(queue q, int thread);            
void  q_destroy(queue q);           
void  q_set_all_finished(queue q);
int   q_finished(queue q);

#endif