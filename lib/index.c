#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSB(x) ((x) >> 7)

unsigned char map(char ch)
{
    switch (ch)
    {
    case 9:
    case 10:
        return ch - 9;
    case 13:
        return 2;
    default:
        return ch - 29;
    }
}

unsigned int rebuildCount(unsigned int curCount, unsigned char newByte, unsigned char countLen)
{
    unsigned int newByteCount = newByte & 0x7F;
    return curCount + 2 * (countLen == 0) + (newByteCount << (countLen * 7));
}

int *generateIndex(FILE *rlb, FILE *index, int checkpointCount)
{
    if (checkpointCount == 0)
    {
        int *positions = (int *)malloc(sizeof(int));
        if (positions == NULL)
        {
            fprintf(stderr, "Failed to allocate memory for positions\n");
            exit(EXIT_FAILURE);
        }
        positions[0] = 0;
        return positions;
    }
    // reserve rlbSize * 4 bytes for positions in BWT
    fseek(index, checkpointCount * sizeof(int), SEEK_SET);
    int *positions = (int *)malloc((checkpointCount + 1) * sizeof(int));
    if (positions == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for positions\n");
        exit(EXIT_FAILURE);
    }
    positions[0] = 0;
    int *writePositions = positions + 1;
    int occ[ALPHABET_SIZE] = {0};
    int writeCpCount = 0;
    int curPosBWT = 0;
    unsigned char buffer[CHECKPOINT_LENGTH + 4]; // In case of 4 bytes run length
    int bytesRead = (int)fread(buffer, 1, CHECKPOINT_LENGTH + 4, rlb);
    while (bytesRead >= CHECKPOINT_LENGTH)
    {
        short i = 0;
        while (MSB(buffer[i]))
        {
            i++;
        }

        unsigned char rlChar = buffer[i];
        unsigned int rlCount = 1;
        unsigned char countLen = 0;
        for (i++; i < CHECKPOINT_LENGTH; i++)
        {
            if (MSB(buffer[i]))
            {
                rlCount = rebuildCount(rlCount, buffer[i], countLen);
                countLen++;
            }
            else
            {
                occ[map(rlChar)] += rlCount;
                curPosBWT += rlCount;
                rlChar = buffer[i];
                rlCount = 1;
                countLen = 0;
            }
        }

        while (bytesRead > i && MSB(buffer[i]))
        { // if last byte is run length
            rlCount = rebuildCount(rlCount, buffer[i], countLen);
            countLen++;
            i++;
        }

        occ[map(rlChar)] += rlCount;
        curPosBWT += rlCount;

        writePositions[writeCpCount] = curPosBWT;
        fwrite(occ, sizeof(int), ALPHABET_SIZE, index);
        writeCpCount++;

        if (bytesRead > CHECKPOINT_LENGTH)
        {
            fseek(rlb, CHECKPOINT_LENGTH - bytesRead, SEEK_CUR); // go back for overhead before
        }
        bytesRead = (int)fread(buffer, 1, CHECKPOINT_LENGTH + 4, rlb);
    }

    fseek(index, 0, SEEK_SET);
    fwrite(writePositions, sizeof(int), checkpointCount, index);

    return positions;
}

int *generateCTable(FILE *rlb, FILE *index, int checkpointCount)
{
    int lastPos = checkpointCount * CHECKPOINT_LENGTH;
    int *cTable = calloc(ALPHABET_SIZE + 1, sizeof(int));
    if (cTable == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for cTable\n");
        exit(EXIT_FAILURE);
    }
    if (checkpointCount > 0)
    {
        fseek(index, -ALPHABET_SIZE * 4, SEEK_END);
        int readN = (int)fread(cTable + 1, sizeof(int), ALPHABET_SIZE, index);
        if (readN != ALPHABET_SIZE)
        {
            fprintf(stderr, "Failed to read cTable from index file\n");
            exit(EXIT_FAILURE);
        }
    }
    fseek(rlb, lastPos, SEEK_SET);

    unsigned char buffer[CHECKPOINT_LENGTH];
    short bytesRead = (short)fread(buffer, 1, CHECKPOINT_LENGTH, rlb);
    if (bytesRead > 0)
    {
        short i = 0;
        while (bytesRead > i && MSB(buffer[i]))
        { // if first byte is run length
            i++;
        }

        char rlChar = buffer[i];
        unsigned int rlCount = 1;
        unsigned char countLen = 0;

        for (i++; i < bytesRead; i++)
        {
            if (MSB(buffer[i]))
            {

                rlCount = rebuildCount(rlCount, buffer[i], countLen);
                countLen++;
            }
            else
            {
                cTable[map(rlChar) + 1] += rlCount;
                rlChar = buffer[i];
                rlCount = 1;
                countLen = 0;
            }
        }

        cTable[map(rlChar) + 1] += rlCount;
    }
    for (int i = 1; i <= ALPHABET_SIZE; i++)
    {
        cTable[i] += cTable[i - 1];
    }
    return cTable;
}

int nthChar(int nth, char ch, int const *cTable)
{
    return cTable[map(ch)] + nth;
}

