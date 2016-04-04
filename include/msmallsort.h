#ifndef __H_MSMALLSORT__
#define __H_MSMALLSORT__

/* Function used to do a binary search for binary insertion sort */
static __inline int
mbininssort_find(SORT_TYPE* dst, const SORT_TYPE x, const size_t size)
{
  int l = 0;
  int r = size - 1;
  int c = r >> 1;

  /* check for out of bounds at the beginning. */
  if (x < dst[0])
    {
      return 0;
    }
  else if (x > dst[r])
    {
      return r;
    }

  while (1)
    {
      if (x < dst[c])
	{
	  if (c - l <= 1)
	    {
	      return c;
	    }

	  r = c;
	}
      else
	{ /* allow = for stability. The binary search favors the right. */
	  if (unlikely(r - c <= 1))
	    {
	      return c + 1;
	    }

	  l = c;
	}

      c = l + ((r - l) >> 1);
    }
}

/* Binary insertion sort, but knowing that the first "start" entries are sorted.  Used in timsort. */
static void
mbininssort_start(SORT_TYPE* dst, const size_t start, const size_t size)
{
  for (uint i = start; i < size; i++)
    {
      /* If this entry is already correct, just move along */
      if (unlikely((dst[i - 1] <= dst[i])))
	{
	  continue;
	}

      /* Else we need to find the right place, shift everything over, and squeeze in */
      const SORT_TYPE x = dst[i];
      const int location = mbininssort_find(dst, x, i);

      for (int j = i - 1; j >= location; j--)
	{
	  dst[j + 1] = dst[j];
	}

      dst[location] = x;
    }
}

/* Binary insertion sort */
void
mbininssort(SORT_TYPE* dst, const size_t size)
{
  /* don't bother sorting an array of size 0 */
  if (unlikely(size == 0))
    {
      return;
    }

  mbininssort_start(dst, 1, size);
}


static inline void
in_register_sort(__m128* data)
{

  __m128 i1 = _mm_min_ps(data[0],data[1]);
  __m128 i2 = _mm_max_ps(data[0],data[1]);
  __m128 i3 = _mm_min_ps(data[2],data[3]);
  __m128 i4 = _mm_max_ps(data[2],data[3]);

  __m128 res1 = _mm_min_ps(i1,i3);
  __m128 i22 = _mm_max_ps(i1,i3);
  __m128 i23 = _mm_min_ps(i2,i4);
  __m128 res4 = _mm_max_ps(i2,i4);

  __m128 res2 = _mm_min_ps(i22,i23);
  __m128 res3 = _mm_max_ps(i22,i23);

  __m128 l11 = _mm_shuffle_ps(res1,res2,_MM_SHUFFLE(1,0,1,0));
  __m128 l12 = _mm_shuffle_ps(res1,res2,_MM_SHUFFLE(3,2,3,2));
  __m128 l13 = _mm_shuffle_ps(res3,res4,_MM_SHUFFLE(1,0,1,0));
  __m128 l14 = _mm_shuffle_ps(res3,res4,_MM_SHUFFLE(3,2,3,2));

  data[0] = _mm_shuffle_ps(l11,l13,_MM_SHUFFLE(2,0,2,0));
  data[1] = _mm_shuffle_ps(l11,l13,_MM_SHUFFLE(3,1,3,1));
  data[2] = _mm_shuffle_ps(l12,l14,_MM_SHUFFLE(2,0,2,0));
  data[3] = _mm_shuffle_ps(l12,l14,_MM_SHUFFLE(3,1,3,1));
}

void
minssort(SORT_TYPE* dst, const size_t size)
{
  for (int i = 0; i < size; i++)
    {
      SORT_TYPE min = dst[i];
      for (int j = i + 1; j < size; j++)
	{
	  if (dst[j] < min)
	    {
	      min = dst[j];
	    }
	}
      dst[i] = min;
    }
}

#endif
