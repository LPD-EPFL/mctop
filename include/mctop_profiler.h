#ifndef __H_MCTOP_PROFILER
#define __H_MCTOP_PROFILER

#include <inttypes.h>
#include <float.h>
#include <assert.h>
#include <stdlib.h>

#include <helper.h>

#define MCTOP_PROF_ON 1

#if !defined(PREFETCHW)
#  if defined(__x86_64__) | defined(__i386__)
#    define PREFETCHW(x) __asm volatile("prefetchw %0" :: "m" (*(unsigned long *)x)) /* write */
#  elif defined(__sparc__)
#    define PREFETCHW(x) __builtin_prefetch((const void*) x, 1, 3)
#  elif defined(__tile__)
#    define PREFETCHW(x) tmc_mem_prefetch (x, 64)
#  else
#    warning "You need to define PREFETCHW(x) for your architecture"
#  endif
#endif

typedef struct mctop_prof
{
  size_t size;
  ticks correction;
  uint8_t padding[CACHE_LINE_SIZE - sizeof(ticks) - sizeof(size_t)];
  ticks latencies[0];
} mctop_prof_t;

typedef struct mctop_prof_stats
{
  size_t num_vals;
  uint64_t median;
  uint64_t avg;
  double std_dev;
  double std_dev_perc;
} mctop_prof_stats_t;

#if MCTOP_PROF_ON == 0
#else  /* DO_TIMINGS */

#  define MCTOP_PROF_START(prof, start)		\
  COMPILER_BARRIER();				\
  ticks start = getticks();			\
  COMPILER_BARRIER();

#  define MCTOP_PROF_STOP(prof, start, rep)		\
  COMPILER_BARRIER();					\
  ticks __mctop_prof_stop = getticks();			\
  (prof)->latencies[rep] =				\
    __mctop_prof_stop - start - (prof)->correction;	\
  COMPILER_BARRIER();

#endif /* MCTOP_PROF_ON */

mctop_prof_t* mctop_prof_create(size_t num_entries);
void mctop_prof_free(mctop_prof_t* prof);
void mctop_prof_stats_calc(mctop_prof_t* prof, mctop_prof_stats_t* stats);
void mctop_prof_stats_print(mctop_prof_stats_t* stats);


#endif	/* __H_MCTOP_PROFILER */
