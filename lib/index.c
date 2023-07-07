#include "index.h"
#include <search.h>
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

unsigned int rebuildCount(unsigned int curCount, unsigned char newByte, unsigned char acc)
{
    unsigned int newByteCount = newByte & 0x7F;
    return curCount + 2 * (acc == 0) + (newByteCount << (acc * 7));
}

struct runLength
{
    unsigned char ch;
    unsigned int count;
    unsigned int pos;
    unsigned int rank;
};

int compareRL(const void *a, const void *b)
{
    const struct runLength *elemA = (const struct runLength *)a;
    const struct runLength *elemB = (const struct runLength *)b;
    return elemA->pos - elemB->pos; // reverse order to create a min-heap
}

unsigned long encodeRL(struct runLength const *rl)
{
    // count and pos is at most 28 bit
    return ((unsigned long)rl->ch << 56) | ((unsigned long)rl->count << 28) | rl->pos;
}

struct runLength decodeRL(unsigned long rl)
{
    struct runLength decoded;
    decoded.ch = rl >> 56;
    decoded.count = (rl >> 28) & 0xFFFFFFF;
    decoded.pos = rl & 0xFFFFFFF;
    return decoded;
}

struct PQueue
{
    struct runLength data[QUICK_TABLE_LEN];
    int size;
};

void swap(struct runLength *x, struct runLength *y)
{
    struct runLength temp = *x;
    *x = *y;
    *y = temp;
}

void insert(struct PQueue *pq, struct runLength rl)
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

struct runLength top(const struct PQueue *pq)
{
    if (pq->size == 0)
    {
        // The queue is empty. You should handle this error in your code.
        struct runLength rl = {0, 0, 0, 0};
        return rl;
    }
    return pq->data[0];
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
    int *positions = (int *)malloc(checkpointCount * sizeof(int));
    if (positions == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for positions\n");
        exit(EXIT_FAILURE);
    }
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
        unsigned char acc = 0;
        struct PQueue pq = {.size = 0};
        for (i++; i < CHECKPOINT_LENGTH; i++)
        {
            if (MSB(buffer[i]))
            {
                rlCount = rebuildCount(rlCount, buffer[i], acc);
                acc++;
            }
            else
            {
                struct runLength rl = {rlChar, rlCount, curPosBWT, occ[map(rlChar)]};
                insert(&pq, rl);
                occ[map(rlChar)] += rlCount;
                curPosBWT += rlCount;
                rlChar = buffer[i];
                rlCount = 1;
                acc = 0;
            }
        }

        while (bytesRead > i && MSB(buffer[i]))
        { // if last byte is run length
            rlCount = rebuildCount(rlCount, buffer[i], acc);
            acc++;
            i++;
        }

        occ[map(rlChar)] += rlCount;
        curPosBWT += rlCount;

        positions[writeCpCount] = curPosBWT;
        qsort(pq.data, pq.size, sizeof(struct runLength), compareRL);
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
    fwrite(positions, sizeof(int), checkpointCount, index);

    int *newPositions = malloc((checkpointCount + 1) * sizeof(int));
    if (newPositions == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for newPositions\n");
        exit(EXIT_FAILURE);
    }
    newPositions[0] = 0;
    memcpy(newPositions + 1, positions, checkpointCount * sizeof(int));
    free(positions);

    return newPositions;
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
        unsigned char acc = 0;

