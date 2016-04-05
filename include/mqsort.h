#ifndef __H_MQSORT__
#define __H_MQSORT__

/* adapted from https://github.com/swenson/sort */

#include <nmmintrin.h>

#define MQSORT_ITERATIVE 1 	/* 0 for recursive */

#define SORT_TYPE int

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define SORT_SWAP(x,y) {SORT_TYPE __SORT_SWAP_t = (x); (x) = (y); (y) = __SORT_SWAP_t;}

#include <msmallsort.h>

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
      all_same &= (dst[i] == value);

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

__attribute__((unused)) static void
mqsort_recursive(SORT_TYPE* dst, const int left, const int right)
{
  if (unlikely(right <= left))
    {
      return;
    }

  const uint n = right - left + 1;
  if (n < 16)
    {
      /* mbininssort(&dst[left], n); */
      minssort(&dst[left], n);
      return;
    }

  const int pivot = left + (n >> 1);
  const int new_pivot = mqsort_partition(dst, left, right, pivot);

  /* check for partition all equal */
  if (new_pivot < 0)
    {
      return;
    }

  mqsort_recursive(dst, left, new_pivot - 1);
  mqsort_recursive(dst, new_pivot + 1, right);
}


/* This function is same in both iterative and recursive*/
int
mqsort_partition1(SORT_TYPE* arr, const int l, const int h)
{
  SORT_TYPE x = arr[h];
  int i = (l - 1);

  for (int j = l; j <= h - 1; j++)
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

typedef struct stack_e
{
  int low, high;
} stack_e_t;

static inline void
stack_push(stack_e_t* stack, const int top, const int low, const int high)
{
  stack_e_t* st = stack + top;
  st->low = low;
  st->high = high;
}

#define STACK_PUSH(stack, top, low, high)	\
  stack[++top].low = low; stack[top].high = high

static inline stack_e_t
stack_pop(stack_e_t* stack, const int top)
{
  return stack[top];
}

#define STACK_POP(stack, top)			\
  stack[top--]


// adapted from http://www.geeksforgeeks.org/iterative-quick-sort/

void
mqsort_iter(SORT_TYPE* arr, const size_t low, const size_t high)
{
  // Create an auxiliary stack
  const size_t stack_size = ((high - low) >> 1) + 1;
  stack_e_t* stack;
  int ret = posix_memalign((void**) &stack, 64, stack_size * sizeof(stack_e_t));
  assert(!ret && stack != NULL);

  register int top = -1;   // initialize top of stack

  // push initial values of l and h to stack
  //  top++;
  stack_push(stack, ++top, low, high);

  // Keep popping from stack while is not empty
  while (likely(top >= 0))
    {
      register stack_e_t se = stack_pop(stack, top--);

      const uint size = se.high - se.low + 1;
      if (size < 32)
	{
	  minssort(arr + se.low, size);
	  continue;
	}

      const int p = mqsort_partition1(arr, se.low, se.high);
      if ((p - 1) > se.low)
	{
	  stack_push(stack, ++top, se.low, p - 1);
	}
      if ((p + 1) < se.high)
	{
	  stack_push(stack, ++top, p + 1, se.high);
	}
    }

  free(stack);
}


void
mqsort_iter1(SORT_TYPE* arr, const size_t low, const size_t high)
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
      const int p = mqsort_partition1(arr, l, h);

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

#if MQSORT_ITERATIVE == 1
  mqsort_iter(dst, 0, size - 1);
  //  mqsort_iter1(dst, 0, size - 1);
#else
  mqsort_recursive(dst, 0, size - 1);
#endif
}

#endif
