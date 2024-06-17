#include <stdio.h>

#ifndef PARAMS_STRUCT
#define PARAMS_STRUCT
typedef struct
{
    char *rlbData;
    int rlbSize;
    char *indexData;
    int idxSize;
    int checkpointCount;
    const int *cTable;
    const int *positions;
} Params;
#endif

#define ALPHABET_SIZE 98
#define QUICK_TABLE_LEN 8
#define PIECE_LENGTH (ALPHABET_SIZE * 4 + QUICK_TABLE_LEN * 12)
#define CHECKPOINT_LENGTH (PIECE_LENGTH + 4)
unsigned char map(char ch);
int *generateIndex(FILE *rlb, FILE *index, int checkpointCount);
int *generateCTable(FILE *rlb, FILE *index, int checkpointCount);
int nthChar(int nth, char ch, int const *cTable);
int occFunc(char ch, int pos);
char decode(int pos, int *rank, int *count, int *startPos);
int findIndex(int const arr[], int n, int key);
