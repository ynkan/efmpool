#include <stdio.h>

#include "fmpool.h"

/* define a "point" struct with x and y coordinates as doubles */
typedef struct Point_s
{
  double x;
  double y;
} Point_t;

/* macro that initializes fmpool functions that take Point_t structs as
   arguments - compile-time only */
FMPOOL_INIT(Point_t)

#define NUM_POINTS 16

int main()
{
  setvbuf(stdout, NULL, _IONBF, 0);
  puts("[RUN] example: create, allocate, return and destroy");
  /* create pool - requests memory from OS */
  fmpool_t(Point_t)* Pool;
  Pool = fmpool_create(Point_t, NUM_POINTS);
  if(Pool == NULL)
  {
    fputs("[FAIL] Could not create pool\n", stderr);
    return EXIT_FAILURE;
  }
  printf("[PASS] Created pool: capacity=%zu\n", fmpool_capacity(Point_t, Pool));
  
  /* grab some pointers to allocated memory from the pool */
  Point_t* PointArray[NUM_POINTS] = {NULL};
  for(size_t i = 0; i < NUM_POINTS; i++)
  {
    Point_t* Point = fmpool_get(Point_t, Pool);
    if(Point)
      PointArray[i] = Point;
    else
    {
      fprintf(stderr, "[FAIL] Allocation %zu failed\n", i);
      fmpool_destroy(Point_t, Pool);
      return EXIT_FAILURE;
    }
  }
  printf("[PASS] Allocated %d objects\n", NUM_POINTS);
  /* release some points back to the pool */
  for(size_t i = 0; i < NUM_POINTS - 5; i++)
  {
    if(!fmpool_free(Point_t, PointArray[i], Pool))
    {
      fprintf(stderr, "[FAIL] Return %zu failed\n", i);
      fmpool_destroy(Point_t, Pool);
      return EXIT_FAILURE;
    }
  }
  printf("[PASS] Returned %d objects; 5 remain allocated\n", NUM_POINTS - 5);

  /* destroy pool when done - releases memory to OS */
  fmpool_destroy(Point_t, Pool);
  puts("[PASS] Destroyed pool and released all blocks");
  puts("[SUMMARY] example: 4 steps completed");
  return 0;
}
