#include "shared.h"

Point_t* allocate_point(fmpool_t(Point_t)* Pool, long double value)
{
    Point_t* Point = fmpool_get(Point_t, Pool);
    if(Point)
    {
        Point->x = value;
        Point->tag = 'p';
    }
    return Point;
}
