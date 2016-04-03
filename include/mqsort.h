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
mqsort1(SORT_TYPE* dst, const size_t size)
{
  /* don't bother sorting an array of size 0 */
  if (size == 0)
    {
      return;
    }

  mqsort_recursive(dst, 0, size - 1);
}



/* This function is same in both iterative and recursive*/
int
partition (SORT_TYPE* arr, const int l, const int h)
{
  SORT_TYPE x = arr[h];
  int i = (l - 1);

  for (int j = l; j <= h- 1; j++)
    {
      if (arr[j] <= x)
	{
	  i++;
	  SORT_SWAP(arr[i], arr[j]);
	}
    }
  SORT_SWAP(arr[i + 1], arr[h]);

  return (i + 1);
}
 

// adapted from http://www.geeksforgeeks.org/iterative-quick-sort/
void
mqsort_iter(SORT_TYPE* arr, const size_t low, const size_t high)
{
  register int l = low, h = high;

  // Create an auxiliary stack
  const size_t stack_size = h - l + 1;
  SORT_TYPE* stack;
  int ret = posix_memalign((void**) &stack, 64, stack_size * sizeof(SORT_TYPE));
  assert(!ret && stack != NULL);


  // initialize top of stack
  int top = -1;

  // push initial values of l and h to stack
  stack[++top] = l;
  stack[++top] = h;

  // Keep popping from stack while is not empty
  while (likely(top >= 0))
    {
      // Pop h and l
      h = stack[top--];
      l = stack[top--];

      // Set pivot element at its correct position
      // in sorted array
      const int p = partition(arr, l, h);

      // If there are elements on left side of pivot,
      // then push left side to stack
      if ((p - 1) > l)
	{
	  stack[++top] = l;
	  stack[++top] = p - 1;
	}

      // If there are elements on right side of pivot,
      // then push right side to stack
      if ((p + 1) < h)
	{
	  stack[++top] = p + 1;
	  stack[++top] = h;
	}
    }

  free(stack);
}

void
mqsort(SORT_TYPE* dst, const size_t size)
{
  /* don't bother sorting an array of size 0 */
  if (size == 0)
    {
      return;
    }

  mqsort_iter(dst, 0, size - 1);
}
