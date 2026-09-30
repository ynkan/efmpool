/*
  The MIT License (MIT)
  Copyright © 2015-2026 Kang Yongning <yongningkang@gmail.com>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the “Software”), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "fmpool.h"
#include <string.h> /* memcpy */

#if FMPOOL_THREAD_SAFE
#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION fmpool_mutex_t;
static inline int fmpool_mutex_init(fmpool_mutex_t* m)
{
    InitializeCriticalSection(m);
    return 0;
}
static inline void fmpool_mutex_destroy(fmpool_mutex_t* m)
{
    DeleteCriticalSection(m);
}
static inline void fmpool_mutex_lock(fmpool_mutex_t* m)
{
    EnterCriticalSection(m);
}
static inline void fmpool_mutex_unlock(fmpool_mutex_t* m)
{
    LeaveCriticalSection(m);
}
#else
#include <pthread.h>
typedef pthread_mutex_t fmpool_mutex_t;
static inline int fmpool_mutex_init(fmpool_mutex_t* m)
{
    return pthread_mutex_init(m, NULL);
}
static inline void fmpool_mutex_destroy(fmpool_mutex_t* m)
{
    pthread_mutex_destroy(m);
}
static inline void fmpool_mutex_lock(fmpool_mutex_t* m)
{
    pthread_mutex_lock(m);
}
static inline void fmpool_mutex_unlock(fmpool_mutex_t* m)
{
    pthread_mutex_unlock(m);
}
#endif
#define FMPOOL_MUTEX_FIELD fmpool_mutex_t mutex;
#define FMPOOL_LOCK(P) fmpool_mutex_lock(&(P)->mutex)
#define FMPOOL_UNLOCK(P) fmpool_mutex_unlock(&(P)->mutex)
#define FMPOOL_MUTEX_INIT(P) fmpool_mutex_init(&(P)->mutex)
#define FMPOOL_MUTEX_DESTROY(P) fmpool_mutex_destroy(&(P)->mutex)
#else
#define FMPOOL_MUTEX_FIELD
#define FMPOOL_LOCK(P) ((void)0)
#define FMPOOL_UNLOCK(P) ((void)0)
#define FMPOOL_MUTEX_INIT(P) 0
#define FMPOOL_MUTEX_DESTROY(P) ((void)0)
#endif

/* C99 equivalent of fundamental maximum alignment for slot storage. */
typedef union fmpool_alignment_s
{
    long double floating;
    long long integer;
    void* pointer;
    void (*function)(void);
} fmpool_alignment_t;

typedef struct fmpool_block_s
{
    struct fmpool_block_s* next;
    unsigned char* items;
    unsigned char* used;
    size_t num;
    size_t first_index;
} fmpool_block_t;

struct fmpool_pool_s
{
    void* head;
    size_t num;
    fmpool_block_t* blocks;
    size_t growth;
    size_t stride;
    FMPOOL_MUTEX_FIELD
};

/* Internal helpers require exclusive access to the pool. */
static bool fmpool_grow(fmpool_pool_t* P, const size_t num)
{
    fmpool_block_t* B;
    if(!num || num > SIZE_MAX / P->stride || num > SIZE_MAX - P->num)
    {
        return false;
    }
    B = calloc(1, sizeof(*B));
    if(B == NULL)
    {
        return false;
    }
    B->items = calloc(num, P->stride);
#if FMPOOL_CHECKS
    B->used = calloc(num, 1);
#endif
    if(B->items == NULL || (FMPOOL_CHECKS && B->used == NULL))
    {
        free(B->used);
        free(B->items);
        free(B);
        return false;
    }
    B->num = num;
    B->first_index = P->num;
    for(size_t i = 0; i < num; i++)
    {
        void* next = i + 1 < num ? B->items + (i + 1) * P->stride : P->head;
        /* memcpy avoids aliasing an object's storage as a pointer object. */
        memcpy(B->items + i * P->stride, &next, sizeof(next));
    }
    B->next = P->blocks;
    P->blocks = B;
    P->head = B->items;
    P->num += num;
    return true;
}

static fmpool_block_t* fmpool_find(fmpool_pool_t* P, const void* OBJ,
        size_t* slot)
{
    uintptr_t addr = (uintptr_t)OBJ;
    for(fmpool_block_t* B = P->blocks; B; B = B->next)
    {
        uintptr_t base = (uintptr_t)B->items;
        if(addr >= base && addr - base < B->num * P->stride &&
                (addr - base) % P->stride == 0)
        {
            *slot = (addr - base) / P->stride;
            return B;
        }
    }
    return NULL;
}

