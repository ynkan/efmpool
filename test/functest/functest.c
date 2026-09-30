#include <time.h>
#include <stdio.h>

#include "fmpool.h"

typedef struct Point_s
{
  double x;
  double y;
} Point_t;

FMPOOL_INIT(Point_t)

#define FAILTEST(testcond,failure_msg) \
   if((testcond)) { \
       fputs("[FAIL] ",stderr); fputs(failure_msg,stderr); fputc('\n',stderr); \
       exit(EXIT_FAILURE); \
   }

#define NUMITEMS 10
#if FMPOOL_CHECKS
static void free_under_and_overflow_test(const char* failure_msg) {
  fmpool_t(Point_t)* pool = fmpool_create(Point_t,NUMITEMS);
  FAILTEST(pool == NULL,"Failed creating bounds-test pool");
  Point_t* p = fmpool_get(Point_t,pool);
  FAILTEST(p == NULL,"Failed allocating bounds-test object");
  /* test the underflow case */
  FAILTEST(fmpool_free(Point_t,(Point_t*)((uintptr_t)p - sizeof(*p)),pool) != false,failure_msg);
  /* test the overflow case */
  FAILTEST(fmpool_free(Point_t,(Point_t*)((uintptr_t)p + NUMITEMS * sizeof(*p)),pool) != false,failure_msg);
  FAILTEST(fmpool_free(Point_t,p,pool) != true,failure_msg);
  if(pool) {
    fmpool_destroy(Point_t, pool);
  }
}
#endif
static void zero_allocation_test(const char* failure_msg) {
  fmpool_t(Point_t)* p;
  FAILTEST(((p = fmpool_create(Point_t,0)) != NULL),failure_msg);
  if(p) {
    fmpool_destroy(Point_t, p);
  }
}

static void* allocate_test(const size_t num,
                             bool (*testfn)(fmpool_t(Point_t)*),
                             const char* failure_msg)
{
  fmpool_t(Point_t)* p;
  FAILTEST(((p = fmpool_create_ex(Point_t,num,0)) == NULL),failure_msg);
  FAILTEST((testfn(p) == false),failure_msg);
  fmpool_destroy(Point_t, p);
  return NULL;
}

static bool one_allocation_test(fmpool_t(Point_t)* pool) {
    return(fmpool_get(Point_t,pool) != NULL &&
           fmpool_get(Point_t,pool) == NULL);
}

static bool freelist_simple_test(fmpool_t(Point_t)* pool) {
    Point_t* p = fmpool_get(Point_t,pool);
    (void)fmpool_get(Point_t,pool);
    fmpool_free(Point_t,p,pool);
    return(p == fmpool_get(Point_t,pool));
}

int main()
{
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("[RUN] functest (thread_safe=%d, checks=%d)\n", FMPOOL_THREAD_SAFE, FMPOOL_CHECKS);
  puts("[RUN] Reject zero capacity");
  zero_allocation_test("Allocating zero-length pool isn't allowed");
  puts("[PASS] Reject zero capacity");
  puts("[RUN] Fixed-capacity exhaustion");
  allocate_test(1,one_allocation_test, "Failed allocating one-length pool");
  puts("[PASS] Fixed-capacity exhaustion");
  puts("[RUN] Free-list reuse");
  allocate_test(2,freelist_simple_test,"Failed freelist simple test");
  puts("[PASS] Free-list reuse");
#if FMPOOL_CHECKS
  puts("[RUN] Reject out-of-pool returns");
  free_under_and_overflow_test("Failed under/over flow test");
  puts("[PASS] Reject out-of-pool returns");
  puts("[SUMMARY] functest: 4 passed, 0 failed, 0 skipped");
#else
  puts("[SKIP] Reject out-of-pool returns (FMPOOL_CHECKS=0)");
  puts("[SUMMARY] functest: 3 passed, 0 failed, 1 skipped");
#endif
  return 0;
}

