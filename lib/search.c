#include "search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "thpool.h"
#include <sys/sysinfo.h>

extern Params *params;

char *reverse(char str[])
{
    int length = strlen(str);
    int i;
    char temp;
    for (i = 0; i < length / 2; i++)
    {
        temp = str[i];
        str[i] = str[length - i - 1];
        str[length - i - 1] = temp;
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

int searchPattern(const char *pattern, int *end)
{
    int indexStart = params->cTable[map(pattern[0])];
    int indexEnd = params->cTable[map(pattern[0]) + 1];
    int occStart;
    int occEnd;
    for (int i = 1; i < strlen(pattern); i++)
    {
        occStart = occFunc(pattern[i], indexStart);
        occEnd = occFunc(pattern[i], indexEnd);
        indexStart = nthChar(occStart, pattern[i], params->cTable);
        indexEnd = nthChar(occEnd, pattern[i], params->cTable);
    }
    *end = indexEnd;
    return indexStart;
}

void findMinId()
{
    recordCount = params->cTable[map('[') + 1] - params->cTable[map('[')];
    unsigned int right;
    struct FindIdParams *fParams = malloc(sizeof(struct FindIdParams));
    fParams->pos = 0;
    fParams->id = &right;
    findId(fParams);
    free(fParams);
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
        int indexStart = searchPattern(pattern, &indexEnd);
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

int searchID(unsigned int id)
{
    char pattern[16];
    sprintf(pattern, "[%d]", id);
    reverse(pattern);
    int index = params->cTable[']' - 39];
    for (int i = 1; i < strlen(pattern); i++)
    {
        int occ = occFunc(pattern[i], index);
        index = nthChar(occ, pattern[i], params->cTable);
    }
    return index;
}

void search(char const *pattern)
{
    int indexEnd;
    cache = calloc(params->checkpointCount + 1, sizeof(struct cacheRL *));
    int indexStart = searchPattern(pattern, &indexEnd);
    unsigned int *idArr = (unsigned int *)malloc((indexEnd - indexStart) * sizeof(unsigned int));
    int matches = indexEnd - indexStart;
    // get cpu thread count
    int cpuCount = get_nprocs();
    threadpool pool = thpool_init(cpuCount);
    struct FindIdParams **fParams = malloc(matches * sizeof(struct FindIdParams *));
    for (int i = 0; i < matches; i++)
    {
        // To decode entire record, we need to find the next record, so plus 1
        fParams[i] = malloc(sizeof(struct FindIdParams));
        fParams[i]->pos = indexStart + i;
        fParams[i]->id = idArr + i;
        // findId(&fParams);
        thpool_add_work(pool, (void *)findId, fParams[i]);
    }
    thpool_wait(pool);
    thpool_destroy(pool);
    for (int i = 0; i < matches; i++)
    {
        free(fParams[i]);
        idArr[i]++;
    }
    free(fParams);
    if (recordCount == 0)
    {
        findMinId();
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
            indexStart = searchID(minId);
        }
        else
        {
            indexStart = searchID(idArr[i]);
        }
        sprintf(record, "[%d]", idArr[i] - 1);
        rebuildRec(record + strlen(record), indexStart);
        printf("%s\n", record);
    }
    freeCache(params->checkpointCount + 1);
    free(record);
    free(idArr);
}

void *findId(void *_params)
{
    struct FindIdParams *__params = (struct FindIdParams *)_params;
    int pos = __params->pos;
    char id[16];
    short i = 0;
    int rank;
    int isId = 0;
    char ch = rebuildCached(pos, &rank);
    while (ch != '[' && i < MAX_RECORD_LENGTH)
    {
        if (ch == ']')
        {
            isId = 1;
        }
        pos = nthChar(rank, ch, params->cTable);
        ch = rebuildCached(pos, &rank);
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
    *(__params->id) = idInt;
    return NULL;
}

char rebuildCached(int pos, int *rank)
{
    int cp = findIndex(params->positions, params->checkpointCount, pos);
    // lock mutex
    // pthread_mutex_lock(&mutex);
    // struct cacheRL *item = cache[cp];
    // struct cacheRL **prev = &cache[cp];
    // while (item != NULL)
    // {
    //     if (item->pos <= pos && item->pos + item->count > pos)
    //     {
    //         // Found a match in the cache. Use the cached result.
    //         *rank = item->rank + pos - item->pos;
    //         char ch = item->ch;
    //         if (item->count == 1)
    //         {
    //             // Remove the item from the cache.
    //             *prev = item->next;
    //             free(item);
    //             cacheSize--;
    //         }
    //         pthread_mutex_unlock(&mutex);
    //         return ch;
    //     }
    //     prev = &(*prev)->next;
    //     item = item->next;
    // }
    // pthread_mutex_unlock(&mutex);

    // No matching cache item found. Compute the result.
    int count;
    int startPos;
    char newCh = decode(pos, rank, &count, &startPos);

    // if (cacheSize < CACHE_SIZE)
    // {
    //     // Allocate a new cache item.
    //     struct cacheRL *newItem = malloc(sizeof(struct cacheRL));
    //     newItem->ch = newCh;
    //     newItem->rank = *rank - pos + startPos;
    //     newItem->pos = startPos;
    //     newItem->count = count;

    //     pthread_mutex_lock(&mutex);
    //     newItem->next = cache[cp];
    //     cache[cp] = newItem;
    //     cacheSize++;
    //     pthread_mutex_unlock(&mutex);
    // }

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

void rebuildRec(char *record, int pos)
{
    short i = 0;
    int rank;
    char ch = rebuildCached(pos, &rank);
    while (ch != ']' && i < MAX_RECORD_LENGTH)
    {
        record[i++] = ch;
        pos = nthChar(rank, ch, params->cTable);
        ch = rebuildCached(pos, &rank);
    }
    record[i] = '\0';
    reverse(record);
}
