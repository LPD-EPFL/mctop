#include <mctop_sort.h>
#include <merge_utils.h>
#include <algorithm>    // std::sort
#include <nmmintrin.h>

void* mctop_sort_thr(void* params);

void
mctop_sort(MCTOP_SORT_TYPE* array, const size_t n_elems, mctop_node_tree_t* nt)
{
#warning comment out
  // if (unlikely(n_elems <= MCTOP_SORT_MIN_LEN_PARALLEL) ||
  //     mctop_alloc_get_num_hw_contexts(nt->alloc) == 1)
  //   {
  //     std::sort(array, array + n_elems);
  //     return;
  //   }

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


static inline size_t
size_get_num_div_by_a_b(const size_t in, const uint a, const uint b)
{
  size_t s = in;
  uint min = a < b ? a : b;
  uint m = a * b;
  if (likely(a > 1 && b > 1))
    {
      while (1)
	{
	  const uint nm = m / min;
	  if (!((nm % a) == 0 && ((nm % b) == 0) && nm <= min))
	    {
	      break;
	    }
	  m = nm;
	}
    }
  else if (a > 1)
    {
      while (1)
	{
	  const uint nm = m / min;
	  if (!((nm % a) == 0 && nm <= min))
	    {
	      break;
	    }
	  m = nm;
	}
    }
  else if (b > 1)				// b > 1
    {
      while (1)
	{
	  const uint nm = m / min;
	  if (!((nm % b) == 0 && nm <= min))
	    {
	      break;
	    }
	  m = nm;
	}
    }

  while (s % m)
    {
      s--;
    }

  return s;
}


void mctop_sort_merge_in_socket(mctop_alloc_t* alloc, mctop_sort_nd_t* nd, const uint node, mctop_type_t barrier_for);

static void
print_error_sorted(MCTOP_SORT_TYPE* array, const size_t n_elems, const uint print_always)
{
  MSD_DO(
	 uint sorted = 1;
	 for (size_t i = 1; i < n_elems; i++)
	   {
	     if (array[i - 1] > array[i])
	       {
		 printf(" >>> error: array[%zu] = %u > array[%zu] = %u\n",
			i - 1, array[i - 1], i, array[i]);
		 sorted = 0;
		 break;
	       }
	   }
	 if (print_always || !sorted)
	   {
	     printf("## Array %p {size %-10zu} is sorted: %u\n", array, n_elems, sorted);
	   }
	 );
}

void mctop_sort_merge_cross_socket(mctop_sort_td_t* td, const uint my_node);

void*
mctop_sort_thr(void* params)
{
  mctop_sort_td_t* td = (mctop_sort_td_t*) params;
  const size_t tot_size = td->n_elems * sizeof(MCTOP_SORT_TYPE);
  mctop_node_tree_t* nt = td->nt;
  mctop_alloc_t* alloc = nt->alloc;
  const size_t node_size = tot_size / alloc->n_sockets;

  mctop_alloc_pin(alloc);
  //  MSD_DO(mctop_alloc_thread_print();)

  const uint my_node = mctop_alloc_thread_node_id();
  mctop_sort_nd_t* nd = &td->node_data[my_node];
  const uint my_node_n_hwcs = mctop_alloc_get_num_hw_contexts_node(alloc, my_node);

  if (mctop_alloc_thread_is_node_leader())
    {
      MSD_DO(printf("Node %u :: Handle %zu KB\n", my_node, node_size / 1024););
      nd->source = (MCTOP_SORT_TYPE*) mctop_alloc_malloc_on_nth_socket(alloc, my_node, tot_size);
      nd->destination = (MCTOP_SORT_TYPE*) mctop_alloc_malloc_on_nth_socket(alloc, my_node, tot_size);
      assert(nd->source != NULL && nd->destination != NULL);
      nd->n_chunks = my_node_n_hwcs * MCTOP_NUM_CHUNKS_PER_THREAD;
      nd->partitions = (mctop_sort_pd_t*) malloc(nd->n_chunks * sizeof(mctop_sort_pd_t));
    }
  mctop_alloc_barrier_wait_node(alloc);

  MCTOP_SORT_TYPE* array_a = nd->source;

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

  //  MSD_DO(printf("Thread %-3u :: Handle %zu elems\n", mctop_alloc_thread_id(), my_n_elems););

  MCTOP_SORT_TYPE* copy = nd->array + my_offset_socket;
  MCTOP_SORT_TYPE* dest = array_a + my_offset_socket;

  const uint my_n_elems_c = my_n_elems / MCTOP_NUM_CHUNKS_PER_THREAD;
  for (uint j = 0; j < MCTOP_NUM_CHUNKS_PER_THREAD; j++)
    {
      const uint offs = j * my_n_elems_c;
      const uint partition_index = (mctop_alloc_thread_insocket_id() * MCTOP_NUM_CHUNKS_PER_THREAD) + j;
      nd->partitions[partition_index].start_index = my_offset_socket + offs;
      nd->partitions[partition_index].n_elems = my_n_elems_c;
      
      // copy and sort in array_a = nd->source;
      MCTOP_SORT_TYPE* low = dest + offs;
      memcpy(low, copy + offs, my_n_elems_c * sizeof(MCTOP_SORT_TYPE));
      MCTOP_SORT_TYPE* high = low + my_n_elems_c;
      std::sort(low, high);
    }

  mctop_alloc_barrier_wait_node(alloc);
  if (mctop_alloc_thread_incore_id() == 0) // only cores!!
    {
      mctop_sort_merge_in_socket(alloc, nd, my_node, nt->barrier_for);

      // the sorted array is in nd->source
      if (mctop_alloc_thread_is_node_leader())
	{
	  print_error_sorted(nd->source, nd->n_elems, 1);
          nd->n_elems = node_size / sizeof(MCTOP_SORT_TYPE);
	}

      if (nt->n_nodes > 1)
	{
	  mctop_sort_merge_cross_socket(td, my_node);
	}
      else
	{
#warning need to land to the correct array!
	}

    }


  return NULL;
}


void static
mctop_merge_barrier_wait(mctop_alloc_t* alloc, mctop_type_t barrier_for)
{
  if (barrier_for == CORE)
    {
      mctop_alloc_barrier_wait_node_cores(alloc);
    }
  else if (barrier_for == HW_CONTEXT)
    {
      mctop_alloc_barrier_wait_node(alloc);
    }
}

void mctop_sort_merge(MCTOP_SORT_TYPE* src, MCTOP_SORT_TYPE* dest,
		      mctop_sort_pd_t* partitions, const uint n_partitions,
		      const uint threads_per_partition, const uint nthreads);


static inline uint
n_threads_per_part_calc(const uint n_partitions, const uint n_threads)
{
  return n_threads;
}


void
mctop_sort_merge_in_socket(mctop_alloc_t* alloc, mctop_sort_nd_t* nd, const uint node, mctop_type_t barrier_for)
{
  MCTOP_SORT_TYPE* src = nd->source;
  MCTOP_SORT_TYPE* dest = nd->destination;

  uint n_partitions = nd->n_chunks;
  const uint nthreads = mctop_alloc_get_num_cores_node(alloc, node);
  uint threads_per_partition = nthreads;

  if (mctop_alloc_thread_is_node_leader())
    {
      MSD_DO(printf("Node %u :: Have %u partitions\n", node, n_partitions););
    }

  while (n_partitions > 1)
    {
      threads_per_partition = n_threads_per_part_calc(n_partitions, nthreads);

      mctop_merge_barrier_wait(alloc, barrier_for);
      
      if (mctop_alloc_thread_is_node_leader())
      	{
      	  // MSD_DO(printf("#### Merge round %u\n", n_partitions););
      	  // printf(" partition_start  partition_size\n");
      	  for(uint i = 0; i < n_partitions; i++)
      	    {
	      // printf(" %8ld  %8ld\n", nd->partitions[i].start_index, nd->partitions[i].n_elems);
	      print_error_sorted(src + nd->partitions[i].start_index, nd->partitions[i].n_elems, 0);
      	    }
      	}

      mctop_sort_merge(src, dest, nd->partitions, n_partitions, threads_per_partition, nthreads);
      mctop_merge_barrier_wait(alloc, barrier_for);

      if (mctop_alloc_thread_is_node_leader())
	{
	  for (uint i = 0; i < n_partitions >> 1; i++)
	    {
	      nd->partitions[i].start_index = nd->partitions[i<<1].start_index;
	      nd->partitions[i].n_elems = nd->partitions[i<<1].n_elems + nd->partitions[(i<<1) + 1].n_elems;
	    }

	  //	  printf("Node %u :: ending round\n", mctop_alloc_thread_node_id()); 
	  if (n_partitions & 1)
	    {
	      nd->partitions[n_partitions >> 1].start_index = nd->partitions[n_partitions - 1].start_index;
	      nd->partitions[n_partitions >> 1].n_elems = nd->partitions[n_partitions - 1].n_elems;
	      memcpy((void*) &dest[nd->partitions[n_partitions - 1].start_index],
		     (void*) &src[nd->partitions[n_partitions - 1].start_index],
		     nd->partitions[n_partitions - 1].n_elems * sizeof(MCTOP_SORT_TYPE));
	    }
	}

      if (n_partitions & 1)
	{
	  n_partitions++;
	}
      n_partitions >>= 1;

      MCTOP_SORT_TYPE* temp = src;
      src = dest;
      dest = temp;
    }

  if (mctop_alloc_thread_is_node_leader())
    {
      if (nd->source != src)
      	{
	  nd->source = src;
	  nd->destination = dest;
	}
    }
}

void
mctop_sort_merge(MCTOP_SORT_TYPE* src, MCTOP_SORT_TYPE* dest,
		 mctop_sort_pd_t* partitions, const uint n_partitions,
		 const uint threads_per_partition, const uint nthreads)
{
  const uint my_id = mctop_alloc_thread_core_insocket_id();
  
  uint next_merge = my_id / threads_per_partition;
  uint next_partition = next_merge << 1;
  while (1)
    {
      if (next_partition >= n_partitions || ((n_partitions % 2) && (next_partition >= n_partitions-1)))
	break;

      const uint pos_in_merge = my_id % threads_per_partition;
    
      const uint partition_a_start = partitions[next_partition].start_index;
      const uint partition_a_size = partitions[next_partition].n_elems;
      const uint partition_b_start = partitions[next_partition + 1].start_index;
      const uint partition_b_size = partitions[next_partition + 1].n_elems;

      MCTOP_SORT_TYPE* my_a = &src[partition_a_start];
      MCTOP_SORT_TYPE* my_b = &src[partition_b_start];
      MCTOP_SORT_TYPE* my_dest = &dest[partition_a_start];

      merge_arrays(my_a, my_b, my_dest, partition_a_size, partition_b_size, pos_in_merge, threads_per_partition);
      next_partition += (nthreads / threads_per_partition) << 1;
    }
}

void
mctop_sort_merge_cross_socket(mctop_sort_td_t* td, const uint my_node)
{
  mctop_node_tree_t* nt = td->nt;
  mctop_alloc_t* alloc = nt->alloc;
  mctop_sort_nd_t* my_nd = &td->node_data[my_node];

  for (int l = mctop_node_tree_get_num_levels(nt) - 1; l >= 0; l--)
    {
      mctop_node_tree_work_t ntw;
      if (mctop_node_tree_get_work_description(nt, l, &ntw))
        {
          mctop_node_tree_barrier_wait(nt, l);
	  uint my_merge_id = mctop_alloc_thread_core_insocket_id();
          const uint threads_in_merge = mctop_alloc_get_num_cores_node(alloc, my_node) +
	    mctop_alloc_get_num_cores_node(alloc, ntw.other_node);

	  uint a, b;
	  if (ntw.node_role == DESTINATION)
	    {
	      a = my_node;
	      b = ntw.other_node;
	    }
	  else 			// SOURCE_ONLY
	    {
	      a = ntw.other_node;
	      b = my_node;
	      my_merge_id += mctop_alloc_get_num_cores_node(alloc, ntw.other_node);
	    }

          MCTOP_SORT_TYPE* my_a = td->node_data[a].source;
	  const size_t n_elems_a = td->node_data[a].n_elems;
	  MCTOP_SORT_TYPE* my_b = td->node_data[b].source;
	  const size_t n_elems_b = td->node_data[b].n_elems;
	  MCTOP_SORT_TYPE* my_dest = td->node_data[a].destination;
	  if (l == 0)
	    {
	      my_dest = td->array;
	    }

          MSD_DO(printf("[Node %u] L%u: MId %-2u: DST %u: a=%p [#%zu], b=%p[#%zu], d=%p -- #thr %u\n", my_node, l, my_merge_id,
			a, my_a, n_elems_a, my_b, n_elems_b, my_dest, threads_in_merge););

          mctop_node_tree_barrier_wait(nt, l);
	  
          merge_arrays(my_a, my_b, my_dest, n_elems_a, n_elems_b, my_merge_id, threads_in_merge);
          mctop_node_tree_barrier_wait(nt, l);
            
          if (mctop_alloc_thread_is_node_leader())
	    {
              MSD_DO(printf("Node %d:: Switching pointers @ lvl %d!\n", my_node, l););
	      my_nd->n_elems = n_elems_a + n_elems_b;
              MCTOP_SORT_TYPE* tmp = my_nd->source;
              my_nd->source = my_nd->destination;
              my_nd->destination = tmp;
            }
	}
      else
	{
	  if (mctop_alloc_thread_is_node_leader())
	    {
	      MSD_DO( printf("Node %d. No work for node @ lvl %d!\n", my_node, l););
	      return;
	    }
	}

      mctop_node_tree_barrier_wait(nt, l);
    }
}

