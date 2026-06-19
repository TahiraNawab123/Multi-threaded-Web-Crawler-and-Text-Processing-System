
/*
 * main.c — Pipeline orchestration
 *
 * Stage 1  URL Reader  (1 thread)   → url_queue  (size 5)
 * Stage 2  Downloaders (D threads)  → chunk_queue (size 30)
 * Stage 3  Parsers     (P threads)  → LocalStats[]
 * Stage 4  Aggregator  (1 thread)   → GlobalStats → stdout
 *
 * Thread split:  D = T/3  (min 1),  P = T - D
 * Optional 4th argument overrides D for experiments.
 */
#include "queue.h"
#include "downloader.h"
#include "parser.h"
#include "aggregator.h"
#include "metrics.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
int main(int argc, char *argv[])
{
    if (argc != 4 && argc != 5)
    {
        fprintf(stderr,
          "Usage: %s <url_file> <total_threads> <chunk_size> [d_override]\n",
           argv[0]);
        return 1;
    }
    const char *url_file = argv[1];
    int total_threads = atoi(argv[2]);
    int chunk_size = atoi(argv[3]);
    if (total_threads < 2)
    {
        fprintf(stderr, "total_threads >= 2\n");
        return 1;
    }
    if (chunk_size < 1)
    {
        fprintf(stderr, "chunk_size >= 1\n");
        return 1;
    }
    int D, P;
    // Decide downloader and parser thread count 
    if (argc == 5)
    {
        D = atoi(argv[4]);
        if (D < 1)
            D = 1;
        if (D > total_threads - 1)
            D = total_threads - 1;
    }
    else
    {
        D = total_threads / 3;
        if (D < 1)
            D = 1;
    }
    P = total_threads - D;
    // Show current configuration
    printf("Config: T=%d  D=%d  P=%d  chunk_size=%d\n\n",
           total_threads, D, P, chunk_size);
    // Initialize libcurl before creating downloader threads
    curl_global_init(CURL_GLOBAL_ALL);
    // Let downloader module know how many downloaders exist
    downloader_init(D);
    // Record overall start time
    double wall_start = now_sec();
    // Create queues used between stages
    BoundedQueue *url_queue = queue_create(URL_QUEUE_SIZE);
    BoundedQueue *chunk_queue = queue_create(CHUNK_QUEUE_SIZE);
    // Store total downloader and parser times
    double dl_acc = 0.0, par_acc = 0.0;
    pthread_mutex_t dl_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t par_lock = PTHREAD_MUTEX_INITIALIZER;
    // Start URL reader thread
    pthread_t reader_tid;
    URLReaderArgs reader_args = {url_file, url_queue};
    pthread_create(&reader_tid, NULL, url_reader_thread, &reader_args);
    // Create downloader threads
    pthread_t *dl_tids = malloc(sizeof(pthread_t) * D);
    DownloaderArgs *dl_args = malloc(sizeof(DownloaderArgs) * D);
    for (int i = 0; i < D; i++)
    {

        dl_args[i] = (DownloaderArgs){
            url_queue, chunk_queue, chunk_size, &dl_acc, &dl_lock};

        pthread_create(&dl_tids[i], NULL, downloader_thread, &dl_args[i]);
    }
    // Create parser threads
    pthread_t *par_tids = malloc(sizeof(pthread_t) * P);
    ParserArgs *par_args = malloc(sizeof(ParserArgs) * P);
    LocalStats **locals = malloc(sizeof(LocalStats *) * P);
    for (int i = 0; i < P; i++)
    {
        locals[i] = local_stats_create();
        par_args[i] = (ParserArgs){
            chunk_queue,
            locals[i],
            &par_acc,
            &par_lock};

        pthread_create(&par_tids[i], NULL, parser_thread, &par_args[i]);
    }
    // Wait for URL reader
    pthread_join(reader_tid, NULL);
    // Wait for downloader threads
    for (int i = 0; i < D; i++)
        pthread_join(dl_tids[i], NULL);
    // Aggregation timing starts here
    double agg_start = now_sec();
    // Wait for parser threads
    for (int i = 0; i < P; i++)
        pthread_join(par_tids[i], NULL);
    // Combine results from all parser threads
    GlobalStats g;
    aggregate(&g, locals, P);
    double agg_end = now_sec();
    // Record overall end time 
    double wall_end = now_sec();
    print_global_stats(&g);
    // Calculate performance results
    double total_time = wall_end - wall_start;
    RunMetrics m = {
        .T = total_threads,
        .D = D,
        .P = P,
        .chunk_size = chunk_size,
        .total_time = total_time,
        .dl_avg = (D > 0) ? dl_acc / D : 0.0,
        .par_avg = (P > 0) ? par_acc / P : 0.0,
        .agg_time = agg_end - agg_start,
        .total_words = g.total_words,
        .throughput = (total_time > 0) ? (double)g.total_words / total_time : 0.0,
    };
    metrics_print(&m);
    // Save results for graph generation
    metrics_append_csv("metrics.csv", &m);
    // Free allocated memory
    global_stats_destroy(&g);
    for (int i = 0; i < P; i++)
        local_stats_destroy(locals[i]);
    free(locals);
    free(par_args);
    free(par_tids);
    free(dl_args);
    free(dl_tids)
    queue_destroy(url_queue);
    queue_destroy(chunk_queue);
    pthread_mutex_destroy(&dl_lock);
    pthread_mutex_destroy(&par_lock);
    curl_global_cleanup();
    return 0;
}