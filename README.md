fmpool - Enhanced Free-List Memory Pool
-------
This is a C99 library that provides efficient type-generic code to allocate heap memory from the OS for user-defined number of objects.  Achieved through the magic of C macros and [free lists](http://en.wikipedia.org/wiki/Free_list).

This library is built for situations where programs repeatedly call malloc/calloc/new/etc. for small data structures, followed closely by free/delete.  Pools grow by adding independent blocks when exhausted. Optional validation and thread safety allow a tradeoff between diagnostics and overhead.  Boost memory pools and others are overkill sometimes.

Features:
   * Block-based growth without moving existing objects.
   * Separate single-thread (`fmpool_st`) and thread-safe (`fmpool_mt`) libraries.
   * Configurable consistency checks (`FMPOOL_CHECKS`, default `1`).
   * Stable slot indices across growth, with pointer/index lookup.

Inspired by [computer game particles](http://gameprogrammingpatterns.com/object-pool.html).

## Try it now
```sh
make; ./bin/perftest
```

## CMake

```sh
cmake -S . -B build -DFMPOOL_THREAD_SAFE=ON -DFMPOOL_CHECKS=ON
cmake --build build
ctest --test-dir build -V
```

`ctest -V` displays successful test logs as well as failures. Each test prints
its checks and a result summary; disabled checks are marked `[SKIP]`. The
example and optional benchmark can also be run directly from their build
subdirectories (`test/example` and `test/perftest`).

Each build produces both `fmpool_st` (single-thread) and `fmpool_mt`
(thread-safe). The implementation is compiled from `fmpool.c`; `fmpool.h`
contains public declarations and lightweight typed wrappers. The original
copyright and MIT license are retained.

| CMake target | Library basename | Pool synchronization |
| --- | --- | --- |
| `fmpool::single_thread` | `fmpool_st` | None |
| `fmpool::thread_safe` | `fmpool_mt` | Mutex |

Static libraries are built by default (`.a` with GCC, `.lib` with MSVC).
Use `-DBUILD_SHARED_LIBS=ON` for shared libraries (`.dll`, `.so`, or `.dylib`
depending on the platform). Both variants expose the same symbols: link
**exactly one** into an application, and use that variant for all operations
on its pools. The choice is made when linking, not by a caller-side macro.

The compatibility target `fmpool::fmpool` selects the single-thread variant
by default; `-DFMPOOL_THREAD_SAFE=ON` makes it select the thread-safe variant.
Both libraries are built and tested regardless of this selection.
`FMPOOL_CHECKS` configures validation in both compiled libraries (default ON).
`BUILD_TESTING` and `FMPOOL_BUILD_EXAMPLES` default to ON;
`FMPOOL_BUILD_BENCHMARKS` defaults to OFF. Subdirectories own their executable
and test definitions. Thread-safe builds use Windows critical sections or
POSIX mutexes (CMake links `Threads::Threads` on POSIX).

To use the source tree:

```cmake
add_subdirectory(fmpool)
target_link_libraries(app PRIVATE fmpool::thread_safe)
# Or: target_link_libraries(app PRIVATE fmpool::single_thread)
```

To install and use the libraries from another project:

```sh
cmake --install build --prefix /path/to/fmpool-install
```

```cmake
find_package(fmpool CONFIG REQUIRED)
target_link_libraries(app PRIVATE fmpool::thread_safe)
```

Pass `-DCMAKE_PREFIX_PATH=/path/to/fmpool-install` when configuring the
consumer. Installation includes both libraries, `include/fmpool.h`, and
CMake package files. CMake propagates the appropriate configuration and DLL
import definitions. With shared libraries, deploy the selected DLL beside
the application on Windows, or configure the runtime library search path
on other systems.

For a manual GCC build of static libraries:

```sh
cc -std=c99 -O2 -DFMPOOL_THREAD_SAFE=0 -c fmpool.c -o fmpool_st.o
ar rcs libfmpool_st.a fmpool_st.o
cc -std=c99 -O2 -DFMPOOL_THREAD_SAFE=1 -pthread -c fmpool.c -o fmpool_mt.o
ar rcs libfmpool_mt.a fmpool_mt.o
cc app.c -I/path/to/include -L/path/to/lib -lfmpool_mt -pthread -o app
```

The `-pthread` flags apply to POSIX builds; omit them on Windows.
To disable checks, compile `fmpool.c` with `-DFMPOOL_CHECKS=0`. Defining
options only in the application cannot change a prebuilt library.
For manual Windows DLL builds, define `FMPOOL_SHARED` and
`FMPOOL_BUILDING_LIBRARY` when compiling the library, and only
`FMPOOL_SHARED` when compiling consumers. `fmpool_is_thread_safe()` and
`fmpool_checks_enabled()` report the actual linked library configuration.

The standalone `test/consumer` project verifies installed-library linking,
including allocation and release across translation units:

```sh
cmake -S test/consumer -B build-consumer -DCMAKE_PREFIX_PATH=/path/to/fmpool-install
cmake --build build-consumer
ctest --test-dir build-consumer -V
```

## ST/MT performance comparison

The original `perftest` compares pool and malloc workloads. The additional
`perftest_st` and `perftest_mt` compile the same `variantbench.c` and link the
single-thread and thread-safe libraries respectively.

```sh
cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release -DFMPOOL_BUILD_BENCHMARKS=ON
cmake --build build-perf --config Release --target benchmark_compare
```

Alternatively, build all targets and run
`ctest --test-dir build-perf -C Release -L performance -V`.
The summary table explicitly separates four groups: **ST / 1 thread**,
**MT / 1 thread**, **MT / 4 threads sharing one pool**, and
**ST / 4 threads with one independent pool per worker**. ST / 4 threads
sharing one pool remains N/A and is never run. CTest also exposes these as separate tests:
`fmpool.performance.st.1thread`, `fmpool.performance.mt.1thread`, and
`fmpool.performance.mt.4threads`, and
`fmpool.performance.st.4threads.independent`. `fmpool.performance.compare` runs all four
and prints the combined table. Each executable also accepts `--threads 1`
or `--threads 4`. For ST with four threads, explicitly use:

```sh
build-perf/test/perftest/perftest_st --threads 4 --independent-pools
```

This links one ST library; each worker exclusively owns its pool. It does not
load four library copies. ST rejects four threads without this explicit mode.

Each group runs warmup and seven measured rounds. **Total work is identical**
for one and four threads (the four workers divide it):

* `reuse`: 1,048,576 get/free pairs in batches of 64, with a shared fixed
  capacity of 256 for shared-pool groups, or four independent pools of 64
  slots each. Creation and destruction are outside timing.
* `growth`: 65,536 get/free pairs across 16 successive shared pools growing
  from 64 to 4,096 slots in blocks of 64. A barrier holds all 4,096 objects
  before any return, ensuring the same peak allocation and block count.
  The independent group creates four pools per batch, each growing from 64
  to 1,024 slots in blocks of 64 (4,096 total slots and 64 total blocks).
  Each worker creates and destroys its own pool. All four-thread groups use
  the same phase barriers to hold the same total live objects before return.
  Includes pool creation/destruction and phase synchronization. Independent
  pools add pool metadata and reduce per-pool validation scan length, so the
  result compares ownership strategies rather than only lock overhead.

Thread creation and joining are outside timing; start/finish synchronization
is included. Per-worker checksums are combined only after completion and
validated against the same expected object data total in all groups.
Output includes median elapsed ms, aggregate ns per get/free pair, and total
throughput in millions of pairs/second. Aggregate ns/pair is wall time divided
by the total operation count, **not individual operation latency**. The final
comparison reports MT(1)-ST(1) overhead and MT(4)/MT(1) total throughput ratio.
The table also reports ST(4 independent)/ST(1) and ST(4 independent)/MT(4 shared)
throughput ratios. Shared-pool MT workers may be slower due to mutex contention;
independent ST pools avoid that contention.

All groups use the same checks setting; repeat with `-DFMPOOL_CHECKS=OFF` to
compare without validation. Measurements are hardware/build/load-dependent
and have no fixed performance threshold. The driver runs ST(1), MT(1), MT(4 shared), and ST(4 independent)
sequentially; avoid other CPU-heavy work while measuring. Run only the compare
test if you want a single measurement of the matrix rather than also running
the individual CTest entries.

## Example
```c
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
  /* create pool - requests memory from OS */
  fmpool_t(Point_t)* Pool;
  Pool = fmpool_create(Point_t, NUM_POINTS);
  
  /* grab some pointers to allocated memory from the pool */
  Point_t* PointArray[NUM_POINTS] = {NULL};
  for(size_t i = 0; i < NUM_POINTS; i++)
  {
    Point_t* Point = fmpool_get(Point_t, Pool);
    if(Point)
      PointArray[i] = Point;
  }
  /* release some points back to the pool */
  for(size_t i = 0; i < NUM_POINTS - 5; i++)
    fmpool_free(Point_t, PointArray[i], Pool);

  /* destroy pool when done - releases memory to OS */
  fmpool_destroy(Point_t, Pool);
  return 0;
}
```

## Details
fmpool requests an initial contiguous block during `fmpool_create`. When exhausted, it adds another block of the initial capacity. `fmpool_create_ex(TYPE, INITIAL, GROWTH)` selects the block size; `GROWTH == 0` selects fixed capacity.  Afterwards, pointers to unused objects are provided with `fmpool_get`.  Objects can be released back to the pool with `fmpool_free` and subsequently re-requested.  When finished, the memory is released back to the OS with `fmpool_destroy`.

## Functions
#### fmpool_create(TYPE, NUM)
Creates a new memory pool.  The new pool initially has `NUM` slots of type `TYPE`, and grows by `NUM` slots when exhausted. `NUM == 0` is rejected. This changes the original fixed-capacity behavior; use `fmpool_create_ex(TYPE, NUM, 0)` to retain it.
>**Return Values**  
>On success: returns a pointer to the new fmpool.  
>On failure: returns a `NULL` pointer.  
  
#### fmpool_destroy(TYPE, POOL)
Frees all memory associated with `POOL`.
>**Return Values**  
>Returns no value. (mimics C99 standard).
  
#### fmpool_get(TYPE, POOL)
Grabs an unused object from the pool.
>**Return Values**  
>On success: returns a `TYPE` pointer.  
>On failure: returns a `NULL` pointer.
  
#### fmpool_free(TYPE, OBJ, POOL)
Returns an allocated object to `POOL`. Does not zero memory. With checks enabled, rejects foreign pointers, interior pointers, and duplicate returns. With checks disabled, the caller must provide a currently allocated object belonging to this pool; violations cause undefined behavior. NULL arguments are always rejected.
>**Return Values**  
>On success: returns `true`.  
>On failure: returns `false`.
  
#### fmpool_capacity(TYPE, POOL)
Returns total capacity across all blocks, or zero for a NULL pool.

#### Generic library API
`fmpool_pool_t` is an opaque pool type. Call
`fmpool_pool_create(sizeof(MyType), INITIAL, GROWTH)` to create it and use
`fmpool_pool_get`, `fmpool_pool_free(OBJ, POOL)`, `fmpool_pool_destroy`,
`fmpool_pool_index`, `fmpool_pool_at`, and `fmpool_pool_capacity` directly
without `FMPOOL_INIT`. Generic get/at return `void*`. Zero object size,
zero initial capacity and overflowing size calculations are rejected.

## Growth consistency and object lifetime

Growth always preserves the address, slot index, identity, and contents of
**every still-allocated object**, regardless of `FMPOOL_CHECKS`. Existing
objects are never copied, relocated, or reinitialized. A failed growth leaves
the pool intact and makes `get` return NULL. Blocks are released only by
`destroy`; free slots are reused before growing.

`fmpool_index(TYPE, POOL, OBJ)` returns a stable zero-based slot index, or
`SIZE_MAX` for an invalid pointer. `fmpool_at(TYPE, POOL, INDEX)` returns the
corresponding slot address, or NULL if outside the current capacity. New
blocks receive consecutive indices after existing blocks. Both lookups are
available in either checks mode and cost O(number of blocks).

```c
Point_t* p = fmpool_get(Point_t, Pool);
size_t id = fmpool_index(Point_t, Pool, p);
/* Other allocations may grow Pool. While p remains allocated: */
/* fmpool_at(Point_t, Pool, id) == p, and p's contents remain intact. */
```

Indices identify storage slots, not generation-counted object handles.
`at` does not allocate or transfer ownership; only access a slot while you
own its allocated object. Returning an object ends its logical lifetime and
may overwrite its data with a free-list link. A subsequent allocation may
reuse its address and index for a different logical object. Stale pointers
cannot detect that reuse, even with checks enabled. Destroy invalidates all
addresses and indices.

Pool structures are now opaque. Replace direct `pool->num` reads with
`fmpool_capacity(TYPE, pool)` and `items[index]` access with `fmpool_at`.
Capacity queries are synchronized by the thread-safe library. Existing
`FMPOOL_INIT`, create/get/free/destroy and pointer/index call syntax remains
available, but programs must now link a compiled library. Put `FMPOOL_INIT`
once per type in a shared header when multiple translation units use it.

Checks use one byte per slot and scan blocks during get/free (O(block count)).
Disabling checks removes this tracking allocation and makes non-growing
get/free O(1). Slot strides are rounded up to a C99 fundamental-alignment
unit (a union of long double, long long, object and function pointers), with
ordinary `calloc` alignment. This may add padding, especially for small
objects. Extended-alignment types are not supported. Growth initializes new storage, but reuse does not
clear old data.

Thread safety serializes get/free and pointer/index lookups, including
internal growth. It does not synchronize reads/writes of object contents or
application data. Callers must synchronize shared object access, and ensure
all other operations and object uses finish before destroying the pool.
The non-thread-safe configuration requires external synchronization whenever
multiple threads share a pool.

## License
[MIT License](http://dn.mit-license.org)

