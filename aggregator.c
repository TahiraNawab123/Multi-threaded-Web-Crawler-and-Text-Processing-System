#include "aggregator.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Global dedup word table (used only during aggregation) ─ */
static struct WordNode **g_word_table = NULL;
static long              g_unique_count = 0;

static void g_table_init(void)
{
    g_word_table   = calloc(HASH_SIZE, sizeof(struct WordNode *));
    g_unique_count = 0;
}

static void g_table_insert(const char *word)
{
    uint32_t idx = fnv1a(word) & (HASH_SIZE - 1);
    for (struct WordNode *n = g_word_table[idx]; n; n = n->next)
        if (strcmp(n->word, word) == 0) return;
    struct WordNode *n = malloc(sizeof(struct WordNode));
    n->word = strdup(word);
    n->next = g_word_table[idx];
    g_word_table[idx] = n;
    g_unique_count++;
}

static void g_table_free(void)
{
    if (!g_word_table) return;
    for (int i = 0; i < HASH_SIZE; i++) {
        struct WordNode *n = g_word_table[i];
        while (n) {
            struct WordNode *t = n->next;
            free(n->word); free(n); n = t;
        }
    }
    free(g_word_table);
    g_word_table = NULL;
}

/* ── Merge all LocalStats into GlobalStats ──────────────── */
void aggregate(GlobalStats *g, LocalStats **locals, int count)
{
    memset(g, 0, sizeof(GlobalStats));
    g->pal_capacity = 256;
    g->palindromes  = malloc(sizeof(char *) * g->pal_capacity);

    /* 1. Sentence count and total words (simple sum) */
    for (int i = 0; i < count; i++) {
        g->total_sentence_count += locals[i]->sentence_count;
        g->total_words          += locals[i]->total_words;
    }

    /* 2. Palindromes: deduplicated union */
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < locals[i]->pal_count; j++) {
            const char *w = locals[i]->palindromes[j];
            int found = 0;
            for (int k = 0; k < g->pal_count; k++)
                if (strcmp(g->palindromes[k], w) == 0) { found = 1; break; }
            if (!found) {
                if (g->pal_count == g->pal_capacity) {
                    g->pal_capacity *= 2;
                    g->palindromes = realloc(g->palindromes,
                                              sizeof(char *) * g->pal_capacity);
                }
                g->palindromes[g->pal_count++] = strdup(w);
            }
        }
    }

    /* 3. Unique words: merge all per-thread hash tables into one */
    g_table_init();
    for (int i = 0; i < count; i++)
        for (int b = 0; b < HASH_SIZE; b++)
            for (struct WordNode *n = locals[i]->word_table[b]; n; n = n->next)
                g_table_insert(n->word);
    g->unique_word_count = g_unique_count;
    g_table_free();
}

/* ── Print the final statistics block ───────────────────── */
void print_global_stats(const GlobalStats *g)
{
    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf(  "║          FINAL STATISTICS  (Group C)                ║\n");
    printf(  "╚══════════════════════════════════════════════════════╝\n\n");

    printf("1. Sentence Count : %ld\n\n", g->total_sentence_count);
    printf("2. Unique Word Count : %ld\n\n", g->unique_word_count);
    printf("3. Palindromic Words Found (%d unique):\n", g->pal_count);

    /* Insertion-sort palindromes for reproducible output */
    for (int i = 1; i < g->pal_count; i++) {
        char *key = g->palindromes[i];
        int   j   = i - 1;
        while (j >= 0 && strcmp(g->palindromes[j], key) > 0) {
            g->palindromes[j + 1] = g->palindromes[j]; j--;
        }
        g->palindromes[j + 1] = key;
    }
    for (int i = 0; i < g->pal_count; i++) {
        printf("   %-20s", g->palindromes[i]);
        if ((i + 1) % 5 == 0) printf("\n");
    }
    if (g->pal_count % 5 != 0) printf("\n");
    printf("\n");
}

void global_stats_destroy(GlobalStats *g)
{
    if (!g) return;
    for (int i = 0; i < g->pal_count; i++) free(g->palindromes[i]);
    free(g->palindromes);
}
