#define HASH_SIZE 200000
#define CACHE_SIZE 500000
#define MAX_MATCHES 10000
#define MAX_PATTERN_LENGTH 512
#define MAX_RECORD_LENGTH 5000
#include <stdio.h>

char *reverse(char *str);
void search(char const *pattern, int const *cTable, int const *position, FILE *rlb, FILE *index, int checkpointCount);
void rebuildRec(char *record, int pos, int const *cTable, int const *position, FILE *rlb, FILE *index,
                int checkpointCount);
unsigned int findId(int pos, int const *cTable, int const *position, FILE *rlb, FILE *index, int checkpointCount);
char rebuildCached(char ch, int *rank, int const *cTable, int const *position, FILE *rlb, FILE *index,
                   int checkpointCount);
void freeCache();