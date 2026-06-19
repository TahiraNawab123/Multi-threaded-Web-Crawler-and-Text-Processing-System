#include "utils.h"
#include <time.h>
#include <ctype.h>
#include <string.h>

// Returns current time in seconds 
double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
// Check if a word is a palindrome
int is_palindrome(const char *word)
{
    char buf[MAX_WORD_LEN];
    int len = 0;
    // Keep only letters and convert to lowercase
    for (const char *p = word; *p && len < MAX_WORD_LEN - 1; p++)
        if (isalpha((unsigned char)*p))
            buf[len++] = (char)tolower((unsigned char)*p);
    buf[len] = '\0';
    if (len < 2)
        return 0;
    int lo = 0, hi = len - 1;
    while (lo < hi)
    {
        if (buf[lo] != buf[hi])
            return 0;
        lo++;
        hi--;
    }
    return 1;
}
// Hash function used for word lookup
uint32_t fnv1a(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++)
    {
        h ^= (unsigned char)*s;
        h *= 16777619u;
    }
    return h;
}