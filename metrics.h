#ifndef METRICS_H
#define METRICS_H

/* All performance data for one run */
typedef struct {
    int    T, D, P, chunk_size;
    double total_time;
    double dl_avg;
    double par_avg;
    double agg_time;
    long   total_words;
    double throughput;
} RunMetrics;

void metrics_print(const RunMetrics *m);
void metrics_append_csv(const char *path, const RunMetrics *m);

#endif
