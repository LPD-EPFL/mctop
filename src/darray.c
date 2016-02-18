#include <darray.h>

darray_t*
darray_create()
{
  darray_t* da = malloc_assert(sizeof(darray_t));
  da->n_elems = 0;
  da->size = DARRAY_MIN_SIZE;
  da->array = malloc_assert(DARRAY_MIN_SIZE * sizeof(size_t));
  return da;
}

void
darray_free(darray_t* da)
{
  free(da->array);
  free(da);
  da = NULL;
}

inline void
darray_empty(darray_t* da)
{
  da->n_elems = 0;
} 

void
darray_add(darray_t* da, size_t elem)
{
  if (unlikely(da->n_elems == da->size))
    {
      size_t size_new = DARRAY_GROW_MUL * da->size;
      da->array = realloc_assert(da->array, size_new * sizeof(size_t));
      da->size = size_new;
    }
  da->array[da->n_elems++] = elem;
}

inline int			
darray_exists(darray_t* da, size_t elem)
{
  for (int i = 0; i < da->n_elems; i++)
    {
      if (unlikely(elem == da->array[i]))
	{
	  return 1;
	}
    }
  return 0;
}



void
darray_iter_init(darray_iter_t* dai, darray_t* da)
{
  dai->darray = da;
  dai->curr = 0;
}

inline int			/* error or OK */
darray_iter_next(darray_iter_t* dai, size_t* elem)
{
  if (dai->curr >= dai->darray->n_elems)
    {
      return 0;
    }
  *elem = dai->darray->array[dai->curr++];
  return 1;
}


void
darray_print(darray_t* da)
{
  for (int i = 0; i < da->n_elems; i++)
    {
      printf("%-3zu ", da->array[i]);
    }
  printf("\n");
}
