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

struct RunLength
{
    unsigned char ch;
    unsigned int count;
    unsigned int pos;
    unsigned int rank;
};

int compareRL(const void *a, const void *b)
{
    const struct RunLength *elemA = (const struct RunLength *)a;
    const struct RunLength *elemB = (const struct RunLength *)b;
    return elemA->pos - elemB->pos; // reverse order to create a min-heap
}

unsigned long encodeRL(struct RunLength const *rl)
{
    // count and pos is at most 28 bit
    return ((unsigned long)rl->ch << 56) | ((unsigned long)rl->count << 28) | rl->pos;
}

struct RunLength decodeRL(unsigned long rl)
{
    struct RunLength decoded;
    decoded.ch = rl >> 56;
    decoded.count = (rl >> 28) & 0xFFFFFFF;
    decoded.pos = rl & 0xFFFFFFF;
    return decoded;
}

struct PQueue
{
    struct RunLength data[QUICK_TABLE_LEN];
    int size;
};

void swap(struct RunLength *x, struct RunLength *y)
{
    struct RunLength temp = *x;
    *x = *y;
    *y = temp;
}

void insert(struct PQueue *pq, struct RunLength rl)
{
    if (pq->size < QUICK_TABLE_LEN)
    {
        pq->data[pq->size] = rl;
        pq->size++;
        int i = pq->size - 1;
        while (i > 0)
        {
            int parent = (i - 1) / 2;
            if (pq->data[i].count < pq->data[parent].count)
            {
                swap(&pq->data[i], &pq->data[parent]);
                i = parent;
            }
            else
            {
                break;
            }
        }
        return;
    }

    if (rl.count > pq->data[0].count)
    {
        pq->data[0] = rl;
        int i = 0;
        while (i < QUICK_TABLE_LEN)
        {
            int left = 2 * i + 1;
            int right = 2 * i + 2;
            int smallest = i;
            if (left < QUICK_TABLE_LEN && pq->data[left].count < pq->data[smallest].count)
            {
                smallest = left;
            }
            if (right < QUICK_TABLE_LEN && pq->data[right].count < pq->data[smallest].count)
            {
                smallest = right;
            }
            if (smallest != i)
            {
                swap(&pq->data[i], &pq->data[smallest]);
                i = smallest;
            }
            else
            {
                break;
            }
        }
    }
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
        struct PQueue pq = {.size = 0};
        for (i++; i < CHECKPOINT_LENGTH; i++)
        {
            if (MSB(buffer[i]))
            {
                rlCount = rebuildCount(rlCount, buffer[i], countLen);
                countLen++;
            }
            else
            {
                struct RunLength rl = {rlChar, rlCount, curPosBWT, occ[map(rlChar)]};
                insert(&pq, rl);
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
        qsort(pq.data, pq.size, sizeof(struct RunLength), compareRL);
        unsigned int quickTable[QUICK_TABLE_LEN * 3];
        for (int j = 0; j < QUICK_TABLE_LEN; j++)
        {
            unsigned long rl = encodeRL(&pq.data[j]);
            quickTable[j * 3] = rl >> 32;
            quickTable[j * 3 + 1] = (unsigned int)rl;
            quickTable[j * 3 + 2] = pq.data[j].rank;
        }
        fwrite(quickTable, sizeof(unsigned int), QUICK_TABLE_LEN * 3, index);
        fwrite(occ, sizeof(int), ALPHABET_SIZE, index);
        writeCpCount++;

        fseek(rlb, -4, SEEK_CUR); // go back for overhead before
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
        // fseek(params->index,
            //   (params->checkpointCount + QUICK_TABLE_LEN * 3) * sizeof(int) + (nearest - 1) * PIECE_LENGTH, SEEK_SET);
        // long readN = fread(&occ, sizeof(int), ALPHABET_SIZE, params->index);
        // if (readN != ALPHABET_SIZE)
        // {
        //     fprintf(stderr, "Failed to read occ from index file\n");
        //     exit(EXIT_FAILURE);
        // }
        long start = (params->checkpointCount + QUICK_TABLE_LEN * 3) * sizeof(int) + (nearest - 1) * PIECE_LENGTH;
        int readN = params->idxSize - start < ALPHABET_SIZE * sizeof(int) ? params->idxSize - start : ALPHABET_SIZE * sizeof(int);
        if (readN != ALPHABET_SIZE * sizeof(int))
        {
            fprintf(stderr, "Failed to read occ from index file\n");
            exit(EXIT_FAILURE);
        }
        memcpy(&occ, params->indexData + start, ALPHABET_SIZE * sizeof(int));
    }
    if (posBWT == pos)
    {
        return occ[map(ch)];
    }
    // fseek(params->rlb, posRLB, SEEK_SET);
    unsigned char buffer[CHECKPOINT_LENGTH + 4];
    // int bytesRead = fread(buffer, 1, CHECKPOINT_LENGTH + 4, params->rlb);
    int bytesRead = params->rlbSize - posRLB < CHECKPOINT_LENGTH + 4 ? params->rlbSize - posRLB : CHECKPOINT_LENGTH + 4;
    if (bytesRead <= 0)
    {
        fprintf(stderr, "Failed to read from rlb file\n");
        exit(EXIT_FAILURE);
    }
    memcpy(buffer, params->rlbData + posRLB, bytesRead);

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
    if (pos == posBWT)
    {
        return occ[map(ch)];
    }
    if (countLen)
    {
        return occ[map(ch)] + (pos - posBWT) * (rlChar == ch);
    }
    return occ[map(ch)] + (rlChar == ch);
}

struct checkpoint
{
    int occTable[ALPHABET_SIZE];
    unsigned int quickTable[QUICK_TABLE_LEN * 3];
};

int findRL(unsigned int const arr[], int key)
{
    int left = 0;
    int right = QUICK_TABLE_LEN - 1;

    while (left <= right)
    {
        int mid = left + (right - left) / 2;

        if ((arr[3 * mid + 1] & 0xFFFFFFF) <= key)
            left = mid + 1;
        else
            right = mid - 1;
    }

    return right; // return the last valid position
}

char readCP(struct checkpoint *cp, int nearest, Params const *params)
{
    long readPos;
    long readLength;
    void *cpPtr;
    if (nearest > 0 && nearest < params->checkpointCount)
    {
        readPos = (params->checkpointCount + QUICK_TABLE_LEN * 3) * sizeof(int) + (nearest - 1) * PIECE_LENGTH;
        readLength = ALPHABET_SIZE + QUICK_TABLE_LEN * 3;
        cpPtr = cp;
    }
    else if (nearest == params->checkpointCount && nearest)
    {
        readPos = (params->checkpointCount + QUICK_TABLE_LEN * 3) * sizeof(int) +
                  (params->checkpointCount - 1) * PIECE_LENGTH;
        readLength = ALPHABET_SIZE;
        cpPtr = &cp->occTable;
    }
    else if (params->checkpointCount)
    {
        readPos = params->checkpointCount * sizeof(int);
        readLength = QUICK_TABLE_LEN * 3;
        cpPtr = &cp->quickTable;
    }
    if (params->checkpointCount)
    {
        // fseek(params->index, readPos, SEEK_SET);
        // long readN = fread(cpPtr, sizeof(int), readLength, params->index);
        long readN = params->idxSize - readPos < readLength * sizeof(int) ? params->idxSize - readPos : readLength * sizeof(int);
        if (readN != readLength * sizeof(int))
        {
            fprintf(stderr, "Failed to read checkpoint from index file\n");
            exit(EXIT_FAILURE);
        }
        memcpy(cpPtr, params->indexData + readPos, readN);
    }
    return params->checkpointCount && nearest < params->checkpointCount;
}

char decode(int pos, int *rank, int *count, int *startPos, Params const *params)
{
    int nearest = findIndex(params->positions, params->checkpointCount, pos);

    int posBWT = params->positions[nearest];
    int posRLB = nearest * CHECKPOINT_LENGTH;

    struct checkpoint cp = {0};
    if (readCP(&cp, nearest, params))
    {
        int rlIndex = findRL(cp.quickTable, pos);
        if (rlIndex >= 0)
        {
            unsigned long rl = (((unsigned long)cp.quickTable[rlIndex * 3]) << 32) | cp.quickTable[rlIndex * 3 + 1];
            struct RunLength decoded = decodeRL(rl);
            if (decoded.pos <= pos && decoded.pos + decoded.count > pos)
            {
                *rank = cp.quickTable[rlIndex * 3 + 2] + (pos - decoded.pos);
                *count = decoded.count;
                *startPos = decoded.pos;
                return decoded.ch;
            }
        }
    }

    // fseek(params->rlb, posRLB, SEEK_SET);
    unsigned char buffer[CHECKPOINT_LENGTH + 4];
    // short bytesRead = (short)fread(buffer, 1, CHECKPOINT_LENGTH + 4, params->rlb);
    short bytesRead = params->rlbSize - posRLB < CHECKPOINT_LENGTH + 4 ? params->rlbSize - posRLB : CHECKPOINT_LENGTH + 4;
    if (bytesRead <= 0)
    {
        fprintf(stderr, "Failed to read from rlb file\n");
        exit(EXIT_FAILURE);
    }
    memcpy(buffer, params->rlbData + posRLB, bytesRead);

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
