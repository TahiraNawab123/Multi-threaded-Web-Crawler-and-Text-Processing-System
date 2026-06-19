#ifndef DOWNLOADER_H
#define DOWNLOADER_H
#include "queue.h"
#include <pthread.h>
// Argument for the single URL-reader thread 
typedef struct{
    const char *url_file;
    BoundedQueue *url_queue;
} URLReaderArgs;

// Argument for each downloader thread
typedef struct{
    BoundedQueue *url_queue;
    BoundedQueue *chunk_queue;
    int chunk_size;
    double *dl_time_acc;
    pthread_mutex_t *dl_time_lock;
} 
DownloaderArgs;
// Called once before spawning downloaders
void downloader_init(int num_downloaders);
void *url_reader_thread(void *arg);
void *downloader_thread(void *arg);
#endif
