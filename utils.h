#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>
#define MAX_WORD_LEN 128

double now_sec(void);
int is_palindrome(const char *word);
uint32_t fnv1a(const char *s);

#endif
