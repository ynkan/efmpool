#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <time.h>
#include <string.h> /* strcmp */
#include "fmpool.h"

#ifdef _WIN32
#include <windows.h>
#elif FMPOOL_THREAD_SAFE
#include <pthread.h>
#endif

typedef struct Point_s
{
    double x;
    double y;
} Point_t;

FMPOOL_INIT(Point_t)

#define ROUNDS 7
#define BATCH 64
#define REUSE_BATCHES 16384
#define GROWTH_OBJECTS 4096
#define GROWTH_BATCHES 16

static volatile double checksum;

static void fail(const char* message)
{
    fprintf(stderr, "[FAIL] variantbench: %s\n", message);
    exit(EXIT_FAILURE);
}

static uint64_t ticks(void)
{
#ifdef _WIN32
    LARGE_INTEGER value;
    if(!QueryPerformanceCounter(&value))
    {
        fail("QueryPerformanceCounter failed");
    }
    return (uint64_t)value.QuadPart;
#else
    struct timespec value;
    if(clock_gettime(CLOCK_MONOTONIC, &value) != 0)
    {
        fail("clock_gettime failed");
    }
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) + (uint64_t)value.tv_nsec;
#endif
}

static uint64_t elapsed_ns(uint64_t start, uint64_t end)
{
#ifdef _WIN32
    LARGE_INTEGER frequency;
    if(!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
    {
        fail("QueryPerformanceFrequency failed");
    }
    return (uint64_t)((long double)(end - start) * 1000000000.0L / frequency.QuadPart);
#else
    return end - start;
#endif
}

#define MAX_THREADS 4

#if FMPOOL_THREAD_SAFE
/* Reusable barriers keep startup outside timing and coordinate shared-pool growth. */
typedef struct barrier_s
{
#ifdef _WIN32
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE condition;
#else
    pthread_mutex_t mutex;
    pthread_cond_t condition;
#endif
    size_t participants;
    size_t arrived;
    size_t generation;
} barrier_t;

static void barrier_init(barrier_t* barrier, size_t participants)
{
    barrier->participants = participants;
    barrier->arrived = 0;
    barrier->generation = 0;
#ifdef _WIN32
    InitializeCriticalSection(&barrier->mutex);
    InitializeConditionVariable(&barrier->condition);
#else
    if(pthread_mutex_init(&barrier->mutex, NULL) != 0 ||
        pthread_cond_init(&barrier->condition, NULL) != 0)
    {
        fail("barrier initialization failed");
    }
#endif
}

static void barrier_wait(barrier_t* barrier)
{
    size_t generation;
#ifdef _WIN32
    EnterCriticalSection(&barrier->mutex);
#else
    if(pthread_mutex_lock(&barrier->mutex) != 0)
    {
        fail("barrier lock failed");
    }
#endif
    generation = barrier->generation;
    if(++barrier->arrived == barrier->participants)
    {
        barrier->arrived = 0;
        barrier->generation++;
#ifdef _WIN32
        WakeAllConditionVariable(&barrier->condition);
#else
        if(pthread_cond_broadcast(&barrier->condition) != 0)
        {
            fail("barrier broadcast failed");
        }
#endif
    }
    else
    {
        while(generation == barrier->generation)
        {
#ifdef _WIN32
            if(!SleepConditionVariableCS(&barrier->condition, &barrier->mutex, INFINITE))
#else
            if(pthread_cond_wait(&barrier->condition, &barrier->mutex) != 0)
#endif
            {
                fail("barrier wait failed");
            }
        }
    }
#ifdef _WIN32
    LeaveCriticalSection(&barrier->mutex);
#else
    if(pthread_mutex_unlock(&barrier->mutex) != 0)
    {
        fail("barrier unlock failed");
    }
#endif
}

static void barrier_destroy(barrier_t* barrier)
{
#ifdef _WIN32
    DeleteCriticalSection(&barrier->mutex);
#else
    if(pthread_cond_destroy(&barrier->condition) != 0 ||
        pthread_mutex_destroy(&barrier->mutex) != 0)
    {
        fail("barrier destruction failed");
    }
#endif
}
#endif

typedef struct workload_s
{
    fmpool_t(Point_t)* Pool;
    size_t threads;
    bool growth;
#if FMPOOL_THREAD_SAFE
    barrier_t control;
    barrier_t work;
#endif
} workload_t;

typedef struct worker_s
{
    workload_t* workload;
    size_t id;
    double sum;
} worker_t;

static void sync_workers(workload_t* workload)
{
#if FMPOOL_THREAD_SAFE
    if(workload->threads > 1)
    {
        barrier_wait(&workload->work);
    }
#else
    (void)workload;
#endif
}

static void run_worker(worker_t* worker)
{
    workload_t* workload = worker->workload;
    Point_t* objects[GROWTH_OBJECTS];
    size_t count = workload->growth ? GROWTH_OBJECTS / workload->threads : BATCH;
    size_t batches = workload->growth ? GROWTH_BATCHES : REUSE_BATCHES / workload->threads;
    double sum = 0.0;
    for(size_t batch = 0; batch < batches; batch++)
    {
        if(workload->growth)
        {
            if(worker->id == 0)
            {
                workload->Pool = fmpool_create_ex(Point_t, BATCH, BATCH);
                if(workload->Pool == NULL)
                {
                    fail("pool creation failed");
                }
            }
            sync_workers(workload);
        }
        for(size_t i = 0; i < count; i++)
        {
            size_t value = workload->growth ? worker->id * count + i : i;
            objects[i] = fmpool_get(Point_t, workload->Pool);
            if(objects[i] == NULL)
            {
                fail("allocation failed");
            }
            objects[i]->x = (double)value;
            objects[i]->y = (double)(value + 1);
        }
        if(workload->growth)
        {
            /* All 4096 objects remain allocated before any worker returns one. */
            sync_workers(workload);
        }
        for(size_t i = 0; i < count; i++)
        {
            sum += objects[i]->x + objects[i]->y;
            if(!fmpool_free(Point_t, objects[i], workload->Pool))
            {
                fail("return failed");
            }
        }
        if(workload->growth)
        {
            sync_workers(workload);
            if(worker->id == 0)
            {
                if(fmpool_capacity(Point_t, workload->Pool) != GROWTH_OBJECTS)
                {
                    fail("unexpected grown capacity");
                }
                fmpool_destroy(Point_t, workload->Pool);
                workload->Pool = NULL;
            }
            sync_workers(workload);
        }
    }
    worker->sum = sum;
}

#if FMPOOL_THREAD_SAFE
#ifdef _WIN32
static DWORD WINAPI worker_entry(LPVOID arg)
#else
static void* worker_entry(void* arg)
#endif
{
    worker_t* worker = arg;
    barrier_wait(&worker->workload->control); /* ready */
    barrier_wait(&worker->workload->control); /* timed start */
    run_worker(worker);
    barrier_wait(&worker->workload->control); /* timed finish */
    return 0;
}
#endif

static uint64_t sample(bool growth, size_t threads)
{
    workload_t workload = {0};
    worker_t workers[MAX_THREADS];
    uint64_t start, end;
    double sum = 0.0;
    double expected = growth ? (double)GROWTH_OBJECTS * GROWTH_OBJECTS * GROWTH_BATCHES :
        (double)BATCH * BATCH * REUSE_BATCHES;
    workload.threads = threads;
    workload.growth = growth;
    if(!growth)
    {
        /* Identical fixed capacity in all three groups, sufficient for four batches. */
        workload.Pool = fmpool_create_ex(Point_t, BATCH * MAX_THREADS, 0);
        if(workload.Pool == NULL)
        {
            fail("pool creation failed");
        }
    }
    for(size_t i = 0; i < threads; i++)
    {
        workers[i].workload = &workload;
        workers[i].id = i;
        workers[i].sum = 0.0;
    }
#if FMPOOL_THREAD_SAFE
    if(threads > 1)
    {
#ifdef _WIN32
        HANDLE handles[MAX_THREADS];
#else
        pthread_t handles[MAX_THREADS];
#endif
        barrier_init(&workload.control, threads + 1);
        barrier_init(&workload.work, threads);
        for(size_t i = 0; i < threads; i++)
        {
#ifdef _WIN32
            handles[i] = CreateThread(NULL, 0, worker_entry, &workers[i], 0, NULL);
            if(handles[i] == NULL)
#else
            if(pthread_create(&handles[i], NULL, worker_entry, &workers[i]) != 0)
#endif
            {
                fail("thread creation failed");
            }
        }
        barrier_wait(&workload.control);
        start = ticks();
        barrier_wait(&workload.control);
        barrier_wait(&workload.control);
        end = ticks();
        for(size_t i = 0; i < threads; i++)
        {
#ifdef _WIN32
            if(WaitForSingleObject(handles[i], INFINITE) != WAIT_OBJECT_0)
            {
                fail("thread join failed");
            }
            CloseHandle(handles[i]);
#else
            if(pthread_join(handles[i], NULL) != 0)
            {
                fail("thread join failed");
            }
#endif
        }
        barrier_destroy(&workload.work);
        barrier_destroy(&workload.control);
    }
    else
#endif
    {
        start = ticks();
        run_worker(&workers[0]);
        end = ticks();
    }
    for(size_t i = 0; i < threads; i++)
    {
        sum += workers[i].sum;
    }
    if(sum != expected)
    {
        fail("object data checksum mismatch");
    }
    checksum += sum;
    if(!growth)
    {
        if(fmpool_capacity(Point_t, workload.Pool) != BATCH * MAX_THREADS)
        {
            fail("reuse unexpectedly changed capacity");
        }
        fmpool_destroy(Point_t, workload.Pool);
    }
    return elapsed_ns(start, end);
}

static void measure(const char* name, bool growth, size_t threads, size_t pairs)
{
    uint64_t samples[ROUNDS];
    uint64_t median;
    (void)sample(growth, threads); /* warm up the complete scenario before recording rounds */
    for(size_t i = 0; i < ROUNDS; i++)
    {
        samples[i] = sample(growth, threads);
        printf("[SAMPLE] %s threads=%zu %s round=%zu elapsed_ms=%.3f aggregate_ns/pair=%.3f\n",
            fmpool_is_thread_safe() ? "MT" : "ST", threads, name, i + 1, (double)samples[i] / 1e6, (double)samples[i] / pairs);
    }
    for(size_t i = 1; i < ROUNDS; i++)
    {
        uint64_t value = samples[i];
        size_t j = i;
        while(j > 0 && samples[j - 1] > value)
        {
            samples[j] = samples[j - 1];
            j--;
        }
        samples[j] = value;
    }
    median = samples[ROUNDS / 2];
    if(median == 0)
    {
        fail("timer resolution is insufficient");
    }
    printf("[SUMMARY] %s threads=%zu %s median_ms=%.3f aggregate_ns/pair=%.3f total_pairs/s=%.0f\n",
        fmpool_is_thread_safe() ? "MT" : "ST", threads, name, (double)median / 1e6, (double)median / pairs,
        (double)pairs * 1e9 / median);
    /* Integer machine-readable output for the CMake comparison driver. */
    printf("RESULT %s pairs=%zu median_ns=%llu checks=%d thread_safe=%d threads=%zu\n",
        name, pairs, (unsigned long long)median,
        fmpool_checks_enabled(), fmpool_is_thread_safe(), threads);
}

int main(int argc, char** argv)
{
    size_t threads = 1;
    if(argc == 3 && strcmp(argv[1], "--threads") == 0)
    {
        if(strcmp(argv[2], "4") == 0)
        {
            threads = 4;
        }
        else if(strcmp(argv[2], "1") != 0)
        {
            fail("thread count must be 1 or 4");
        }
    }
    else if(argc != 1)
    {
        fail("usage: variantbench [--threads 1|4]");
    }
    if(threads > 1 && !fmpool_is_thread_safe())
    {
        fail("ST does not support a shared pool with 4 threads; use MT");
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("[RUN] variant=%s threads=%zu checks=%d rounds=%d\n",
        fmpool_is_thread_safe() ? "MT" : "ST", threads, fmpool_checks_enabled(), ROUNDS);
    if(fmpool_is_thread_safe() != (FMPOOL_THREAD_SAFE != 0) ||
        fmpool_checks_enabled() != (FMPOOL_CHECKS != 0))
    {
        fail("linked library configuration does not match the target");
    }
    puts("[INFO] Same total get/free pairs in every group; workers share one pool.");
    puts("[INFO] Thread creation/join excluded; start/finish synchronization included.");
    puts("[INFO] reuse excludes pool creation; growth includes pool creation/destruction and phase barriers.");
    puts("[INFO] aggregate_ns/pair is wall time / total pairs, not individual operation latency.");
    measure("reuse", false, threads, (size_t)BATCH * REUSE_BATCHES);
    measure("growth", true, threads, (size_t)GROWTH_OBJECTS * GROWTH_BATCHES);
    printf("[PASS] Two scenarios completed; checksum=%.0f\n", checksum);
    return 0;
}
