#include <mctop_sort.h>
#include <algorithm>    // std::sort

void* mctop_sort_thr(void* params);

void
mctop_sort(MCTOP_SORT_TYPE* array, const size_t n_elems, mctop_node_tree_t* nt)
{
  if (unlikely(n_elems <= MCTOP_SORT_MIN_LEN_PARALLEL))
    {
      std::sort(array, array + n_elems);
      return;
    }

  mctop_alloc_t* alloc = nt->alloc;

  const uint n_hwcs = mctop_alloc_get_num_hw_contexts(alloc);
  const uint n_sockets = mctop_alloc_get_num_sockets(alloc);

  pthread_t threads[n_hwcs];
  pthread_attr_t attr;
  void* status;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  mctop_sort_td_t* td = (mctop_sort_td_t*) malloc(sizeof(mctop_sort_td_t) +
						  (n_sockets * sizeof(mctop_sort_nd_t)));
  assert(td != NULL);
  td->nt = nt;
  td->array = array;
  td->n_elems = n_elems;

  const size_t n_elems_nd = n_elems / n_sockets;
  for (uint i = 0; i < n_sockets; i++)
    {
      td->node_data[i].array = array + (i * n_elems_nd);
      td->node_data[i].n_elems = n_elems_nd;
    }
  td->node_data[n_sockets - 1].n_elems += (n_elems % n_elems_nd);

  for(uint t = 0; t < n_hwcs; t++)
    {
      if (pthread_create(&threads[t], &attr, mctop_sort_thr, td))
	{
	  printf("mctop_sort ERROR: pthread_create()\n");
	  exit(-1);
	}
    }

  pthread_attr_destroy(&attr);

  for(uint t = 0; t < n_hwcs; t++)
    {
      if (pthread_join(threads[t], &status))
	{
	  printf("mctop_sort ERROR: pthread_join()\n");
	  exit(-1);
	}
    }

  free(td);
}

struct two_arrays
{
  MCTOP_SORT_TYPE* left;
  MCTOP_SORT_TYPE* right;
};


static inline size_t
size_get_num_div_by_a_b(const size_t in, const uint a, const uint b)
{
  size_t s = in;
  uint min = a < b ? a : b;
  uint m = a * b;
  while (1)
    {
      const uint nm = m / min;
      if (!((nm % a) == 0 && ((nm % b) == 0)))
	{
	  break;
	}
	m = nm;
    }

  while (s % m)
    {
      s--;
    }

  return s;
}

void*
mctop_sort_thr(void* params)
{
  mctop_sort_td_t* td = (mctop_sort_td_t*) params;
  mctop_node_tree_t* nt = td->nt;
  mctop_alloc_t* alloc = nt->alloc;

  mctop_alloc_pin(alloc);
  MSD_DO(mctop_alloc_thread_print();)

  const uint my_node = mctop_alloc_thread_node_id();
  mctop_sort_nd_t* nd = &td->node_data[my_node];
  const size_t node_size = nd->n_elems * sizeof(MCTOP_SORT_TYPE);
  const uint my_node_n_hwcs = mctop_alloc_get_num_hw_contexts_node(alloc, my_node);

  if (mctop_alloc_thread_is_node_leader())
    {
      MSD_DO(printf("Node %u :: Handle %zu KB\n", my_node, node_size / 1024););
      nd->left = (MCTOP_SORT_TYPE*) mctop_alloc_malloc_on_nth_socket(alloc, my_node, 2 * node_size);
      nd->right = nd->left + node_size;
      nd->n_chunks = my_node_n_hwcs * MCTOP_NUM_CHUNKS_PER_THREAD;
    }
  mctop_alloc_barrier_wait_node(alloc);

  MCTOP_SORT_TYPE* array_a = nd->left, * array_b = nd->right;

  size_t my_n_elems = nd->n_elems / my_node_n_hwcs;
  const uint my_node_n_hwcs_merge = (nt->barrier_for == CORE) ?
    mctop_alloc_get_num_cores_node(alloc, my_node) :
    mctop_alloc_get_num_hw_contexts_node(alloc, my_node);
  my_n_elems = size_get_num_div_by_a_b(my_n_elems, MCTOP_SSE_K, my_node_n_hwcs_merge);

  const uint my_offset_socket = mctop_alloc_thread_insocket_id() * my_n_elems;

  if (unlikely(mctop_alloc_thread_is_node_last()))
    {
      my_n_elems = nd->n_elems - ((my_node_n_hwcs - 1) * my_n_elems);
    }

  MSD_DO(printf("Thread %-3u :: Handle %zu elems\n", mctop_alloc_thread_id(), my_n_elems););

  MCTOP_SORT_TYPE* copy = nd->array + my_offset_socket;
  MCTOP_SORT_TYPE* dest = array_a + my_offset_socket;

  const uint my_n_elems_c = my_n_elems / MCTOP_NUM_CHUNKS_PER_THREAD;
  for (uint j = 0; j < MCTOP_NUM_CHUNKS_PER_THREAD; j++)
    {
      const uint offs = j * my_n_elems_c;

#if MCTOP_SORT_COPY_FIRST == 0
      MCTOP_SORT_TYPE* low = copy + offs;
      MCTOP_SORT_TYPE* high = copy + my_n_elems_c;
#else
      memcpy(dest + offs, copy + offs, my_n_elems_c * sizeof(MCTOP_SORT_TYPE));
      MCTOP_SORT_TYPE* low = dest + offs;
      MCTOP_SORT_TYPE* high = dest + my_n_elems_c;
#endif
      std::sort(low, high);
    }

  return NULL;
}
