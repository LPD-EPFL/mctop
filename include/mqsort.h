/* adapted from https://github.com/swenson/sort */

#include <nmmintrin.h>

#define SORT_TYPE int

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define SORT_SWAP(x,y) {SORT_TYPE __SORT_SWAP_t = (x); (x) = (y); (y) = __SORT_SWAP_t;}
/* #define SORT_CMP(x, y)  ((x) < (y) ? -1 : ((x) == (y) ? 0 : 1)) */
#define SORT_CMP(x, y)  ((x) - (y))


#include <mbininssort.h>

static __inline int
mqsort_partition(SORT_TYPE* dst, const int left, const int right, const int pivot)
{
  const SORT_TYPE value = dst[pivot];
  int index = left;
  int all_same = 1;
  /* move the pivot to the right */
  SORT_SWAP(dst[pivot], dst[right]);

  for (int i = left; i < right; i++)
    {
      /* int cmp = SORT_CMP(dst[i], value); */

      /* check if everything is all the same */
      /* if (cmp != 0) */
      /* 	{ */
	  //      	  all_same &= 0;
      all_same &= (dst[i] == value);
      	/* } */

      if (dst[i] < value)
	{
	  SORT_SWAP(dst[i], dst[index]);
	  index++;
	}
    }

  SORT_SWAP(dst[right], dst[index]);

  /* avoid degenerate case */
  if (unlikely(all_same))
    {
      return -1;
    }

  return index;
}

static __inline uint
is_aligned_16(uintptr_t addr)
{
  const uintptr_t mask = 0xE;
  return (addr & mask) == 0;
}

static void
mqsort_recursive(SORT_TYPE* dst, const int left, const int right)
{
  if (unlikely(right <= left))
    {
      return;
    }

  const uint n = right - left + 1;
  /* if (n == 16 && is_aligned_16((uintptr_t) &dst[left])) */
  /*   { */
  /*     in_register_sort((__m128*) &dst[left]); */
  /*     return; */
  /*   } */
  /* else */
  if (n <= 16)
    {
      mbininssort(&dst[left], n);
      return;
    }

  const int pivot = left + ((right - left) >> 1);
  const int new_pivot = mqsort_partition(dst, left, right, pivot);

  /* check for partition all equal */
  if (new_pivot < 0)
    {
      return;
    }

  mqsort_recursive(dst, left, new_pivot - 1);
  mqsort_recursive(dst, new_pivot + 1, right);
}

void
mqsort(SORT_TYPE* dst, const size_t size)
{
  /* don't bother sorting an array of size 0 */
  if (size == 0)
    {
      return;
    }

  mqsort_recursive(dst, 0, size - 1);
}
