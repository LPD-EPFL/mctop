/* adapted from https://github.com/swenson/sort */

#define SORT_TYPE int


#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

static inline void
mmerge(SORT_TYPE* inl, SORT_TYPE* inr, const size_t len,
       SORT_TYPE* out)
{
  size_t l = 0, r = 0, o = 0;
  while (l < len && r < len)
    {
      if (inl[l] < inr[r])
	{
	  out[o] = inl[l++];
	}
      else
	{
	  out[o] = inr[r++];
	}
      o++;
    }

  while (l < len)
    {
      out[o++] = inl[l++];
    }
  while (r < len)
    {
      out[o++] = inr[r++];
    }
}

void
mmergesort(SORT_TYPE* dst, const size_t size)
{
  const size_t small_array = 1;

  SORT_TYPE* arrays[2];
  arrays[0] = dst;
  int ret = posix_memalign((void**) &arrays[1], 64, size * sizeof(SORT_TYPE));
  assert(!ret && arrays[1] != NULL);

  //  step with "small" sort
  size_t c;
  for (c = 0; c < size; c += small_array)
    {
      mbininssort(dst + c, small_array);
      //in_register_sort((__m128*) (dst + c));
    }

  mbininssort(dst + c, size - c);

  // merging!

  uint input_array = 0;
  size_t sorted_size = small_array;
  do
    {
      const uint output_array = !input_array;
      for (size_t c = 0; c < size; c += (2 * sorted_size))
	{
	  SORT_TYPE* in = arrays[input_array] + c;
	  mmerge(in, in + sorted_size, sorted_size,
		 arrays[output_array] + c);
	}
      
      input_array = output_array;
      sorted_size <<= 1;
    }
  while (sorted_size < size);

  input_array = !input_array;
  if (input_array == 0)
    {
      memcpy(dst, arrays[1], size * sizeof(SORT_TYPE));
    }


  free(arrays[1]);
}



void
mmergesort_2(SORT_TYPE* src, SORT_TYPE* dst, const size_t size, uint* in)
{
  const uint64_t middle = size / 2;
  uint64_t out = 0;
  uint64_t i = 0;
  uint64_t j = middle;

  if (size < 16)
    {
      mbininssort(src, size);
      return;
    }

  mmergesort_2(dst, src, middle, in);
  mmergesort_2(&dst[middle], &src[middle], size - middle, in);

  *in = !(*in);
  while (out != size)
    {
      if (i < middle)
	{
	  if (j < size)
	    {
	      if (src[i] <= src[j])
		{
		  dst[out] = src[i++];
		}
	      else
		{
		  dst[out] = src[j++];
		}
	    }
	  else
	    {
	      dst[out] = src[i++];
	    }
	}
      else
	{
	  dst[out] = src[j++];
	}

      out++;
    }
}


//------------------------------------------------------------
//not completely correct yet
//------------------------------------------------------------
void
mmergesort2(SORT_TYPE* src, const size_t size)
{
  SORT_TYPE* help;
  int ret = posix_memalign((void**) &help, 64, size * sizeof(SORT_TYPE));
  assert(!ret && help != NULL);
  
  uint in = 1;
  mmergesort_2(src, help, size, &in);

  //printf("in  %d\n", in);
  if (in)
    {
      memcpy(src, help, size * sizeof(SORT_TYPE));
    }

  free(help);
}
