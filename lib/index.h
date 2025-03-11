#include <stdio.h>

#ifndef PARAMS_STRUCT
#define PARAMS_STRUCT
typedef struct
{
    FILE *rlb;
    FILE *index;
    int checkpointCount;
    const int *cTable;
    const int *positions;
} Params;
#endif

#define ALPHABET_SIZE 98
#define PIECE_LENGTH (ALPHABET_SIZE * 4)
#define CHECKPOINT_LENGTH (PIECE_LENGTH + 4)
unsigned char map(char ch);
int *generateIndex(FILE *rlb, FILE *index, int checkpointCount);
int *generateCTable(FILE *rlb, FILE *index, int checkpointCount);
int nthChar(int nth, char ch, int const *cTable);
int occFunc(char ch, int pos,  Params const *params);
char decode(int pos, int *rank, int *count, int *startPos, Params const *params);
int findIndex(int const arr[], int n, int key);