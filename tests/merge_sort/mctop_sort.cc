#include <mctop_sort.h>

void* mctop_sort_thr(void* params);


typedef struct mctop_sort_td
{
  mctop_node_tree_t* nt;
  MCTOP_SORT_TYPE* array;
  size_t n_elems;
  MCTOP_SORT_TYPE* node_arrays[0];  
} mctop_sort_td_t;


void
mctop_sort(MCTOP_SORT_TYPE* array, const size_t n_elems, mctop_node_tree_t* nt)
{
  mctop_alloc_t* alloc = nt->alloc;

  const uint n_hwcs = mctop_alloc_get_num_hw_contexts(alloc);
  pthread_t threads[n_hwcs];
  pthread_attr_t attr;
  void* status;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);


  mctop_sort_td_t* td = (mctop_sort_td_t*) malloc(sizeof(mctop_sort_td_t) +
						  (mctop_alloc_get_num_sockets(alloc) *
						   sizeof(MCTOP_SORT_TYPE*)));
  td->nt = nt;
  td->array = array;
  td->n_elems = n_elems;

  for(uint t = 0; t < n_hwcs; t++)
    {
      int rc = pthread_create(&threads[t], &attr, mctop_sort_thr, td);
      if (rc)
	{
	  printf("ERROR; return code from pthread_create() is %d\n", rc);
	  exit(-1);
	}
    }

  pthread_attr_destroy(&attr);

  for(uint t = 0; t < n_hwcs; t++)
    {
      int rc = pthread_join(threads[t], &status);
      if (rc) 
	{
	  printf("ERROR; return code from pthread_join() is %d\n", rc);
	  exit(-1);
	}
    }

  free(td);
}


void*
mctop_sort_thr(void* params)
{
  mctop_sort_td_t* td = (mctop_sort_td_t*) params;
  mctop_node_tree_t* nt = td->nt;
  mctop_alloc_t* alloc = nt->alloc;
  mctop_alloc_pin(alloc);


  mctop_alloc_thread_print();  
  return NULL;
}
