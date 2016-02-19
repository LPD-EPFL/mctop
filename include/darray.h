/*   
 *   File: darray.h
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: darray macros
 *
 *
 */

#ifndef _DARRAY_H_
#define _DARRAY_H_

#include <helper.h>

#define DARRAY_MIN_SIZE 8
#define DARRAY_GROW_MUL 2

typedef struct darray
{
  size_t n_elems;
  size_t size;
  size_t* array;
} darray_t;

typedef struct darray_iter
{
  darray_t* darray;
  size_t curr;
} darray_iter_t;


darray_t* darray_create();
void darray_free(darray_t* da);
void darray_empty(darray_t* da);

void darray_add(darray_t* da, size_t elem);
int darray_add_uniq(darray_t* da, size_t elem);

int darray_exists(darray_t* da, size_t elem);
size_t darray_get_num_elems(darray_t* da);
size_t darray_get_elem_n(darray_t* da, size_t n);

void darray_sort(darray_t* da);
void darray_copy(darray_t* to, darray_t* from);


void darray_iter_init(darray_iter_t* dai, darray_t* da);
int darray_iter_next(darray_iter_t* dai, size_t* elem);

#define DARRAY_FOR_EACH(da, idx)		\
  for (int idx = 0; idx < (da)->n_elems; idx++)
#define DARRAY_GET_N(da, idx)			\
  (da)->array[idx]

void darray_print(darray_t* da);

#endif	/* _DARRAY_H_ */
