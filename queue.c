#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

BoundedQueue *queue_create(int capacity)
{
    BoundedQueue *q = malloc(sizeof(BoundedQueue));
    q->buf      = malloc(sizeof(void *) * capacity);
    q->capacity = capacity;
    q->head = q->tail = q->count = q->closed = 0;
    pthread_mutex_init(&q->lock,      NULL);
    pthread_cond_init (&q->not_full,  NULL);
    pthread_cond_init (&q->not_empty, NULL);
    return q;
}

void queue_destroy(BoundedQueue *q)
{
    if (!q) return;
    free(q->buf);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy (&q->not_full);
    pthread_cond_destroy (&q->not_empty);
    free(q);
}

void queue_push(BoundedQueue *q, void *item)
{
    pthread_mutex_lock(&q->lock);
    while (q->count == q->capacity)
        pthread_cond_wait(&q->not_full, &q->lock);
    q->buf[q->tail] = item;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

void *queue_pop(BoundedQueue *q)
{
    pthread_mutex_lock(&q->lock);
    while (q->count == 0 && !q->closed)
        pthread_cond_wait(&q->not_empty, &q->lock);
    if (q->count == 0) {
        pthread_cond_signal(&q->not_empty);
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }
    void *item = q->buf[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return item;
}

void queue_close(BoundedQueue *q)
{
    pthread_mutex_lock(&q->lock);
    q->closed = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}
