/*   
 *   File: helper.h
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: helper macros
 *
 *
 */

#ifndef _HELPER_H_
#define _HELPER_H_

#include <stdio.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <malloc.h>
#include <assert.h>
#include <stdarg.h>

#define XSTR(s)                         STR(s)
#define STR(s)                          #s
#define UNUSED         __attribute__ ((unused))

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define CACHE_LINE_SIZE 64

extern int set_cpu(int cpu);
extern int get_num_hw_ctx();

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

static inline void
print_id(size_t id, const char* format, ...)
{
  va_list args;
  fprintf(stdout, "[%03zu] ", id);
  va_start(args, format);
  vfprintf(stdout, format, args);
  va_end(args);
  fflush(stdout);
}

static inline unsigned long* 
seeds_create() 
{
  unsigned long* seeds;
  seeds = (unsigned long*) memalign(CACHE_LINE_SIZE, CACHE_LINE_SIZE);
  return seeds;
}

//Marsaglia's xorshf generator
static inline unsigned long
xorshf96(unsigned long* x, unsigned long* y, unsigned long* z)  //period 2^96-1
{
  unsigned long t;
  (*x) ^= (*x) << 16;
  (*x) ^= (*x) >> 5;
  (*x) ^= (*x) << 1;

  t = *x;
  (*x) = *y;
  (*y) = *z;
  (*z) = t ^ (*x) ^ (*y);

  return *z;
}

static inline unsigned long
marsaglia_rand(unsigned long* seeds)
{
  return xorshf96(seeds, seeds + 1, seeds + 2);
}

uint64_t ll_random_traverse(volatile uint64_t* list, const size_t reps);
void ll_random_create(volatile uint64_t* mem, const size_t size);

void** table_malloc(const size_t rows, const size_t cols, const size_t elem_size);
void** table_calloc(const size_t rows, const size_t cols, const size_t elem_size);
void table_free(void** m, const size_t cols);


#endif	/* _HELPER_H_ */
