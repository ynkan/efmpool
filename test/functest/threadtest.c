#include <stdio.h>
#include "fmpool.h"

#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION test_mutex_t;
static inline int test_mutex_init(test_mutex_t* m)
{
    InitializeCriticalSection(m);
    return 0;
}
static inline void test_mutex_destroy(test_mutex_t* m)
{
    DeleteCriticalSection(m);
}
static inline void test_mutex_lock(test_mutex_t* m)
{
    EnterCriticalSection(m);
}
static inline void test_mutex_unlock(test_mutex_t* m)
{
    LeaveCriticalSection(m);
}
#else
#include <pthread.h>
typedef pthread_mutex_t test_mutex_t;
static inline int test_mutex_init(test_mutex_t* m)
{
    return pthread_mutex_init(m, NULL);
}
static inline void test_mutex_destroy(test_mutex_t* m)
{
    pthread_mutex_destroy(m);
}
static inline void test_mutex_lock(test_mutex_t* m)
{
    pthread_mutex_lock(m);
}
static inline void test_mutex_unlock(test_mutex_t* m)
{
    pthread_mutex_unlock(m);
}
#endif

typedef struct { size_t owner, value; } Item;
FMPOOL_INIT(Item)
#define NTHREADS 4
#define COUNT 128
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "[FAIL] threadtest: %s, line %d\n", #c, __LINE__); abort(); } } while (0)
static fmpool_t(Item)* pool;
static Item* held[NTHREADS][COUNT];
static test_mutex_t gate;
static size_t arrived;
#ifdef _WIN32
static DWORD WINAPI worker(LPVOID arg)
#else
static void* worker(void* arg)
#endif
{
    size_t id = *(size_t*)arg;
    for (size_t i = 0; i < COUNT; ++i) {
        held[id][i] = fmpool_get(Item, pool);
        CHECK(held[id][i]);
    }
    test_mutex_lock(&gate);
    ++arrived;
    test_mutex_unlock(&gate);
    for (;;) {
        size_t ready;
        test_mutex_lock(&gate);
        ready = arrived;
        test_mutex_unlock(&gate);
        if (ready == NTHREADS) break;
    }
    /* All objects remain checked out, allowing a cross-thread uniqueness check. */
    for (size_t i = 0; i < COUNT; ++i)
        for (size_t t = 0; t < NTHREADS; ++t)
            for (size_t j = 0; j < COUNT; ++j)
                if (t != id || j != i) CHECK(held[id][i] != held[t][j]);
    /* No thread modifies held[][] after the barrier. */
    for (size_t i = 0; i < COUNT; ++i) CHECK(fmpool_free(Item, held[id][i], pool));
    for (size_t i = 0; i < 20000; ++i) {
        Item* p = fmpool_get(Item, pool);
        CHECK(p);
        p->owner = id; p->value = i;
        CHECK(p->owner == id && p->value == i);
        CHECK(fmpool_free(Item, p, pool));
    }
    return 0;
}
int main(void)
{
    size_t ids[NTHREADS];
#ifdef _WIN32
    HANDLE threads[NTHREADS];
#else
    pthread_t threads[NTHREADS];
#endif
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("[RUN] threadtest (thread_safe=%d, checks=%d)\n", FMPOOL_THREAD_SAFE, FMPOOL_CHECKS);
    printf("[RUN] %d threads, %d held objects each, 20000 get/free cycles each\n", NTHREADS, COUNT);
    pool = fmpool_create(Item, 2);
    CHECK(pool);
    CHECK(test_mutex_init(&gate) == 0);
    for (size_t i = 0; i < NTHREADS; ++i) {
        ids[i] = i;
#ifdef _WIN32
        threads[i] = CreateThread(NULL, 0, worker, &ids[i], 0, NULL);
        CHECK(threads[i]);
#else
        CHECK(pthread_create(&threads[i], NULL, worker, &ids[i]) == 0);
#endif
    }
    for (size_t i = 0; i < NTHREADS; ++i) {
#ifdef _WIN32
        CHECK(WaitForSingleObject(threads[i], INFINITE) == WAIT_OBJECT_0);
        CloseHandle(threads[i]);
#else
        CHECK(pthread_join(threads[i], NULL) == 0);
#endif
    }
    puts("[PASS] Concurrent growth and 512 distinct object addresses");
    puts("[PASS] 80000 concurrent get/free cycles completed");
    printf("[INFO] Final pool capacity: %zu\n", fmpool_capacity(Item, pool));
    test_mutex_destroy(&gate);
    fmpool_destroy(Item, pool);
    puts("[SUMMARY] threadtest: 2 passed, 0 failed");
    return 0;
}
