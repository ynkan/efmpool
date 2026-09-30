#ifndef FMPOOL_CONSUMER_SHARED_H
#define FMPOOL_CONSUMER_SHARED_H

#include "fmpool.h"

typedef struct Point_s
{
    long double x;
    char tag;
} Point_t;

FMPOOL_INIT(Point_t)

Point_t* allocate_point(fmpool_t(Point_t)* Pool, long double value);

#endif
