#include "search.h"
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

struct cacheItem
{
    char ch;
    int rank;
    int pos;
    int count;
    struct cacheItem *next;
};

static struct cacheItem **cache;

static int cacheHits = 0;
static int cacheMisses = 0;
static int maxCacheSize = 0;
static int cacheSize = 0;

int searchPattern(const char *pattern, int *end, Params const *params)
{
    int indexStart = params->cTable[map(pattern[0])];
    int indexEnd = params->cTable[map(pattern[0]) + 1];
    int occStart;
    int occEnd;
    for (int i = 1; i < strlen(pattern); i++)
    {
        occEnd = doubleOccFunc(pattern[i], indexStart, indexEnd, &occStart, params);
        indexStart = nthChar(occStart, pattern[i], params->cTable);
        indexEnd = nthChar(occEnd, pattern[i], params->cTable);
    }
    *end = indexEnd;
    return indexStart;
}

void search(char const *pattern, Params const *params)
{
    int indexEnd;
    cache = calloc(params->checkpointCount + 1, sizeof(struct cacheItem **));
    int indexStart = searchPattern(pattern, &indexEnd, params);
    unsigned int *idArr = (unsigned int *)malloc((indexEnd - indexStart) * sizeof(unsigned int));
    int j = 0;
    for (int i = indexStart; i < indexEnd; i++)
    {
        // To decode entire record, we need to find the next record, so plus 1
        idArr[j++] = findId(i, params) + 1;
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
        indexStart = searchPattern(newPattern, &indexEnd, params);
        if (indexStart == indexEnd)
        {
            // must be end_id + 1, so instead search for start id because all ids are consecutive
            int numRec = params->cTable[map('[') + 1] - params->cTable[map('[')];
            unsigned int startId = idArr[i] - numRec;
            newPattern[0] = '[';
            sprintf(newPattern + 1, "%d]", startId);
            reverse(newPattern);
            indexStart = searchPattern(newPattern, &indexEnd, params);
        }
        sprintf(record, "[%d]", idArr[i] - 1);
        rebuildRec(record + strlen(record), indexStart, params);
        printf("%s\n", record);
    }

    freeCache(params->checkpointCount + 1);
    free(record);
    free(newPattern);
    free(idArr);
    // summary();
}

unsigned int findId(int pos, Params const *params)
{
    char *id = (char *)malloc(16);
    short i = 0;
    int rank;
    int isId = 0;
    char ch = rebuildCached(pos, &rank, params);
    while (ch != '[' && i < MAX_RECORD_LENGTH)
    {
        if (ch == ']')
        {
            isId = 1;
        }
        pos = nthChar(rank, ch, params->cTable);
        ch = rebuildCached(pos, &rank, params);
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

char rebuildCached(int pos, int *rank, Params const *params)
{
    int cp = findIndex(params->positions, params->checkpointCount, pos);
    int count;
    int startPos;

    struct cacheItem *item = cache[cp];
    struct cacheItem *prev = NULL;
    while (item != NULL)
    {
        if (item->pos <= pos && item->pos + item->count > pos)
        {
            cacheHits++;
            // Found a match in the cache. Use the cached result.
            *rank = item->rank + pos - item->pos;
            char ch = item->ch;
            if (item->count == 1)
            {
                // Remove the item from the cache.
                if (prev == NULL)
                {
                    cache[cp] = item->next;
                }
                else
                {
                    prev->next = item->next;
                }
                free(item);
                cacheSize--;
            }
            return ch;
        }
        prev = item;
        item = item->next;
    }
    cacheMisses++;

    // No matching cache item found. Compute the result.
    char newCh = decode(pos, rank, &count, &startPos, params);

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
        if (cacheSize > maxCacheSize)
        {
            maxCacheSize = cacheSize;
        }
    }

    return newCh;
}

void freeCache(int n)
{
    int deepest = 0;
    for (int i = 0; i < n; i++)
    {
        int depth = 0;
        struct cacheItem *item = cache[i];
        while (item != NULL)
        {
            depth++;
            struct cacheItem *next = item->next;
            free(item);
            item = next;
        }
        if (depth > deepest)
        {
            deepest = depth;
        }
    }
    free(cache);
    // printf("Deepest cache: %d\n", deepest);
    // printf("Cache hits: %d\n", cacheHits);
    // printf("Cache misses: %d\n", cacheMisses);
    // printf("Max cache size: %d\n", maxCacheSize);
}

void rebuildRec(char *record, int pos, Params const *params)
{
    short i = 0;
    int rank;
    char ch = rebuildCached(pos, &rank, params);
    while (ch != ']' && i < MAX_RECORD_LENGTH)
    {
        record[i++] = ch;
        pos = nthChar(rank, ch, params->cTable);
        ch = rebuildCached(pos, &rank, params);
    }
    record[i] = '\0';
    reverse(record);
}