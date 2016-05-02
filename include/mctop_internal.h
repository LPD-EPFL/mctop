#ifndef __H_MCTOP_INTERNAL__
#define __H_MCTOP_INTERNAL__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>

#include <atomics.h>

#include <barrier.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MCTOP_DEBUG 0

#if MCTOP_DEBUG == 1
static inline void
mctop_dprint(const char* format, ...)
{
va_list args;
va_start(args, format);
vfprintf(stdout, format, args);
va_end(args);
fflush(stdout);
}
#else
static inline void
mctop_dprint(const char* format, ...)
{

}
#endif

typedef struct tld
{
int id;
uint n_threads;
int hw_context;
barrier2_t* barrier2;
pthread_barrier_t* barrier;
mctop_t* topo;
volatile int* run;
} tld_t;

/* ******************************************************************************** */
/* Printing helpers */
/* ******************************************************************************** */

#define P5DOUBLE "%-10.2f %-10.2f %-10.2f %-10.2f %-10.2f "
#define P5DOUBLEP "%-10f %-10f %-10f %-10f %-10f "
#define G5DOUBLE(a) a[0], a[1], a[2], a[3], a[4]

/* ******************************************************************************** */
/* MCTOP Cache */
/* ******************************************************************************** */

mctop_cache_info_t* mctop_cache_size_estimate();
void mctop_cache_info_free(mctop_cache_info_t* mci);
mctop_cache_info_t* mctop_cache_info_create(const uint n_levels);


/* ******************************************************************************** */
/* AUX functions */
/* ******************************************************************************** */

static inline void
__copy_doubles(double* to, double* from, const uint n, const uint div)
{
  for (uint i = 0; i < n; i++)
    {
      to[i] = from[i] / div;
    }
}


void** table_malloc(const size_t rows, const size_t cols, const size_t elem_size);
void** table_calloc(const size_t rows, const size_t cols, const size_t elem_size);
void table_free(void** m, const size_t cols);

static inline void*
malloc_assert(size_t size)
{
void* m = malloc(size);
assert(m != NULL);
return m;
}

static inline void*
realloc_assert(void* old, size_t size)
{
void* m = realloc(old, size);
assert(m != NULL);
return m;
}

static inline void*
calloc_assert(size_t n, size_t size)
{
void* m = calloc(n, size);
assert(m != NULL);
return m;
}

extern int mctop_set_cpu(mctop_t* topo, int cpu);


#ifdef __cplusplus
}
#endif

#endif	/* __H_MCTOP_INTERNAL__ */

