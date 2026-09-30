#include <stdio.h>
#include "fmpool.h"
typedef struct { double x, y; } Point_t;
FMPOOL_INIT(Point_t)
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "[FAIL] growthtest: %s, line %d\n", #c, __LINE__); exit(1); } } while (0)

int main(void)
{
    Point_t* objects[35];
    volatile size_t oversized = SIZE_MAX;
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("[RUN] growthtest (thread_safe=%d, checks=%d)\n", FMPOOL_THREAD_SAFE, FMPOOL_CHECKS);
    puts("[RUN] Invalid capacity and NULL arguments");
    fmpool_t(Point_t)* pool = fmpool_create_ex(Point_t, 2, 3);
    CHECK(pool);
    CHECK(fmpool_is_thread_safe() == (FMPOOL_THREAD_SAFE != 0));
    CHECK(fmpool_checks_enabled() == (FMPOOL_CHECKS != 0));
    CHECK(!fmpool_create(Point_t, 0));
    CHECK(!fmpool_create(Point_t, oversized));
    CHECK(!fmpool_get(Point_t, NULL));
    CHECK(!fmpool_free(Point_t, NULL, pool));
    fmpool_destroy(Point_t, NULL);
    puts("[PASS] Invalid capacity and NULL arguments");
    puts("[RUN] Block growth preserves addresses, indices and object access");
    for (size_t i = 0; i < 35; ++i) {
        objects[i] = fmpool_get(Point_t, pool);
        CHECK(objects[i]);
        for (size_t j = 0; j < i; ++j) CHECK(objects[i] != objects[j]);
        objects[i]->x = (double)i;
        CHECK(fmpool_index(Point_t, pool, objects[i]) == i);
        for (size_t j = 0; j <= i; ++j) {
            CHECK(fmpool_at(Point_t, pool, j) == objects[j]);
            CHECK(fmpool_index(Point_t, pool, objects[j]) == j);
            CHECK(objects[j]->x == (double)j);
            fmpool_at(Point_t, pool, j)->y = (double)(i + j);
            CHECK(objects[j]->y == (double)(i + j));
        }
    }
    CHECK(fmpool_capacity(Point_t, pool) == 35);
    printf("[PASS] Growth: initial=2, block=3, objects=35, capacity=%zu\n", fmpool_capacity(Point_t, pool));
    puts("[RUN] Invalid index and pointer lookups");
    CHECK(!fmpool_at(Point_t, pool, 35));
    CHECK(!fmpool_at(Point_t, pool, SIZE_MAX));
    CHECK(fmpool_index(Point_t, pool, NULL) == SIZE_MAX);
    { Point_t foreign; CHECK(fmpool_index(Point_t, pool, &foreign) == SIZE_MAX); }
    CHECK(fmpool_index(Point_t, pool, (Point_t*)((unsigned char*)objects[0] + 1)) == SIZE_MAX);
    for (size_t i = 0; i < 35; ++i) CHECK(objects[i]->x == (double)i);
    puts("[PASS] Invalid index and pointer lookups");
#if FMPOOL_CHECKS
    puts("[RUN] Reject foreign, interior and unallocated pointers");
    {
        Point_t foreign;
        fmpool_t(Point_t)* other = fmpool_create(Point_t, 1);
        CHECK(other);
        CHECK(!fmpool_free(Point_t, &foreign, pool));
        CHECK(!fmpool_free(Point_t, objects[0], other));
        CHECK(!fmpool_free(Point_t, (Point_t*)((unsigned char*)objects[0] + 1), pool));
        CHECK(!fmpool_free(Point_t, fmpool_at(Point_t, other, 0), other));
        fmpool_destroy(Point_t, other);
    }
    puts("[PASS] Reject foreign, interior and unallocated pointers");
#else
    puts("[SKIP] Invalid return checks (FMPOOL_CHECKS=0)");
#endif
    puts("[RUN] Return and reuse objects across blocks");
    for (size_t i = 0; i < 35; ++i) CHECK(fmpool_free(Point_t, objects[i], pool));
#if FMPOOL_CHECKS
    CHECK(!fmpool_free(Point_t, objects[34], pool));
    puts("[PASS] Reject duplicate return");
#else
    puts("[SKIP] Duplicate return check (FMPOOL_CHECKS=0)");
#endif
    CHECK(fmpool_get(Point_t, pool) == objects[34]);
    fmpool_destroy(Point_t, pool);
    puts("[PASS] Return and reuse objects across blocks");
    puts("[RUN] Fixed-capacity exhaustion and reuse");
    pool = fmpool_create_ex(Point_t, 1, 0);
    CHECK(pool);
    objects[0] = fmpool_get(Point_t, pool);
    CHECK(objects[0]);
    CHECK(!fmpool_get(Point_t, pool));
    CHECK(fmpool_free(Point_t, objects[0], pool));
    CHECK(fmpool_get(Point_t, pool) == objects[0]);
    fmpool_destroy(Point_t, pool);
    puts("[PASS] Fixed-capacity exhaustion and reuse");
    puts("[RUN] Failed growth preserves pool state");
    pool = fmpool_create_ex(Point_t, 1, SIZE_MAX);
    CHECK(pool);
    objects[0] = fmpool_get(Point_t, pool);
    CHECK(objects[0]);
    CHECK(!fmpool_get(Point_t, pool));
    CHECK(fmpool_capacity(Point_t, pool) == 1);
    CHECK(fmpool_free(Point_t, objects[0], pool));
    CHECK(fmpool_get(Point_t, pool) == objects[0]);
    fmpool_destroy(Point_t, pool);
    puts("[PASS] Failed growth preserves pool state");
    puts("[RUN] Default growth policy");
    pool = fmpool_create(Point_t, 1);
    CHECK(pool);
    CHECK(fmpool_get(Point_t, pool));
    CHECK(fmpool_get(Point_t, pool));
    CHECK(fmpool_capacity(Point_t, pool) == 2);
    fmpool_destroy(Point_t, pool);
    puts("[PASS] Default growth policy: capacity=2");
    printf("[SUMMARY] growthtest: %d passed, 0 failed, %d skipped\n",
            FMPOOL_CHECKS ? 9 : 7, FMPOOL_CHECKS ? 0 : 2);
    return 0;
}
