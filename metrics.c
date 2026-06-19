
#include "metrics.h"
#include <stdio.h>
// Display performance results on screen 
void metrics_print(const RunMetrics *m)
{
    printf("\n====================================\n");
    printf("       PERFORMANCE METRICS\n");
    printf("====================================\n\n");
    printf("  Total execution time       : %.4f s\n", m->total_time);
    printf("  Download stage (avg/thread): %.4f s\n", m->dl_avg);
    printf("  Parse stage    (avg/thread): %.4f s\n", m->par_avg);
    printf("  Aggregation time           : %.4f s\n", m->agg_time);
    printf("  Total words processed      : %ld\n", m->total_words);
    printf("  Throughput                 : %.2f words/s\n", m->throughput);
    // Show thread configuration used in this run
    printf("  D = %d  P = %d  chunk_size = %d  T = %d\n\n",
           m->D, m->P, m->chunk_size, m->T);
}

// Save metrics to CSV file
void metrics_append_csv(const char *path, const RunMetrics *m)
{
    FILE *f = fopen(path, "a");
    if (!f)
        return;
    fseek(f, 0, SEEK_END);
    // Add column names if file is empty
    if (ftell(f) == 0)
        fprintf(f, "T,D,P,chunk_size,total_time,dl_avg,par_avg,"
                   "agg_time,total_words,throughput\n");
    // Write current run data
    fprintf(f, "%d,%d,%d,%d,%.4f,%.4f,%.4f,%.4f,%ld,%.2f\n",
            m->T, m->D, m->P, m->chunk_size,
            m->total_time, m->dl_avg, m->par_avg, m->agg_time,
            m->total_words, m->throughput);
    fclose(f);
}
