#ifndef __H_BARRIER__
#define __H_BARRIER__

#include <atomics.h>

/* ******************************************************************************** */
/* 2-thread barrier */
/* ******************************************************************************** */

#define BARRIER2_NUM_BARRIER (CACHE_LINE_SIZE / sizeof(int32_t))

typedef struct barrier2		/* 2-thread barrier */
{
  volatile int32_t val[BARRIER2_NUM_BARRIER];
  volatile int32_t eval[BARRIER2_NUM_BARRIER];
} barrier2_t;

static inline barrier2_t*
barrier2_create()
{
  barrier2_t* b = (barrier2_t*) memalign(CACHE_LINE_SIZE, sizeof(barrier2_t));
  assert(b);
  for (int i = 0; i < BARRIER2_NUM_BARRIER; i++)
    {
      b->val[i] = 0;
      b->eval[i] = 0;
    }
  return b;
}

static inline void
barrier2_cross(barrier2_t* b, const int tid, const size_t round)
{
  COMPILER_BARRIER();
  asm volatile("mfence");
  int vn = round & (BARRIER2_NUM_BARRIER - 1);
  if (tid == 0)
    {
      DAF_U32(&b->val[vn]);
    }
  else				/* tid == 1 */
    {
      IAF_U32(&b->val[vn]);
    }

  COMPILER_BARRIER();
  while (b->val[vn] != 0)
    {
      PAUSE();
      asm volatile ("mfence");
    }
  asm volatile("mfence");
  COMPILER_BARRIER();
}

static inline void
barrier2_cross_explicit(barrier2_t* b, const int tid, const size_t barrier_num)
{
  asm volatile("mfence");
  int vn = barrier_num;
  if (tid == 0)
    {
      DAF_U32(&b->eval[vn]);
    }
  else				/* tid == 1 */
    {
      IAF_U32(&b->eval[vn]);
    }

  COMPILER_BARRIER();
  while (b->eval[vn] != 0)
    {
      PAUSE();
      asm volatile ("mfence");
    }
  asm volatile("mfence");
  COMPILER_BARRIER();
}


#endif /* __H_BARRIER__ */


