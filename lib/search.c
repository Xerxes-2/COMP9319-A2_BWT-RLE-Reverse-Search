#include "search.h"
#include "index.h"
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *reverse(char *str)
{
    char *start = str;
    char *end = str + strlen(str) - 1;
    char temp;
    while (start < end)
    {
        temp = *start;
        *start = *end;
        *end = temp;
        start++;
        end--;
    }
    return str;
}

int compareInt(const void *a, const void *b)
{
    return (*(const int *)a - *(const int *)b);
}

int searchPattern(const char *pattern, const int *cTable, const int *position, FILE *rlb, FILE *index,
                  int checkpointCount, int *end)
{
    int indexStart = cTable[map(pattern[0])];
    int indexEnd = cTable[map(pattern[0]) + 1];
    int occStart;
    int occEnd;
    for (int i = 1; i < strlen(pattern); i++)
    {
        occStart = occ(pattern[i], indexStart, position, index, rlb, checkpointCount);
        occEnd = occ(pattern[i], indexEnd, position, index, rlb, checkpointCount);
        indexStart = nthChar(occStart, pattern[i], cTable);
        indexEnd = nthChar(occEnd, pattern[i], cTable);
    }
    if (end != NULL)
    {
        *end = indexEnd;
    }
    return indexStart;
}

static struct cacheItem **cache;

void search(char const *pattern, int const *cTable, int const *position, FILE *rlb, FILE *index, int checkpointCount)
{
    int indexEnd;
    int indexStart = searchPattern(pattern, cTable, position, rlb, index, checkpointCount, &indexEnd);
    unsigned int *idArr = (int *)malloc((indexEnd - indexStart) * sizeof(int));
    int j = 0;
    cache = calloc(checkpointCount + 1, sizeof(struct cacheItem **));
    for (int i = indexStart; i < indexEnd; i++)
    {
        // To decode entire record, we need to find the next record, so plus 1
        idArr[j++] = findId(i, cTable, position, rlb, index, checkpointCount) + 1;
    }
    qsort(idArr, j, sizeof(int), compareInt);
    char *newPattern = (char *)malloc(15);
    char *record = malloc(MAX_RECORD_LENGTH * sizeof(char));
    int matches = 0;
    for (int i = 0; i < j; i++)
    {
        if (i > 0 && idArr[i] == idArr[i - 1])
        {
            // skip duplicate
            continue;
        }
        matches++;
        newPattern[0] = '[';
        sprintf(newPattern + 1, "%d]", idArr[i]);
        reverse(newPattern);
        indexStart = searchPattern(newPattern, cTable, position, rlb, index, checkpointCount, &indexEnd);
        if (indexStart == indexEnd)
        {
            // must be end id + 1, so instead search for start id because all ids are consecutive
            int numRec = cTable[map('[') + 1] - cTable[map('[')];
            unsigned int startId = idArr[i] - numRec;
            newPattern[0] = '[';
            sprintf(newPattern + 1, "%d]", startId);
            reverse(newPattern);
            indexStart = searchPattern(newPattern, cTable, position, rlb, index, checkpointCount, &indexEnd);
        }
        sprintf(record, "[%d]", idArr[i] - 1);
        rebuildRec(record + strlen(record), indexStart, cTable, position, rlb, index, checkpointCount);
        printf("%s\n", record);
    }

    freeCache(checkpointCount + 1);
    free(record);
    free(newPattern);
    free(idArr);
    summary();
}

unsigned int findId(int pos, int const *cTable, int const *position, FILE *rlb, FILE *index, int checkpointCount)
{
    char *id = (char *)malloc(16);
    short i = 0;
    int rank;
    int isId = 0;
    int count;
    int startPos;
    char ch = decode(pos, position, index, rlb, checkpointCount, &rank, &count, &startPos);
    while (ch != '[' && i < MAX_RECORD_LENGTH)
    {
        if (ch == ']')
        {
            isId = 1;
        }
        ch = rebuildCached(ch, &rank, cTable, position, rlb, index, checkpointCount);
        if (isId)
        {
            id[i++] = ch;
        }
    }
    if (i > 0)
    {
        id[i - 1] = '\0';
    }
    reverse(id);
    unsigned int idInt = atoi(id);
    free(id);
    return idInt;
}

// Each cache item now also has a pointer to the next item in the list.
struct cacheItem
{
    char ch;
    int rank;
    int pos;
    int count;
    struct cacheItem *next;
};

static int cacheHits = 0;
static int cacheMisses = 0;

char rebuildCached(char ch, int *rank, int const *cTable, int const *position, FILE *rlb, FILE *index,
                   int checkpointCount)
{
    static int cacheSize = 0;

    int pos = nthChar(*rank, ch, cTable);
    int cp = findIndex(position, checkpointCount, pos);
    int count;
    int startPos;

    struct cacheItem *item = cache[cp];
    while (item != NULL)
    {
        if (item->pos <= pos && item->pos + item->count > pos)
        {
            cacheHits++;
            // Found a match in the cache. Use the cached result.
            *rank = item->rank + pos - item->pos;
            return item->ch;
        }
        item = item->next;
    }
    cacheMisses++;

    // No matching cache item found. Compute the result.
    char newCh = decode(pos, position, index, rlb, checkpointCount, rank, &count, &startPos);

    if (cacheSize < CACHE_SIZE)
    {
        // Allocate a new cache item.
        struct cacheItem *newItem = malloc(sizeof(struct cacheItem));
        newItem->ch = newCh;
        newItem->rank = *rank - pos + startPos;
        newItem->pos = startPos;
        newItem->count = count;

        newItem->next = cache[cp];
        cache[cp] = newItem;
        cacheSize++;
    }

    return newCh;
}

void freeCache(int n)
{
    int total = 0;
    for (int i = 0; i < n; i++)
    {
        struct cacheItem *item = cache[i];
        while (item != NULL)
        {
            total++;
            struct cacheItem *next = item->next;
            free(item);
            item = next;
        }
    }
    free(cache);
    printf("Cache hits: %d\n", cacheHits);
    printf("Cache misses: %d\n", cacheMisses);
    printf("Total cache items: %d\n", total);
}

void rebuildRec(char *record, int pos, int const *cTable, int const *position, FILE *rlb, FILE *index,
                int checkpointCount)
{
    short i = 0;
    int rank;
    int count;
    int startPos;
    char ch = decode(pos, position, index, rlb, checkpointCount, &rank, &count, &startPos);
    while (ch != ']' && i < MAX_RECORD_LENGTH)
    {
        record[i++] = ch;
        ch = rebuildCached(ch, &rank, cTable, position, rlb, index, checkpointCount);
    }
    record[i] = '\0';
    reverse(record);
}