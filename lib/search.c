#include "search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *reverse(char str[])
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

struct cacheRL
{
    char ch;
    int rank;
    int pos;
    int count;
    struct cacheRL *next;
};

static struct cacheRL **cache;
static int cacheSize = 0;
static unsigned int recordCount = 0;
static unsigned int minId = 0;

int searchPattern(const char *pattern, int *end, Params const *params)
{
    int indexStart = params->cTable[map(pattern[0])];
    int indexEnd = params->cTable[map(pattern[0]) + 1];
    int occStart;
    int occEnd;
    for (int i = 1; i < strlen(pattern); i++)
    {
        occStart = occFunc(pattern[i], indexStart, params);
        occEnd = occFunc(pattern[i], indexEnd, params);
        indexStart = nthChar(occStart, pattern[i], params->cTable);
        indexEnd = nthChar(occEnd, pattern[i], params->cTable);
    }
    *end = indexEnd;
    return indexStart;
}

void findMinId(Params const *Params)
{
    recordCount = Params->cTable[map('[') + 1] - Params->cTable[map('[')];
    unsigned int right = findId(0, Params);
    unsigned int left = 0;
    if (right >= recordCount)
    {
        left = right - recordCount;
    }
    char pattern[16];
    int indexEnd;
    while (left < right)
    {
        unsigned int mid = (left + right) / 2;
        sprintf(pattern, "[%d]", mid);
        reverse(pattern);
        int indexStart = searchPattern(pattern, &indexEnd, Params);
        if (indexStart == indexEnd)
        {
            left = mid + 1;
        }
        else
        {
            right = mid;
        }
    }
    minId = left;
}

int searchID(unsigned int id, Params const *params)
{
    static char pattern[16];
    sprintf(pattern, "[%d]", id);
    reverse(pattern);
    int index = params->cTable[']' - 39];
    for (int i = 1; i < strlen(pattern); i++)
    {
        int occ = occFunc(pattern[i], index, params);
        index = nthChar(occ, pattern[i], params->cTable);
    }
    return index;
}

void search(char const *pattern, Params const *params)
{
    int indexEnd;
    cache = calloc(params->checkpointCount + 1, sizeof(struct cacheRL *));
    int indexStart = searchPattern(pattern, &indexEnd, params);
    unsigned int *idArr = (unsigned int *)malloc((indexEnd - indexStart) * sizeof(unsigned int));
    int matches = 0;
    for (int i = indexStart; i < indexEnd; i++)
    {
        // To decode entire record, we need to find the next record, so plus 1
        idArr[matches++] = findId(i, params) + 1;
    }
    if (recordCount == 0)
    {
        findMinId(params);
    }
    qsort(idArr, matches, sizeof(int), compareInt);
    char *record = malloc(MAX_RECORD_LENGTH * sizeof(char));
    unsigned int upperBound = minId + recordCount;
    for (int i = 0; i < matches; i++)
    {
        if (i > 0 && idArr[i] == idArr[i - 1])
        {
            // skip duplicate
            continue;
        }
        if (idArr[i] == upperBound)
        {
            indexStart = searchID(minId, params);
        }
        else
        {
            indexStart = searchID(idArr[i], params);
        }
        sprintf(record, "[%d]", idArr[i] - 1);
        rebuildRec(record + strlen(record), indexStart, params);
        printf("%s\n", record);
    }

    freeCache(params->checkpointCount + 1);
    free(record);
    free(idArr);
}

unsigned int findId(int pos, Params const *params)
{
    static char id[16];
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
    char *end;
    unsigned int idInt = (unsigned int)strtol(id, &end, 10);
    return idInt;
}

char rebuildCached(int pos, int *rank, Params const *params)
{
    int cp = findIndex(params->positions, params->checkpointCount, pos);
    struct cacheRL *item = cache[cp];
    struct cacheRL **prev = &cache[cp];
    while (item != NULL)
    {
        if (item->pos <= pos && item->pos + item->count > pos)
        {
            // Found a match in the cache. Use the cached result.
            *rank = item->rank + pos - item->pos;
            char ch = item->ch;
            if (item->count == 1)
            {
                // Remove the item from the cache.
                *prev = item->next;
                free(item);
                cacheSize--;
            }
            return ch;
        }
        prev = &(*prev)->next;
        item = item->next;
    }

    // No matching cache item found. Compute the result.
    int count;
    int startPos;
    char newCh = decode(pos, rank, &count, &startPos, params);

    if (cacheSize < CACHE_SIZE)
    {
        // Allocate a new cache item.
        struct cacheRL *newItem = malloc(sizeof(struct cacheRL));
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
    for (int i = 0; i < n; i++)
    {
        struct cacheRL *item = cache[i];
        while (item != NULL)
        {
            struct cacheRL *next = item->next;
            free(item);
            item = next;
        }
    }
    free(cache);
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
