/*
  The MIT License (MIT)
  Copyright © 2015-2023 David Newman <davidjndev@gmail.com>

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

#ifndef _FMPOOL_H_INCLUDED_
#define _FMPOOL_H_INCLUDED_

#include <stddef.h> /* size_t */
#include <stdlib.h> /* calloc, free */
#include <stdbool.h> /* bool */
#include <stdint.h> /* uintptr_t, SIZE_MAX */

/* These options configure fmpool.c, not the layout of public types. */
#ifndef FMPOOL_THREAD_SAFE
#define FMPOOL_THREAD_SAFE 0
#endif
#ifndef FMPOOL_CHECKS
#define FMPOOL_CHECKS 1
#endif

#if defined(_WIN32) && defined(FMPOOL_SHARED)
#ifdef FMPOOL_BUILDING_LIBRARY
#define FMPOOL_API __declspec(dllexport)
#else
#define FMPOOL_API __declspec(dllimport)
#endif
#else
#define FMPOOL_API
#endif

#if defined(__GNUC__) || defined(__clang__)
#define FMPOOL_INLINE inline __attribute__((always_inline)) /* TODO: ? */
#else
#define FMPOOL_INLINE inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fmpool_pool_s fmpool_pool_t;

/* Generic library API. A zero growth selects fixed capacity. */
FMPOOL_API fmpool_pool_t* fmpool_pool_create(size_t object_size,
        size_t num, size_t growth);
FMPOOL_API void fmpool_pool_destroy(fmpool_pool_t* P);
FMPOOL_API void* fmpool_pool_get(fmpool_pool_t* P);
FMPOOL_API bool fmpool_pool_free(void* OBJ, fmpool_pool_t* P);
FMPOOL_API size_t fmpool_pool_index(fmpool_pool_t* P, const void* OBJ);
FMPOOL_API void* fmpool_pool_at(fmpool_pool_t* P, size_t index);
FMPOOL_API size_t fmpool_pool_capacity(fmpool_pool_t* P);
/* Query the linked library, including for consumers built without CMake. */
FMPOOL_API bool fmpool_is_thread_safe(void);
FMPOOL_API bool fmpool_checks_enabled(void);

#ifdef __cplusplus
}
#endif

/*
 * Growth appends storage: live object addresses, indices and contents never change.
 * Indices identify slots, not generations; free ends an object's logical lifetime.
 * at/index do not allocate or transfer ownership. Only access owned live objects.
 * Thread safety covers pool operations, not object data or concurrent destruction.
 */
#define FMPOOL_INIT(TYPE)\
    typedef struct fmpool_##TYPE##_s fmpool_##TYPE##_t; \
    static FMPOOL_INLINE fmpool_##TYPE##_t* fmpool_##TYPE##_create_ex(const size_t num, const size_t growth) \
    { \
        return (fmpool_##TYPE##_t*)fmpool_pool_create(sizeof(TYPE), num, growth); \
    } \
    static FMPOOL_INLINE fmpool_##TYPE##_t* \
    fmpool_##TYPE##_create(const size_t num) \
    { \
        return fmpool_##TYPE##_create_ex(num, num); \
    } \
    static FMPOOL_INLINE void fmpool_##TYPE##_destroy(fmpool_##TYPE##_t* P) \
    { \
        fmpool_pool_destroy((fmpool_pool_t*)P); \
    } \
    static FMPOOL_INLINE TYPE* fmpool_##TYPE##_get(fmpool_##TYPE##_t* P) \
    { \
        return (TYPE*)fmpool_pool_get((fmpool_pool_t*)P); \
    } \
    static FMPOOL_INLINE bool fmpool_##TYPE##_free(TYPE* OBJ, \
            fmpool_##TYPE##_t* P) \
    { \
        return fmpool_pool_free(OBJ, (fmpool_pool_t*)P); \
    } \
    static FMPOOL_INLINE size_t fmpool_##TYPE##_index(fmpool_##TYPE##_t* P, const TYPE* OBJ) \
    { \
        return fmpool_pool_index((fmpool_pool_t*)P, OBJ); \
    } \
    static FMPOOL_INLINE TYPE* fmpool_##TYPE##_at(fmpool_##TYPE##_t* P, size_t index) \
    { \
        return (TYPE*)fmpool_pool_at((fmpool_pool_t*)P, index); \
    } \
    static FMPOOL_INLINE size_t fmpool_##TYPE##_capacity(fmpool_##TYPE##_t* P) \
    { \
        return fmpool_pool_capacity((fmpool_pool_t*)P); \
    }

#define fmpool_t(TYPE) fmpool_##TYPE##_t
#define fmpool_create(TYPE, NUM) fmpool_##TYPE##_create(NUM)
#define fmpool_create_ex(TYPE, NUM, GROWTH) fmpool_##TYPE##_create_ex(NUM, GROWTH)
#define fmpool_index(TYPE, POOL, OBJ) fmpool_##TYPE##_index(POOL, OBJ)
#define fmpool_at(TYPE, POOL, INDEX) fmpool_##TYPE##_at(POOL, INDEX)
#define fmpool_destroy(TYPE, POOL) fmpool_##TYPE##_destroy(POOL)
#define fmpool_get(TYPE, POOL) fmpool_##TYPE##_get(POOL)
#define fmpool_free(TYPE, OBJ, POOL) fmpool_##TYPE##_free(OBJ, POOL)
#define fmpool_capacity(TYPE, POOL) fmpool_##TYPE##_capacity(POOL)

#endif // _FMPOOL_H_INCLUDED_

