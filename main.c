#include "lib/index.h"
#include "lib/search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
    FILE *rlb;
    FILE *index = NULL;
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s <rlb file> <index file> <pattern>\n", argv[0]);
        return 1;
    }

    // open rlb file
    rlb = fopen(argv[1], "rb");
    if (rlb == NULL)
    {
        fprintf(stderr, "Failed to open rlb file\n");
        return 1;
    }
    struct stat st;
    stat(argv[1], &st);
    int rlbSize = (int)st.st_size;
    int checkpointCount = rlbSize / CHECKPOINT_LENGTH;

    // open index file
    int *positions;
    if (checkpointCount > 0)
    {
        index = fopen(argv[2], "rb");
        if (index == NULL)
        {
            // create index file
            index = fopen(argv[2], "wb");
            if (index == NULL)
            {
                fprintf(stderr, "Failed to create index file\n");
                fclose(rlb);
                return 1;
            }
            positions = generateIndex(rlb, index, checkpointCount);

            // reopen index file
            fclose(index);
            index = fopen(argv[2], "rb");
            if (index == NULL)
            {
                fprintf(stderr, "Failed to reopen index file\n");
                fclose(rlb);
                return 1;
            }
            fseek(index, checkpointCount * 4, SEEK_SET);
        }
        else
        {
            // read positions from index file
            positions = (int *)malloc((checkpointCount + 1) * sizeof(int));
            if (positions == NULL)
            {
                fprintf(stderr, "Failed to allocate memory for positions\n");
                exit(EXIT_FAILURE);
            }
            positions[0] = 0;
            int readN = (int)fread(positions + 1, sizeof(int), checkpointCount, index);
            if (readN != checkpointCount)
            {
                fprintf(stderr, "Failed to read positions from index file\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    else
    {
        positions = (int *)malloc(sizeof(int));
        if (positions == NULL)
        {
            fprintf(stderr, "Failed to allocate memory for positions\n");
            exit(EXIT_FAILURE);
        }
        positions[0] = 0;
    }

    char *pattern = argv[3];
    reverse(pattern);

    int *cTable = generateCTable(rlb, index, checkpointCount);

    search(pattern, cTable, positions, rlb, index, checkpointCount);

    free(positions);
    free(cTable);
    fclose(rlb);
    if (checkpointCount > 0 && index)
    {
        fclose(index);
    }

    return 0;
}