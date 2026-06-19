#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Create stats structure for a parser thread 
LocalStats *local_stats_create(void)
{
    LocalStats *s = calloc(1, sizeof(LocalStats));
    // Initial space for storing palindromes 
    s->pal_capacity = 64;
    s->palindromes = malloc(sizeof(char *) * s->pal_capacity);
    // Hash table for unique words
    s->word_table = calloc(HASH_SIZE, sizeof(struct WordNode *));
    return s;
}
// Check if palindrome already exists
static int local_has_palindrome(LocalStats *s, const char *w)
{
    for (int i = 0; i < s->pal_count; i++)
        if (strcmp(s->palindromes[i], w) == 0)
            return 1;

    return 0;
}
// Add a new palindrome
static void local_add_palindrome(LocalStats *s, const char *w)
{
    if (local_has_palindrome(s, w))
        return;
    // Increase storage if needed
    if (s->pal_count == s->pal_capacity)
    {
        s->pal_capacity *= 2;
        s->palindromes = realloc(
            s->palindromes,
            sizeof(char *) * s->pal_capacity);
    }
    s->palindromes[s->pal_count++] = strdup(w);
}
// Insert word into local hash table
static int local_word_insert(LocalStats *s, const char *word)
{
    uint32_t idx = fnv1a(word) & (HASH_SIZE - 1);
    for (struct WordNode *n = s->word_table[idx]; n; n = n->next)
        if (strcmp(n->word, word) == 0)
            return 0;
    struct WordNode *n = malloc(sizeof(struct WordNode));
    n->word = strdup(word);
    n->next = s->word_table[idx];
    s->word_table[idx] = n;
    return 1;
}
// Process a chunk received from the downloader
void process_chunk(LocalStats *s, const char *text)
{
    char *buf = strdup(text);
    if (!buf)
        return;
    // Count sentence endings
    for (const char *p = text; *p; p++)
    {
        if (*p == '.' || *p == '?' || *p == '!')
        { const char *q = p + 1;
            while (*q == ' ' || *q == '\t')
                q++;
            if (*q != '.')
                s->sentence_count++;
        }
    }
    // Process words one by one
    char *saveptr = NULL;
    char *token = strtok_r(buf, " \t\r\n", &saveptr);
    while (token)
    {
        char clean[MAX_WORD_LEN];
        int ci = 0;
        const char *tp = token;
        // Skip leading non-letter characters
        while (*tp && !isalpha((unsigned char)*tp))
            tp++;
        // Keep only alphabetic characters
        while (*tp &&
               isalpha((unsigned char)*tp) &&
               ci < MAX_WORD_LEN - 1)
        {
            clean[ci++] = (char)tolower((unsigned char)*tp++);
        }

        clean[ci] = '\0';

        if (ci >= 1)
        {
            s->total_words++;
            // Track unique words 
            if (local_word_insert(s, clean))
               s->unique_word_count++;
            // Check for palindrome words
            if (ci >= 2 && is_palindrome(clean))
                local_add_palindrome(s, clean);
        }
      token = strtok_r(NULL, " \t\r\n", &saveptr);
    }

    free(buf);
}
// Main parser thread function
void *parser_thread(void *arg)
{
    ParserArgs *a = (ParserArgs *)arg;
    double t_start = now_sec();
    while (1)
    {
        ChunkItem *ci = (ChunkItem *)queue_pop(a->chunk_queue);
        if (!ci)
            break;
        process_chunk(a->local, ci->text);
        free(ci->text);
        free(ci);
    }
    double elapsed = now_sec() - t_start;
    pthread_mutex_lock(a->parse_time_lock);
    *a->parse_time_acc += elapsed;
    pthread_mutex_unlock(a->parse_time_lock);
    return NULL;
}

// Free memory used by LocalStats
void local_stats_destroy(LocalStats *s)
{
    if (!s)
        return;

    for (int i = 0; i < s->pal_count; i++)
        free(s->palindromes[i]);
    free(s->palindromes);
    for (int b = 0; b < HASH_SIZE; b++)
    {
        struct WordNode *n = s->word_table[b];
    while (n)
        {
            struct WordNode *t = n->next;
            free(n->word);
            free(n);
            n = t;
        }
    }
    free(s->word_table);
    free(s);
}
