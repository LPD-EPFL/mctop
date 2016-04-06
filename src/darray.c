#include <mctop.h>
#include <mctop_internal.h>
#include <darray.h>

darray_t*
darray_create()
{
  darray_t* da = malloc_assert(sizeof(darray_t));
  da->n_elems = 0;
  da->size = DARRAY_MIN_SIZE;
  da->array = malloc_assert(DARRAY_MIN_SIZE * sizeof(uintptr_t));
  return da;
}

darray_t*
darray_create_copy(darray_t* from)
{
  darray_t* da = darray_create();
  darray_copy(da, from);
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

#define DARRAY_RESIZE(da)					\
  if (unlikely(da->n_elems == da->size))			\
    {								\
      size_t size_new = DARRAY_GROW_MUL * da->size;		\
      da->array = realloc_assert(da->array,			\
				 size_new * sizeof(uintptr_t));	\
      da->size = size_new;					\
    }
  


void
darray_add(darray_t* da, uintptr_t elem)
{
  DARRAY_RESIZE(da);
  da->array[da->n_elems++] = elem;
}

void
darray_add_double(darray_t* da, double elem)
{
  DARRAY_RESIZE(da);
  uintptr_t* ptr = da->array + da->n_elems++;
  double* ptrd = (double*) ptr;
  *ptrd = elem;
}

int 
darray_add_uniq(darray_t* da, uintptr_t elem)
{
  if (darray_exists(da, elem))
    {
      return 0;
    }
  darray_add(da, elem);
  return 1;
}


static void
darray_shift_right(darray_t* da)
{
  DARRAY_RESIZE(da);
  if (da->n_elems++ == 0)
    {
      return;
    }

  for (int i = da->n_elems - 1; i > 0; i--)
    {
      da->array[i] = da->array[i - 1];
    }
  da->array[0] = 0;
}

void
darray_push(darray_t* da, uintptr_t elem)
{
  darray_shift_right(da);
  da->array[0] = elem;
}


int
darray_remove(darray_t* da, uintptr_t elem)
{
  for (int i = 0; i < da->n_elems; i++)
    {
      if (unlikely(elem == da->array[i]))
	{
	  da->n_elems--;
	  for (int j = i; j < da->n_elems; j++)
	    {
	      da->array[j] = da->array[j + 1];
	    }
	  return 1;
	}
    }
  return 0;
}

int
darray_pop(darray_t* da, uintptr_t* elem)
{
  if (unlikely(da->n_elems == 0))
    {
      return 0;
    }

  *elem = da->array[0];
  da->n_elems--;
  for (int i = 0; i < da->n_elems; i++)
    {
      da->array[i] = da->array[i + 1];
    }
  return 1;
}


inline uintptr_t
darray_get(darray_t* da, const size_t idx)
{
  return da->array[idx];
}

int
darray_elem_is_at(darray_t* da, uintptr_t elem, const size_t idx)
{
  return da->array[idx] == elem;
}

inline int			
darray_exists(darray_t* da, uintptr_t elem)
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

inline size_t
darray_get_num_elems(darray_t* da)
{
  return da->n_elems;
}

inline uintptr_t
darray_get_elem_n(darray_t* da, uintptr_t n)
{
  assert(n < da->n_elems);
  return da->array[n];
}

static inline void
darray_swap_if_greater(size_t* arr, int a, int b)
{
  if (arr[a] > arr[b])
    {
      uintptr_t tmp = arr[a];
      arr[a] = arr[b];
      arr[b] = tmp;
    }
}

void
darray_sort(darray_t* da)
{
  for (int i = 0; i < da->n_elems; i++)
    {
      for (int j = i + 1; j < da->n_elems; j++)
	{
	  darray_swap_if_greater(da->array, i, j);
	}
    }
}

void
darray_copy(darray_t* to, darray_t* from)
{
  darray_empty(to);
  DARRAY_FOR_EACH(from, i)
    {
      darray_add(to, DARRAY_GET_N(from, i));
    }
}

/* ******************************************************************************** */
/* iterator */
/* ******************************************************************************** */

void
darray_iter_init(darray_iter_t* dai, darray_t* da)
{
  dai->darray = da;
  dai->curr = 0;
}

inline int			/* error or OK */
darray_iter_next(darray_iter_t* dai, uintptr_t* elem)
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

