#ifndef AGGREGATOR_H
#define AGGREGATOR_H
#include "parser.h"
typedef struct
{
    long total_sentence_count;
    char **palindromes;
    int pal_count;
    int pal_capacity;
    long unique_word_count;
    long total_words;
} 
GlobalStats;
void aggregate(GlobalStats *g, LocalStats **locals, int count);
void print_global_stats(const GlobalStats *g);
void global_stats_destroy(GlobalStats *g);

#endif
