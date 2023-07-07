#include <stdio.h>

#define ALPHABET_SIZE 98
#define QUICK_TABLE_LEN 16
#define PIECE_LENGTH (ALPHABET_SIZE * 4 + QUICK_TABLE_LEN * 12)
#define CHECKPOINT_LENGTH (PIECE_LENGTH + 4)
unsigned char map(char ch);
int *generateIndex(FILE *rlb, FILE *index, int checkpointCount);
int *generateCTable(FILE *rlb, FILE *index, int checkpointCount);
int nthChar(int nth, char ch, int const *cTable);
int occ(char ch, int pos, int const *positions, FILE *index, FILE *rlb, int checkpointCount);
char decode(int pos, int const *positions, FILE *index, FILE *rlb, int checkpointCount, int *rank, int *count,
            int *startPos);
int findIndex(int const arr[], int n, int key);
void summary();