#include <barrier.h>

barrier2_t*
barrier2_create()
{
  barrier2_t* b = (barrier2_t*) malloc(sizeof(barrier2_t));
  assert(b);
  for (int i = 0; i < BARRIER2_NUM_BARRIER; i++)
    {
      b->val[i] = 0;
      b->eval[i] = 0;
    }
  return b;
}

inline void
barrier2_cross(barrier2_t* b, const int tid, const size_t round)
{
  COMPILER_BARRIER();
  MFENCE();
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
      MFENCE();
    }
  MFENCE();
  COMPILER_BARRIER();
}

inline void
barrier2_cross_explicit(barrier2_t* b, const int tid, const size_t barrier_num)
{
  MFENCE();
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
      MFENCE();
    }
  MFENCE();
  COMPILER_BARRIER();
}