        for (i++; i < bytesRead; i++)
        {
            if (MSB(buffer[i]))
            {

                rlCount = rebuildCount(rlCount, buffer[i], acc);
                acc++;
            }
            else
            {
                cTable[map(rlChar) + 1] += rlCount;
                rlChar = buffer[i];
                rlCount = 1;
                acc = 0;
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

static unsigned long callOcc = 0;
static unsigned long callDecode = 0;

int occ(char ch, int pos, int const *positions, FILE *index, FILE *rlb, int checkpointCount)
{
    callOcc++;
    int nearest = findIndex(positions, checkpointCount, pos);

    int posBWT = positions[nearest];
    int posRLB = nearest * CHECKPOINT_LENGTH;
    int occ = 0;
    if (checkpointCount && nearest)
    {
        fseek(index, (checkpointCount + map(ch) + QUICK_TABLE_LEN * 3) * sizeof(int) + (nearest - 1) * PIECE_LENGTH,
              SEEK_SET);
        unsigned long readN = fread(&occ, sizeof(int), 1, index);
        if (readN != 1)
        {
            fprintf(stderr, "Failed to read occ from index file\n");
            exit(EXIT_FAILURE);
        }
    }
    if (posBWT == pos)
    {
        return occ;
    }
    fseek(rlb, posRLB, SEEK_SET);
    unsigned char buffer[CHECKPOINT_LENGTH + 4];
    short bytesRead = (short)fread(buffer, 1, CHECKPOINT_LENGTH + 4, rlb);
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
    unsigned char acc = 0;

    for (i++; i < bytesRead; i++)
    {
        if (MSB(buffer[i]))
        {
            rlCount = rebuildCount(rlCount, buffer[i], acc);
            acc++;
        }
        else
        {
            if (posBWT + rlCount > pos)
            {
                return occ + (pos - posBWT) * (rlChar == ch);
            }
            posBWT += rlCount;
            occ += rlCount * (rlChar == ch);
            rlChar = buffer[i];
            rlCount = 1;
            acc = 0;
        }
    }
    if (pos == posBWT)
    {
        return occ;
    }
    if (acc)
    {
        return occ + (pos - posBWT) * (rlChar == ch);
    }
    else
    {
        return occ + (rlChar == ch);
    }
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

char decode(int pos, int const *positions, FILE *index, FILE *rlb, int checkpointCount, int *rank, int *count,
            int *startPos)
{
    callDecode++;
    int nearest = findIndex(positions, checkpointCount, pos);

    int posBWT = positions[nearest];
    int posRLB = nearest * CHECKPOINT_LENGTH;

    struct checkpoint cp = {0};
    if (nearest > 0 && nearest < checkpointCount)
    {
        fseek(index, (checkpointCount + QUICK_TABLE_LEN * 3) * sizeof(int) + (nearest - 1) * PIECE_LENGTH, SEEK_SET);
        unsigned long readN = fread(&cp, sizeof(int), ALPHABET_SIZE + QUICK_TABLE_LEN * 3, index);
        if (readN != ALPHABET_SIZE + QUICK_TABLE_LEN * 3)
        {
            fprintf(stderr, "Failed to read occ from index file\n");
            exit(EXIT_FAILURE);
        }
        int rlIndex = findRL(cp.quickTable, pos);
        if (rlIndex >= 0)
        {
            unsigned long rl = (((unsigned long)cp.quickTable[rlIndex * 3]) << 32) | cp.quickTable[rlIndex * 3 + 1];
            struct runLength decoded = decodeRL(rl);
            if (decoded.pos <= pos && decoded.pos + decoded.count > pos)
            {
                *rank = cp.quickTable[rlIndex * 3 + 2] + (pos - decoded.pos);
                *count = decoded.count;
                *startPos = decoded.pos;
                return decoded.ch;
            }
        }
    }
    else if (nearest == checkpointCount && checkpointCount)
    {
        fseek(index, (checkpointCount + QUICK_TABLE_LEN * 3) * sizeof(int) + (checkpointCount - 1) * PIECE_LENGTH,
              SEEK_SET);
        unsigned long readN = fread(&cp.occTable, sizeof(int), ALPHABET_SIZE, index);
        if (readN != ALPHABET_SIZE)
        {
            fprintf(stderr, "Failed to read occ from index file\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (checkpointCount)
    {
        fseek(index, checkpointCount * sizeof(int), SEEK_SET);
        unsigned int quickTable[QUICK_TABLE_LEN * 3];
        unsigned long readN = fread(quickTable, sizeof(int), QUICK_TABLE_LEN * 3, index);
        if (readN != QUICK_TABLE_LEN * 3)
        {
            fprintf(stderr, "Failed to read occ from index file\n");
            exit(EXIT_FAILURE);
        }
        int rlIndex = findRL(cp.quickTable, pos);
        if (rlIndex >= 0)
        {
            unsigned long rl = (((unsigned long)cp.quickTable[rlIndex * 3]) << 32) | cp.quickTable[rlIndex * 3 + 1];
            struct runLength decoded = decodeRL(rl);
            if (decoded.pos <= pos && decoded.pos + decoded.count > pos)
            {
                *rank = cp.quickTable[rlIndex * 3 + 2] + (pos - decoded.pos);
                *count = decoded.count;
                *startPos = decoded.pos;
                return decoded.ch;
            }
        }
    }

    fseek(rlb, posRLB, SEEK_SET);

    unsigned char buffer[CHECKPOINT_LENGTH + 4];
    short bytesRead = (short)fread(buffer, 1, CHECKPOINT_LENGTH + 4, rlb);
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
    unsigned char acc = 0;

    for (i++; i < bytesRead; i++)
    {
        if (MSB(buffer[i]))
        {

            rlCount = rebuildCount(rlCount, buffer[i], acc);
            acc++;
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
            acc = 0;
        }
    }

    if (acc > 0)
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

void summary()
{
    printf("Total call of occ: %lu\n", callOcc);
    printf("Total call of decode: %lu\n", callDecode);
}