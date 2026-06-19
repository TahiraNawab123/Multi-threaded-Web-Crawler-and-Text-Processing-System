#ifndef QUEUE_H
#define QUEUE_H
#include <pthread.h>
#define URL_QUEUE_SIZE 5
#define CHUNK_QUEUE_SIZE 30
#define MAX_URL_LEN 512

// Circular queue shared between threads
typedef struct{
    void **buf;
    int capacity;
    int head, tail, count;
    pthread_mutex_t lock;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
    int closed;
} BoundedQueue;
// Items travelling through the queues
typedef struct{
    char url[MAX_URL_LEN];
} 
UrlItem;
typedef struct{
    char *text; // heap-allocated word chunk, caller frees
    int word_count;
} 
ChunkItem;
// Queue operations
BoundedQueue *queue_create(int capacity);
void queue_destroy(BoundedQueue *q);
void queue_push(BoundedQueue *q, void *item);
void *queue_pop(BoundedQueue *q);
void queue_close(BoundedQueue *q);
#endif

