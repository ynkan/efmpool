#include <stdio.h>
#include "shared.h"

#define CHECK(c) do { if(!(c)) { \
    fprintf(stderr, "[FAIL] consumer: %s, line %d\n", #c, __LINE__); \
    exit(EXIT_FAILURE); } } while(0)

int main(void)
{
    struct alignment_s { char prefix; Point_t value; };
    fmpool_t(Point_t)* Pool;
    Point_t* Points[5];
    fmpool_pool_t* raw;
    unsigned char* bytes[9];
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("[RUN] Installed library consumer (thread_safe=%d, checks=%d)\n",
            fmpool_is_thread_safe(), fmpool_checks_enabled());
    CHECK(fmpool_is_thread_safe() == (FMPOOL_THREAD_SAFE != 0));
    CHECK(fmpool_checks_enabled() == (FMPOOL_CHECKS != 0));
    Pool = fmpool_create_ex(Point_t, 1, 2);
    CHECK(Pool);
    for(size_t i = 0; i < 5; i++)
    {
        Points[i] = allocate_point(Pool, (long double)i);
        CHECK(Points[i]);
        CHECK((uintptr_t)Points[i] % offsetof(struct alignment_s, value) == 0);
    }
    CHECK(fmpool_capacity(Point_t, Pool) == 5);
    for(size_t i = 0; i < 5; i++)
    {
        CHECK(Points[i]->x == (long double)i && Points[i]->tag == 'p');
        CHECK(fmpool_index(Point_t, Pool, Points[i]) == i);
        CHECK(fmpool_at(Point_t, Pool, i) == Points[i]);
        CHECK(fmpool_free(Point_t, Points[i], Pool));
    }
    fmpool_destroy(Point_t, Pool);
    puts("[PASS] Allocate in another translation unit, preserve alignment and free here");

    CHECK(!fmpool_pool_create(0, 1, 1));
    CHECK(!fmpool_pool_create(SIZE_MAX, 1, 1));
    CHECK(fmpool_pool_capacity(NULL) == 0);
    raw = fmpool_pool_create(3, 1, 2);
    CHECK(raw);
    for(size_t i = 0; i < 9; i++)
    {
        bytes[i] = fmpool_pool_get(raw);
        CHECK(bytes[i]);
        bytes[i][0] = (unsigned char)i;
        bytes[i][2] = 42;
    }
    CHECK(fmpool_pool_capacity(raw) == 9);
    for(size_t i = 0; i < 9; i++)
    {
        CHECK(bytes[i][0] == i && bytes[i][2] == 42);
        CHECK(fmpool_pool_index(raw, bytes[i]) == i);
        CHECK(fmpool_pool_at(raw, i) == bytes[i]);
        CHECK(fmpool_pool_free(bytes[i], raw));
    }
    CHECK(fmpool_pool_get(raw) == bytes[8]);
    fmpool_pool_destroy(raw);
    puts("[PASS] Generic API: tiny objects, growth, indices, reuse and invalid sizes");
    puts("[SUMMARY] consumer: 2 passed, 0 failed");
    return 0;
}
