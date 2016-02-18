#ifndef __H_BARRIER__
#define __H_BARRIER__

#include <helper.h>
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


barrier2_t* barrier2_create();
void barrier2_cross(barrier2_t* b, const int tid, const size_t round);
void barrier2_cross_explicit(barrier2_t* b, const int tid, const size_t barrier_num);



#endif /* __H_BARRIER__ */


