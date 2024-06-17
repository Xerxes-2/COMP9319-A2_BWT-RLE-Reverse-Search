#define CACHE_SIZE 150000
#define MAX_MATCHES 10000
#define MAX_PATTERN_LENGTH 512
#define MAX_RECORD_LENGTH 5000
#include <stdio.h>
#include "index.h"

char *reverse(char str[]);
void search(char const *pattern);
void rebuildRec(char *record, int pos);
struct FindIdParams
{
    int pos;
    unsigned int *id;
};
void *findId(void *_params);
char rebuildCached(int pos, int *rank);
void freeCache(int n);
