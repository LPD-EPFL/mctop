#include <mctop_sort.h>
#include <algorithm>    // std::sort

void* mctop_sort_thr(void* params);


typedef struct mctop_sort_td
{
  mctop_node_tree_t* nt;
  MCTOP_SORT_TYPE* array;
  size_t n_elems;
  MCTOP_SORT_TYPE* node_arrays_1[0];  
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


#define MCTOP_SORT_DEBUG 0
struct two_arrays
{
  MCTOP_SORT_TYPE* left;
  MCTOP_SORT_TYPE* right;
};

void*
mctop_sort_thr(void* params)
{
  mctop_sort_td_t* td = (mctop_sort_td_t*) params;
  mctop_node_tree_t* nt = td->nt;
  mctop_alloc_t* alloc = nt->alloc;
  mctop_alloc_pin(alloc);
#if MCTOP_SORT_DEBUG == 1
  mctop_alloc_thread_print();  
#endif
  const uint my_insocket_id = mctop_alloc_thread_insocket_id();
  const uint my_node = mctop_alloc_thread_node_id();
  const uint node_len = td->n_elems / mctop_alloc_get_num_sockets(alloc);
  const uint node_size = node_len * sizeof(MCTOP_SORT_TYPE);

  if (mctop_alloc_thread_is_node_leader())
    {
#if MCTOP_SORT_DEBUG == 1
      printf("Node %u :: Handle %u KB\n", my_node, node_size / 1024);
#endif
      struct two_arrays* ta = (struct two_arrays*) malloc(sizeof(struct two_arrays));
      ta->left = (MCTOP_SORT_TYPE*) mctop_alloc_malloc_on_nth_socket(alloc, my_node, 2 * node_size);
      ta->right = ta->left + node_size;
      mctop_node_tree_scratchpad_set(nt, my_node, ta);
    }
  mctop_alloc_barrier_wait_node(alloc);

  struct two_arrays* ta = (struct two_arrays*) mctop_node_tree_scratchpad_get(nt, my_node);
  MCTOP_SORT_TYPE* array_a = ta->left, * array_b = ta->right;
  if (mctop_alloc_thread_is_node_leader())
    {
      free(ta);
    }

  size_t my_len = node_len / mctop_alloc_get_num_hw_contexts_node(alloc, my_node);
  const size_t my_offset_socket = my_insocket_id * my_len;
  const size_t my_offset_global = (my_node * node_len) + my_offset_socket;
  if (mctop_alloc_thread_is_node_last())
    {
      const uint extra = node_len % mctop_alloc_get_num_hw_contexts_node(alloc, my_node);
      my_len += extra;
    }
#if MCTOP_SORT_DEBUG == 1
  printf("%u will handle %zu elems\n", mctop_alloc_thread_id(), my_len);
#endif
  MCTOP_SORT_TYPE* copy = td->array + my_offset_global;
  MCTOP_SORT_TYPE* dest = array_a + my_offset_socket;

  // memcpy(dest, copy, my_len * sizeof(MCTOP_SORT_TYPE));
  // std::sort(dest, dest + my_len);
  //  memcpy(dest, copy, my_len * sizeof(MCTOP_SORT_TYPE));

  const uint n_chunks = 1;
  const size_t my_len_c = my_len / n_chunks;
  for (uint j = 0; j < n_chunks; j++)
    {
      const size_t offs = j * my_len_c;

#if MCTOP_SORT_FOR_QSORT == 0
      MCTOP_SORT_TYPE* low = copy + offs;
      MCTOP_SORT_TYPE* high = copy + my_len_c;
#else
      memcpy(dest + offs, copy + offs, my_len_c * sizeof(MCTOP_SORT_TYPE));
      MCTOP_SORT_TYPE* low = dest + offs;
      MCTOP_SORT_TYPE* high = dest + my_len_c;
#endif
      std::sort(low, high);
    }

  return NULL;
}
