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
#define COMPILER_BARRIER() __asm volatile ("" ::: "memory")

#define CACHE_LINE_SIZE 64

extern int set_cpu(int cpu);
extern int get_num_hw_ctx();
struct timespec timespec_diff(struct timespec start, struct timespec end);
uint64_t spin_time(size_t n);
int dvfs_scale_up(const size_t n_reps, const double ratio, double* dur);


typedef uint64_t ticks;

#if defined(__i386__)
static inline ticks
getticks(void)
{
  ticks ret;

  __asm__ __volatile__("rdtsc" : "=A" (ret));
  return ret;
}
#elif defined(__x86_64__)
static inline ticks
getticks(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#elif defined(__sparc__)
static inline ticks
getticks()
{
  ticks ret = 0;
  __asm__ __volatile__ ("rd %%tick, %0" : "=r" (ret) : "0" (ret));
  return ret;
}
#elif defined(__tile__)
#include <arch/cycle.h>
static inline ticks getticks()
{
  return get_cycle_count();
}
#endif

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

/// Round up to next higher power of 2 (return x if it's already a power
/// of 2) for 32-bit numbers
static inline uint32_t
pow2roundup (uint32_t x)
{
  if (x == 0)
    {
      return 1;
    }
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x + 1;
}

uint64_t ll_random_traverse(volatile uint64_t* list, const size_t reps);
void ll_random_create(volatile uint64_t* mem, const size_t size);


#endif	/* _HELPER_H_ */
