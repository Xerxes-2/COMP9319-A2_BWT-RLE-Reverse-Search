#define CACHE_SIZE 180000
#define MAX_MATCHES 10000
#define MAX_PATTERN_LENGTH 512
#define MAX_RECORD_LENGTH 5000
#include <stdio.h>
#include "index.h"

char *reverse(char *str);
void search(char const *pattern, Params const *params);
void rebuildRec(char *record, int pos, Params const *params);
unsigned int findId(int pos, Params const *params);
char rebuildCached(int pos, int *rank, Params const *params);
void freeCache(int n);