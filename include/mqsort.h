/* adapted from https://github.com/swenson/sort */

#define SORT_TYPE int

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define SORT_SWAP(x,y) {SORT_TYPE __SORT_SWAP_t = (x); (x) = (y); (y) = __SORT_SWAP_t;}
/* #define SORT_CMP(x, y)  ((x) < (y) ? -1 : ((x) == (y) ? 0 : 1)) */
#define SORT_CMP(x, y)  ((x) - (y))


/* Function used to do a binary search for binary insertion sort */
static __inline int
mbininssort_find(SORT_TYPE* dst, const SORT_TYPE x, const size_t size)
{
  int l, c, r;
  SORT_TYPE cx;
  l = 0;
  r = size - 1;
  c = r >> 1;

  /* check for out of bounds at the beginning. */
  if (SORT_CMP(x, dst[0]) < 0)
    {
      return 0;
    }
  else if (SORT_CMP(x, dst[r]) > 0)
    {
      return r;
    }

  cx = dst[c];

  while (1)
    {
      const int val = SORT_CMP(x, cx);
      if (val < 0)
	{
	  if (c - l <= 1)
	    {
	      return c;
	    }

	  r = c;
	}
      else
	{ /* allow = for stability. The binary search favors the right. */
	  if (r - c <= 1)
	    {
	      return c + 1;
	    }

	  l = c;
	}

      c = l + ((r - l) >> 1);
      cx = dst[c];
    }
}

/* Binary insertion sort, but knowing that the first "start" entries are sorted.  Used in timsort. */
static void
mbininssort_start(SORT_TYPE* dst, const size_t start, const size_t size)
{
  uint i;

  for (i = start; i < size; i++)
    {
      int j;
      SORT_TYPE x;
      int location;

      /* If this entry is already correct, just move along */
      if (SORT_CMP(dst[i - 1], dst[i]) <= 0)
	{
	  continue;
	}

      /* Else we need to find the right place, shift everything over, and squeeze in */
      x = dst[i];
      location = mbininssort_find(dst, x, i);

      for (j = i - 1; j >= location; j--)
	{
	  dst[j + 1] = dst[j];
	}

      dst[location] = x;
    }
}

/* Binary insertion sort */
void
mbininssort_SORT(SORT_TYPE* dst, const size_t size)
{
  /* don't bother sorting an array of size 0 */
  if (size == 0)
    {
      return;
    }

  mbininssort_start(dst, 1, size);
}

static __inline int
mqsort_partition(SORT_TYPE* dst, const int left, const int right, const int pivot)
{
  SORT_TYPE value = dst[pivot];
  int index = left;
  int i;
  int all_same = 1;
  /* move the pivot to the right */
  SORT_SWAP(dst[pivot], dst[right]);

  for (i = left; i < right; i++)
    {
      int cmp = SORT_CMP(dst[i], value);

      /* check if everything is all the same */
      if (cmp != 0)
	{
	  all_same &= 0;
	}

      if (cmp < 0)
	{
	  SORT_SWAP(dst[i], dst[index]);
	  index++;
	}
    }

  SORT_SWAP(dst[right], dst[index]);

  /* avoid degenerate case */
  if (all_same)
    {
      return -1;
    }

  return index;
}

/* Return the median index of the objects at the three indices. */
static __inline int
mqsort_median(const SORT_TYPE* dst, const int a, const int b, const int c)
{
  const int AB = SORT_CMP(dst[a], dst[b]) < 0;

  if (AB)
    {
      /* a < b */
      const int BC = SORT_CMP(dst[b], dst[c]) < 0;

      if (BC)
	{
	  /* a < b < c */
	  return b;
	} else {
	/* a < b, c < b */
	const int AC = SORT_CMP(dst[a], dst[c]) < 0;

	if (AC)
	  {
	    /* a < c < b */
	    return c;
	  } else
	  {
	    /* c < a < b */
	    return a;
	  }
      }
    }
  else
    {
      /* b < a */
      const int AC = SORT_CMP(dst[a], dst[b]) < 0;

      if (AC)
	{
	  /* b < a < c */
	  return a;
	}
      else
	{
	  /* b < a, c < a */
	  const int BC = SORT_CMP(dst[b], dst[c]) < 0;

	  if (BC)
	    {
	      /* b < c < a */
	      return c;
	    }
	  else
	    {
	      /* c < b < a */
	      return b;
	    }
	}
    }
}

static void
mqsort_recursive(SORT_TYPE* dst, const int left, const int right)
{
  if (right <= left)
    {
      return;
    }

  if ((right - left + 1) < 16)
    {
      mbininssort_SORT(&dst[left], right - left + 1);
      return;
    }

  const int pivot = left + ((right - left) >> 1);
  /* pivot = mqsort_median(dst, left, pivot, right); */
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
