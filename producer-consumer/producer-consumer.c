#include "producer-consumer.h"
#include <stdio.h>
#include <stdlib.h>

//n tenho a certeza do pcq_head nem do pcq_tail o resto deve tar bem
int pcq_create(pc_queue_t *queue, size_t capacity) {
    queue->pcq_capacity = capacity;
    queue->pcq_current_size = 0;
    pthread_mutex_init(&queue->pcq_current_size_lock, NULL);
    queue->pcq_head = 0;
    pthread_mutex_init(&queue->pcq_head_lock, NULL);
    queue->pcq_tail = 0;
    pthread_mutex_init(&queue->pcq_tail_lock, NULL);
    pthread_cond_init(&queue->pcq_pusher_condvar, NULL);
    pthread_mutex_init(&queue->pcq_pusher_condvar_lock, NULL);
    pthread_cond_init(&queue->pcq_popper_condvar, NULL);
    pthread_mutex_init(&queue->pcq_popper_condvar_lock, NULL);
    return 0;
}

//n tenho a certeza do pcq_capacity, pcq_current_size, pcq_head, pcq_tail o resto deve tar bem
int pcq_destroy(pc_queue_t *queue) {
    queue->pcq_capacity = 0;
    queue->pcq_current_size = 0;
    pthread_mutex_destroy(&queue->pcq_current_size_lock);
    queue->pcq_head = 0;
    pthread_mutex_destroy(&queue->pcq_head_lock);
    queue->pcq_tail = 0;
    pthread_mutex_destroy(&queue->pcq_tail_lock);
    pthread_cond_destroy(&queue->pcq_pusher_condvar);
    pthread_mutex_destroy(&queue->pcq_pusher_condvar_lock);
    pthread_cond_destroy(&queue->pcq_popper_condvar);
    pthread_mutex_destroy(&queue->pcq_popper_condvar_lock);
    return 0;
}

//esse site deve ajudar pro queue e enqueue https://stackoverflow.com/questions/64627094/thread-safe-producer-consumer-with-shared-queue-in-c
int pcq_enqueue(pc_queue_t *queue, void *elem) {
    (void)elem; //Só pra dar pra compilar
    while (queue->pcq_current_size == queue->pcq_capacity) {
        pthread_cond_wait(&queue->pcq_pusher_condvar, &queue->pcq_pusher_condvar_lock);
    }
    pthread_mutex_lock(&queue->pcq_current_size_lock);

    queue->pcq_current_size++;

    pthread_mutex_unlock(&queue->pcq_current_size_lock);
    return 0;
}

//esse site deve ajudar pro queue e enqueue https://stackoverflow.com/questions/64627094/thread-safe-producer-consumer-with-shared-queue-in-c
/* void *pcq_dequeue(pc_queue_t *queue) {
    while (queue->pcq_current_size == 0) {
        pthread_cond_wait(&queue->pcq_popper_condvar, &queue->pcq_popper_condvar_lock);
    }
    pthread_mutex_lock(&queue->pcq_current_size_lock);

    queue->pcq_current_size--;

    pthread_mutex_unlock(&queue->pcq_current_size_lock);
} */ //nao tava a compilar