fmpool_pool_t* fmpool_pool_create(size_t object_size,
        size_t num, size_t growth)
{
    fmpool_pool_t* P;
    size_t stride;
    const size_t alignment = sizeof(fmpool_alignment_t);
    if(num == 0)
    {
        return NULL; /* creating pool with zero items */
    }
    if(object_size == 0 || object_size > SIZE_MAX - (alignment - 1))
    {
        return NULL;
    }
    stride = (object_size + alignment - 1) / alignment * alignment;
    if(num > SIZE_MAX / stride)
    {
        return NULL;
    }
    P = calloc(1, sizeof(fmpool_pool_t));
    if(P == NULL)
    {
        return NULL; /* calloc failed */
    }
    if(FMPOOL_MUTEX_INIT(P) != 0)
    {
        free(P);
        return NULL;
    }
    P->growth = growth;
    P->stride = stride;
    if(!fmpool_grow(P, num))
    {
        FMPOOL_MUTEX_DESTROY(P);
        free(P);
        return NULL; /* calloc failed */
    }
    return P;
}

void fmpool_pool_destroy(fmpool_pool_t* P)
{
    if(P == NULL)
    {
        return;
    }
    while(P->blocks)
    {
        fmpool_block_t* B = P->blocks;
        P->blocks = B->next;
        free(B->used);
        free(B->items);
        free(B);
    }
    FMPOOL_MUTEX_DESTROY(P);
    free(P);
}

void* fmpool_pool_get(fmpool_pool_t* P)
{
    void* item;
    if(P == NULL)
    {
        return NULL;
    }
    FMPOOL_LOCK(P);
    if(!P->head && !fmpool_grow(P, P->growth))
    {
        FMPOOL_UNLOCK(P);
        return NULL;
    }
    item = P->head;
    memcpy(&P->head, item, sizeof(P->head));
#if FMPOOL_CHECKS
    {
        size_t slot;
        fmpool_block_t* B = fmpool_find(P, item, &slot);
        B->used[slot] = 1;
    }
#endif
    FMPOOL_UNLOCK(P);
    return item;
}

bool fmpool_pool_free(void* OBJ, fmpool_pool_t* P)
{
    if(!P || !OBJ)
    {
        return false;
    }
    FMPOOL_LOCK(P);
#if FMPOOL_CHECKS
    {
        size_t slot;
        fmpool_block_t* B = fmpool_find(P, OBJ, &slot);
        if(!B || !B->used[slot])
        {
            FMPOOL_UNLOCK(P);
            return false;
        }
        B->used[slot] = 0;
    }
#endif
    memcpy(OBJ, &P->head, sizeof(P->head));
    P->head = OBJ;
    FMPOOL_UNLOCK(P);
    return true;
}

size_t fmpool_pool_index(fmpool_pool_t* P, const void* OBJ)
{
    size_t result = SIZE_MAX;
    size_t slot;
    fmpool_block_t* B;
    if(!P || !OBJ)
    {
        return SIZE_MAX;
    }
    FMPOOL_LOCK(P);
    B = fmpool_find(P, OBJ, &slot);
    if(B)
    {
        result = B->first_index + slot;
    }
    FMPOOL_UNLOCK(P);
    return result;
}

void* fmpool_pool_at(fmpool_pool_t* P, size_t index)
{
    void* result = NULL;
    if(P == NULL)
    {
        return NULL;
    }
    FMPOOL_LOCK(P);
    for(fmpool_block_t* B = P->blocks; B; B = B->next)
    {
        if(index >= B->first_index && index - B->first_index < B->num)
        {
            result = B->items + (index - B->first_index) * P->stride;
            break;
        }
    }
    FMPOOL_UNLOCK(P);
    return result;
}

size_t fmpool_pool_capacity(fmpool_pool_t* P)
{
    size_t num;
    if(P == NULL)
    {
        return 0;
    }
    FMPOOL_LOCK(P);
    num = P->num;
    FMPOOL_UNLOCK(P);
    return num;
}

bool fmpool_is_thread_safe(void)
{
    return FMPOOL_THREAD_SAFE != 0;
}

bool fmpool_checks_enabled(void)
{
    return FMPOOL_CHECKS != 0;
}
