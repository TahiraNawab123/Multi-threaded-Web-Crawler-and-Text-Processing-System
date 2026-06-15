#ifndef PARSER_H
#define PARSER_H

#include "utils.h"
#include "queue.h"
#include <pthread.h>

#define HASH_SIZE      (1 << 20)   /* 1 M hash buckets   */
#define MAX_PALINDROMES 4096

/* Linked-list node for word hash table */
struct WordNode {
    char            *word;
    struct WordNode *next;
};

/* Per-parser-thread statistics */
typedef struct {
    long   sentence_count;

    char **palindromes;
    int    pal_count;
    int    pal_capacity;

    struct WordNode **word_table;   /* HASH_SIZE buckets  */
    long   unique_word_count;
    long   total_words;
} LocalStats;

/* Thread argument passed to each parser thread */
typedef struct {
    BoundedQueue    *chunk_queue;
    LocalStats      *local;
    double          *parse_time_acc;
    pthread_mutex_t *parse_time_lock;
} ParserArgs;

LocalStats *local_stats_create(void);
void        local_stats_destroy(LocalStats *s);
void        process_chunk(LocalStats *s, const char *text);
void       *parser_thread(void *arg);

#endif
