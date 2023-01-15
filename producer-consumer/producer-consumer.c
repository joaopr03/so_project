#include "producer-consumer.h"
#include <stdio.h>
#include <stdlib.h>

int pcq_create(pc_queue_t *queue, size_t capacity) {
    queue->pcq_capacity = capacity;
    queue->pcq_current_size = 0;
    queue->pcq_head = 0;
    queue->pcq_tail = 0;

    if (pthread_mutex_init(&queue->pcq_current_size_lock, NULL) != 0) {
        return -1;
    }
    if (pthread_mutex_init(&queue->pcq_head_lock, NULL) != 0) {
        return -1;
    }
    if (pthread_mutex_init(&queue->pcq_tail_lock, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&queue->pcq_pusher_condvar, NULL) != 0) {
        return -1;
    }
    if (pthread_mutex_init(&queue->pcq_pusher_condvar_lock, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&queue->pcq_popper_condvar, NULL) != 0) {
        return -1;
    }
    if (pthread_mutex_init(&queue->pcq_popper_condvar_lock, NULL) != 0) {
        return -1;
    }
    return 0;
}

int pcq_destroy(pc_queue_t *queue) {
    queue->pcq_capacity = 0;
    for (size_t i = 0; i < queue->pcq_current_size ; i++) {
        free(queue->pcq_buffer[i]);
    }
    queue->pcq_current_size = 0;
    if (pthread_mutex_destroy(&queue->pcq_current_size_lock) != 0) {
        return -1;
    }
    queue->pcq_head = 0;
    if (pthread_mutex_destroy(&queue->pcq_head_lock) != 0) {
        return -1;
    }
    queue->pcq_tail = 0;
    if (pthread_mutex_destroy(&queue->pcq_tail_lock) != 0) {
        return -1;
    }
    if (pthread_cond_destroy(&queue->pcq_pusher_condvar) != 0) {
        return -1;
    }
    if (pthread_mutex_destroy(&queue->pcq_pusher_condvar_lock) != 0) {
        return -1;
    }
    if (pthread_cond_destroy(&queue->pcq_popper_condvar) != 0) {
        return -1;
    }
    if (pthread_mutex_destroy(&queue->pcq_popper_condvar_lock) != 0) {
        return -1;
    }
    return 0;
}

int pcq_enqueue(pc_queue_t *queue, void *elem) {
    if (pthread_mutex_lock(&queue->pcq_head_lock) != 0) {
        return -1;
    }
    while (queue->pcq_current_size == queue->pcq_capacity) {
        if (pthread_cond_wait(&queue->pcq_pusher_condvar, &queue->pcq_pusher_condvar_lock) != 0) {
            return -1;
        }
    }
    if (pthread_mutex_lock(&queue->pcq_current_size_lock) != 0) {
        return -1;
    }
    queue->pcq_current_size++;
    queue->pcq_tail = queue->pcq_current_size-1;
    for (size_t i = queue->pcq_tail; i > 0 ; i--) {
        queue->pcq_buffer[i] = queue->pcq_buffer[i-1];
    }
    queue->pcq_buffer[0] = elem;

    if (pthread_cond_signal(&queue->pcq_popper_condvar) != 0) {
        return -1;
    }
    if (pthread_mutex_unlock(&queue->pcq_current_size_lock) != 0) {
        return -1;
    }
    if (pthread_mutex_unlock(&queue->pcq_head_lock) != 0) {
        return -1;
    }
    return 0;
}

void *pcq_dequeue(pc_queue_t *queue) {
    if (pthread_mutex_lock(&queue->pcq_tail_lock) != 0) {
        return NULL;
    }
    while (queue->pcq_current_size == 0) {
        if (pthread_cond_wait(&queue->pcq_popper_condvar, &queue->pcq_popper_condvar_lock) != 0) {
            return NULL;
        }
    }
    if (pthread_mutex_lock(&queue->pcq_current_size_lock) != 0) {
        return NULL;
    }

    //free(queue->pcq_buffer[queue->pcq_tail]);
    queue->pcq_current_size--;
    queue->pcq_tail--;
    
    if (pthread_cond_signal(&queue->pcq_pusher_condvar) != 0) {
        return NULL;
    }
    if (pthread_mutex_unlock(&queue->pcq_current_size_lock) != 0) {
        return NULL;
    }
    if (pthread_mutex_unlock(&queue->pcq_tail_lock) != 0) {
        return NULL;
    }
    return queue->pcq_buffer[queue->pcq_tail+1];
} 