int findIndex(int const arr[], int n, int key)
{
    int left = 0;
    int right = n;

    while (left <= right)
    {
        int mid = left + (right - left) / 2;

        if (arr[mid] <= key)
            left = mid + 1;
        else
            right = mid - 1;
    }

    return right; // return the last valid position
}

int occFunc(char ch, int pos, Params const *params)
{
    int nearest = findIndex(params->positions, params->checkpointCount, pos);
    int posBWT = params->positions[nearest];
    int posRLB = nearest * CHECKPOINT_LENGTH;
    int occ[ALPHABET_SIZE] = {0};
    if (nearest)
    {
        fseek(params->index,
              params->checkpointCount * sizeof(int) + (nearest - 1) * PIECE_LENGTH, SEEK_SET);
        unsigned long readN = fread(&occ, sizeof(int), ALPHABET_SIZE, params->index);
        if (readN != ALPHABET_SIZE)
        {
            fprintf(stderr, "Failed to read occ from index file\n");
            exit(EXIT_FAILURE);
        }
    }
    if (posBWT == pos)
    {
        return occ[map(ch)];
    }
    fseek(params->rlb, posRLB, SEEK_SET);
    unsigned char buffer[CHECKPOINT_LENGTH + 4];
    short bytesRead = (short)fread(buffer, 1, CHECKPOINT_LENGTH + 4, params->rlb);
    if (bytesRead <= 0)
    {
        fprintf(stderr, "Failed to read from rlb file\n");
        exit(EXIT_FAILURE);
    }

    short i = 0;
    while (bytesRead > i && MSB(buffer[i]))
    { // if first byte is run length
        i++;
    }

    char rlChar = buffer[i];
    unsigned int rlCount = 1;
    unsigned char countLen = 0;

    for (i++; i < bytesRead; i++)
    {
        if (MSB(buffer[i]))
        {
            rlCount = rebuildCount(rlCount, buffer[i], countLen);
            countLen++;
        }
        else
        {
            if (posBWT + rlCount > pos)
            {
                return occ[map(ch)] + (pos - posBWT) * (rlChar == ch);
            }
            posBWT += rlCount;
            occ[map(rlChar)] += rlCount;
            rlChar = buffer[i];
            rlCount = 1;
            countLen = 0;
        }
    }

    return occ[map(ch)] + (pos - posBWT) * (rlChar == ch);
}

struct checkpoint
{
    int occTable[ALPHABET_SIZE];
};

void readCP(struct checkpoint *cp, int nearest, Params const *params)
{
    long readPos;
    long readLength;
    void *cpPtr;
    if (params->checkpointCount && nearest)
    {
        readPos = params->checkpointCount * sizeof(int) +
                  (nearest - 1) * PIECE_LENGTH;
        readLength = ALPHABET_SIZE;
        cpPtr = &cp->occTable;
        fseek(params->index, readPos, SEEK_SET);
        unsigned long readN = fread(cpPtr, sizeof(int), readLength, params->index);
        if (readN != readLength)
        {
            fprintf(stderr, "Failed to read checkpoint from index file\n");
            exit(EXIT_FAILURE);
        }
    }
}

char decode(int pos, int *rank, int *count, int *startPos, Params const *params)
{
    int nearest = findIndex(params->positions, params->checkpointCount, pos);

    int posBWT = params->positions[nearest];
    int posRLB = nearest * CHECKPOINT_LENGTH;

    struct checkpoint cp = {0};
    readCP(&cp, nearest, params);

    fseek(params->rlb, posRLB, SEEK_SET);
    int buffer_len = pos - posBWT + 9;
    if (buffer_len > CHECKPOINT_LENGTH + 4)
    {
        buffer_len = CHECKPOINT_LENGTH + 4;
    }
    unsigned char buffer[CHECKPOINT_LENGTH + 4];
    short bytesRead = (short)fread(buffer, 1, buffer_len, params->rlb);
    if (bytesRead <= 0)
    {
        fprintf(stderr, "Failed to read from rlb file\n");
        exit(EXIT_FAILURE);
    }

    short i = 0;
    while (bytesRead > i && MSB(buffer[i]))
    { // if first byte is run length
        i++;
    }

    char rlChar = buffer[i];
    unsigned int rlCount = 1;
    unsigned char countLen = 0;

    for (i++; i < bytesRead; i++)
    {
        if (MSB(buffer[i]))
        {

            rlCount = rebuildCount(rlCount, buffer[i], countLen);
            countLen++;
        }
        else
        {
            if (posBWT + rlCount > pos)
            {
                *rank = cp.occTable[map(rlChar)] + (pos - posBWT);
                *count = rlCount;
                *startPos = posBWT;
                return rlChar;
            }
            posBWT += rlCount;
            cp.occTable[map(rlChar)] += rlCount;
            rlChar = buffer[i];
            rlCount = 1;
            countLen = 0;
        }
    }

    if (countLen > 0)
    {
        *rank = cp.occTable[map(rlChar)] + (pos - posBWT);
    }
    else
    {
        *rank = cp.occTable[map(rlChar)];
    }

    *count = rlCount;
    *startPos = posBWT;
    return rlChar;
}