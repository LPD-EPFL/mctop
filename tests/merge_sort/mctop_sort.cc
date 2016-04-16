#include <mctop_sort.h>
#include <merge_utils.h>
#include <algorithm>    // std::sort
#include <string.h>

void* mctop_sort_thr(void* params);

void
mctop_sort(MCTOP_SORT_TYPE* array, const size_t n_elems, mctop_node_tree_t* nt)
{
  if (unlikely(n_elems <= MCTOP_SORT_MIN_LEN_PARALLEL) ||
      mctop_alloc_get_num_hw_contexts(nt->alloc) == 1)
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


void mctop_sort_merge_in_socket(mctop_alloc_t* alloc, mctop_sort_nd_t* nd, const uint node);

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

static inline uint
mctop_sort_thread_insocket_merge_participate()
{
#if MCTOP_SORT_USE_SSE == 1 || MCTOP_SORT_USE_SSE == 4
  if (mctop_alloc_thread_incore_id() == 0) // only cores!!
    {
      return 1;
    }
  return 0;
#else  // for every other type all hyperthreads participate
  return 1;
#endif
}
  
static inline uint
mctop_sort_thread_crosssocket_merge_participate()
{
#if MCTOP_SORT_USE_SSE == 1 || MCTOP_SORT_USE_SSE == 2 || MCTOP_SORT_USE_SSE == 4
  if (mctop_alloc_thread_incore_id() == 0) // only cores!!
    {
      return 1;
    }
  return 0;
#else  // everyone!
  return 1;
#endif
}


void*
mctop_sort_thr(void* params)
{
  MCTOP_F_STEP(__steps, __a, __b);
  mctop_sort_td_t* td = (mctop_sort_td_t*) params;
  const size_t tot_size = td->n_elems * sizeof(MCTOP_SORT_TYPE);
  mctop_node_tree_t* nt = td->nt;
  mctop_alloc_t* alloc = nt->alloc;
  const size_t node_size = tot_size / alloc->n_sockets;

  mctop_alloc_pin(alloc);
  //#if MCTOP_SORT_USE_NUMA_ALLOC == 0
  //mctop_hwcid_fix_numa_node(alloc->topo, mctop_alloc_thread_hw_context_id());
  //#endif
  //  MSD_DO(mctop_alloc_thread_print();)

  const uint my_node = mctop_alloc_thread_node_id();
  mctop_sort_nd_t* nd = &td->node_data[my_node];
  const uint my_node_n_hwcs = mctop_alloc_get_num_hw_contexts_node(alloc, my_node);

  if (mctop_alloc_thread_is_node_leader())
    {
      MSD_DO(printf("Node %u :: Handle %zu KB\n", my_node, node_size / 1024););
#if __sparc__
      nd->source = (MCTOP_SORT_TYPE*) malloc(2 * tot_size);
#elif MCTOP_SORT_USE_NUMA_ALLOC == 1
      nd->source = (MCTOP_SORT_TYPE*) mctop_alloc_malloc_on_nth_socket(alloc, my_node, 2 * tot_size);
#else
      nd->source = (MCTOP_SORT_TYPE*) malloc(2 * tot_size);
#endif	// MCTOP_SORT_USE_NUMA_ALLOC == 1
      nd->destination = nd->source + td->n_elems;
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

  // ////////////////////////////////////////////////////////////////////////////////////////////////////
  // sequential sorting of chunks
  // ////////////////////////////////////////////////////////////////////////////////////////////////////

  MCTOP_SORT_TYPE* copy = nd->array + my_offset_socket;
  MCTOP_SORT_TYPE* dest = array_a + my_offset_socket;

  MCTOP_P_STEP("preparation", __steps, __a, __b, !mctop_alloc_thread_id());
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
      MCTOP_P_STEP("memcpy", __steps, __a, __b, !mctop_alloc_thread_id());
      std::sort(low, high);
      MCTOP_P_STEP("seq sort", __steps, __a, __b, !mctop_alloc_thread_id());
    }

#if MCTOP_SORT_USE_SSE == 1 || MCTOP_SORT_USE_SSE == 2 || MCTOP_SORT_USE_SSE == 4
  // need extra barrier, cause the SMT threads will not need
  // to wait on the first merge barriers
  mctop_alloc_barrier_wait_node(alloc);
#endif

  // ////////////////////////////////////////////////////////////////////////////////////////////////////
  // merging 
  // ////////////////////////////////////////////////////////////////////////////////////////////////////

  if (likely(mctop_sort_thread_insocket_merge_participate())) // with SSE, only cores participate
    {
      // ///////////////////////////////////////////////////////////////////////
      // in-socket merging
      // ///////////////////////////////////////////////////////////////////////
      mctop_sort_merge_in_socket(alloc, nd, my_node);
      MCTOP_P_STEP("in-socket merge", __steps, __a, __b, !mctop_alloc_thread_id());
    }

  if (likely(mctop_sort_thread_crosssocket_merge_participate()))
    {
      // the sorted array is in nd->source
      if (nt->n_nodes > 1)
	{
	  if (mctop_alloc_thread_is_node_leader())
	    {
	      free(nd->partitions);
	      print_error_sorted(nd->source, nd->n_elems, 1);
	      nd->n_elems = node_size / sizeof(MCTOP_SORT_TYPE);
	    }
	  // ///////////////////////////////////////////////////////////////////////
	  // cross-socket merging
	  // ///////////////////////////////////////////////////////////////////////
	  mctop_sort_merge_cross_socket(td, my_node);
	}
    }

  if (nt->n_nodes == 1)
    {
      mctop_alloc_barrier_wait_node(alloc);
      memcpy(td->array + my_offset_socket,
	     nd->source + my_offset_socket,
	     my_n_elems * sizeof(MCTOP_SORT_TYPE));
    }

  mctop_alloc_barrier_wait_all(alloc);
  MCTOP_P_STEP("cross-socket merge", __steps, __a, __b, !mctop_alloc_thread_id());
  
  if (mctop_alloc_thread_is_node_leader())
    {
#if __sparc__ || MCTOP_SORT_USE_NUMA_ALLOC == 1
      mctop_alloc_malloc_free(array_a, tot_size);
#else
      free(array_a);
#endif	// MCTOP_SORT_USE_NUMA_ALLOC == 1
    }

  return NULL;
}


void static
mctop_merge_barrier_wait(mctop_alloc_t* alloc)
{
#if MCTOP_SORT_USE_SSE == 1 || MCTOP_SORT_USE_SSE == 4
  mctop_alloc_barrier_wait_node_cores(alloc);
#else
  mctop_alloc_barrier_wait_node(alloc);
#endif
}

#if MCTOP_SORT_USE_SSE == 2
void mctop_sort_merge(MCTOP_SORT_TYPE* src, MCTOP_SORT_TYPE* dest,
		      mctop_sort_pd_t* partitions, const uint n_partitions,
		      const uint threads_per_partition, const uint nthreads, const uint n_cores_socket);
#else
void mctop_sort_merge(MCTOP_SORT_TYPE* src, MCTOP_SORT_TYPE* dest,
		      mctop_sort_pd_t* partitions, const uint n_partitions,
		      const uint threads_per_partition, const uint nthreads);
#endif

static inline uint
n_threads_per_part_calc(const uint n_partitions, const uint n_threads)
{
  return n_threads;
}


void
mctop_sort_merge_in_socket(mctop_alloc_t* alloc, mctop_sort_nd_t* nd, const uint node)
{
  MCTOP_SORT_TYPE* src = nd->source;
  MCTOP_SORT_TYPE* dest = nd->destination;

  uint n_partitions = nd->n_chunks;
#if MCTOP_SORT_USE_SSE == 1 || MCTOP_SORT_USE_SSE == 2 || MCTOP_SORT_USE_SSE == 4
  const uint n_threads = mctop_alloc_get_num_cores_node(alloc, node);
#else
  const uint n_threads = mctop_alloc_get_num_hw_contexts_node(alloc, node);
#endif

  uint threads_per_partition = n_threads;

  if (mctop_alloc_thread_is_node_leader())
    {
      MSD_DO(printf("Node %u :: Have %u partitions\n", node, n_partitions););
    }

  while (n_partitions > 1)
    {
      threads_per_partition = n_threads_per_part_calc(n_partitions, n_threads);

      mctop_merge_barrier_wait(alloc);
      
      if (mctop_alloc_thread_is_node_leader())
      	{
      	  //MSD_DO(printf("#### Merge round %u\n", n_partitions););
      	  //printf(" partition_start  partition_size\n");
      	  for(uint i = 0; i < n_partitions; i++)
      	    {
	      //printf(" %8ld  %8ld\n", nd->partitions[i].start_index, nd->partitions[i].n_elems);
	      print_error_sorted(src + nd->partitions[i].start_index, nd->partitions[i].n_elems, 0);
      	    }
      	}
#if MCTOP_SORT_USE_SSE == 2
      uint n_cores_socket = mctop_alloc_get_num_cores_node(alloc, mctop_alloc_thread_local_node());
      mctop_sort_merge(src, dest, nd->partitions, n_partitions, threads_per_partition, n_threads, n_cores_socket);
#else
      mctop_sort_merge(src, dest, nd->partitions, n_partitions, threads_per_partition, n_threads);
#endif
      mctop_merge_barrier_wait(alloc);

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


#if MCTOP_SORT_USE_SSE == 2
void
mctop_sort_merge(MCTOP_SORT_TYPE* src, MCTOP_SORT_TYPE* dest,
		 mctop_sort_pd_t* partitions, const uint n_partitions,
		 const uint threads_per_partition, const uint n_threads, const uint n_cores_socket)
{
  uint next_merge = (mctop_alloc_thread_incore_id() == 0) ? 0 : MCTOP_SORT_SSE_HYPERTHREAD_RATIO;
  // if (mctop_alloc_thread_core_insocket_id() == 0)
    // printf("merge round %u: n_thread %u threads_per_partition %u\n", n_partitions, n_threads, threads_per_partition); 
  uint next_partition = next_merge << 1;
  while (1)
    {
      if (next_partition >= n_partitions || ((n_partitions & 1) && (next_partition >= (n_partitions - 1))))
	{
	  break;
	}

      const uint pos_in_merge = mctop_alloc_thread_core_insocket_id();
    
      // printf("core %u (%u) working on merge %u, partition %u, pos_in_merge %u\n", mctop_alloc_thread_core_insocket_id(), mctop_alloc_thread_incore_id(), next_merge, next_partition, pos_in_merge);
      const uint partition_a_start = partitions[next_partition].start_index;
      const uint partition_a_size = partitions[next_partition].n_elems;
      const uint partition_b_start = partitions[next_partition + 1].start_index;
      const uint partition_b_size = partitions[next_partition + 1].n_elems;

      MCTOP_SORT_TYPE* my_a = &src[partition_a_start];
      MCTOP_SORT_TYPE* my_b = &src[partition_b_start];
      MCTOP_SORT_TYPE* my_dest = &dest[partition_a_start];

      if (mctop_alloc_thread_incore_id() == 0) {
        merge_arrays(my_a, my_b, my_dest, partition_a_size, partition_b_size, pos_in_merge, threads_per_partition);
      }
      else {
        merge_arrays_no_sse(my_a, my_b, my_dest, partition_a_size, partition_b_size, pos_in_merge, threads_per_partition);
      }
      
      if (mctop_alloc_thread_incore_id() == 0) {
          next_merge++;
          if (next_merge % MCTOP_SORT_SSE_HYPERTHREAD_RATIO == 0)
            next_merge++;
      }
      else {
        next_merge += MCTOP_SORT_SSE_HYPERTHREAD_RATIO;
      }
      
      next_partition = next_merge << 1;
    }
}
#else
void
mctop_sort_merge(MCTOP_SORT_TYPE* src, MCTOP_SORT_TYPE* dest,
		 mctop_sort_pd_t* partitions, const uint n_partitions,
		 const uint threads_per_partition, const uint n_threads)
{
#if MCTOP_SORT_USE_SSE == 1
  const uint my_id = mctop_alloc_thread_core_insocket_id();
#else
  const uint my_id = mctop_alloc_thread_insocket_id();
#endif
  //printf("merge round %u: my_id = %u, n_thread %u\n", n_partitions, my_id, n_threads); 
  uint next_merge = my_id / threads_per_partition;
  uint next_partition = next_merge << 1;
  while (1)
    {
      if (next_partition >= n_partitions || ((n_partitions & 1) && (next_partition >= (n_partitions - 1))))
	{
	  break;
	}

      const uint pos_in_merge = my_id % threads_per_partition;
    
      const uint partition_a_start = partitions[next_partition].start_index;
      const uint partition_a_size = partitions[next_partition].n_elems;
      const uint partition_b_start = partitions[next_partition + 1].start_index;
      const uint partition_b_size = partitions[next_partition + 1].n_elems;

      MCTOP_SORT_TYPE* my_a = &src[partition_a_start];
      MCTOP_SORT_TYPE* my_b = &src[partition_b_start];
      MCTOP_SORT_TYPE* my_dest = &dest[partition_a_start];

#if MCTOP_SORT_USE_SSE == 1
      merge_arrays(my_a, my_b, my_dest, partition_a_size, partition_b_size, pos_in_merge, threads_per_partition);
#else
      merge_arrays_no_sse(my_a, my_b, my_dest, partition_a_size, partition_b_size, pos_in_merge, threads_per_partition);
#endif
      next_partition += (n_threads / threads_per_partition) << 1;
    }
}
#endif

void
mctop_sort_merge_cross_socket(mctop_sort_td_t* td, const uint my_node)
{
  MCTOP_F_STEP(__steps, __a, __b);
  mctop_node_tree_t* nt = td->nt;
  // mctop_alloc_t* alloc = nt->alloc;
  mctop_sort_nd_t* my_nd = &td->node_data[my_node];

  for (int l = mctop_node_tree_get_num_levels(nt) - 1; l >= 0; l--)
    {
      mctop_node_tree_work_t ntw;
      if (mctop_node_tree_get_work_description(nt, l, &ntw))
        {
          mctop_node_tree_barrier_wait(nt, l);

#if MCTOP_SORT_USE_SSE == 1 || MCTOP_SORT_USE_SSE == 2
	  uint my_merge_id = mctop_alloc_thread_core_insocket_id();
#else
	  uint my_merge_id = mctop_alloc_thread_insocket_id();
#endif
          const uint threads_in_merge = ntw.num_hw_contexts;
	  my_merge_id += ntw.id_offset;

          MCTOP_SORT_TYPE* my_a = td->node_data[ntw.destination].source;
	  const size_t n_elems_a = td->node_data[ntw.destination].n_elems;
	  MCTOP_SORT_TYPE* my_b = td->node_data[ntw.source].source;
	  const size_t n_elems_b = td->node_data[ntw.source].n_elems;
	  MCTOP_SORT_TYPE* my_dest = td->node_data[ntw.destination].destination;
	  if (l == 0)
	    {
	      my_dest = td->array;
	    }

          MSD_DO(
		 if (mctop_alloc_thread_is_node_leader())
		   {
		     printf("[Nd %u Others %u+%u] L%u: MId %-2u: DST %u: a=%p [#%zu], b=%p[#%zu], d=%p -- #thr %u\n", 
			    my_node, ntw.destination, ntw.source, l, my_merge_id, ntw.destination, my_a, n_elems_a,
			    my_b, n_elems_b, my_dest, threads_in_merge);
		   });

	  
#if MCTOP_SORT_USE_SSE == 1 || MCTOP_SORT_USE_SSE == 2
          merge_arrays(my_a, my_b, my_dest, n_elems_a, n_elems_b, my_merge_id, threads_in_merge);
#else
          merge_arrays_no_sse(my_a, my_b, my_dest, n_elems_a, n_elems_b, my_merge_id, threads_in_merge);
#endif

	  mctop_node_tree_barrier_wait(nt, l);
            
          if (mctop_alloc_thread_is_node_leader())
	    {
	      MSD_DO(
		     if (my_merge_id == 0)
		       {
			 print_error_sorted(my_dest, (n_elems_a + n_elems_b), 1);
		       }
		     );
	      my_nd->n_elems = n_elems_a + n_elems_b;
              MCTOP_SORT_TYPE* tmp = my_nd->source;
              my_nd->source = my_nd->destination;
              my_nd->destination = tmp;
            }

	  MCTOP_P_STEP_ND("   cross-socket merge", my_node, __steps, __a, __b, !my_merge_id);
	}
      else
	{
	  if (mctop_alloc_thread_is_node_leader())
	    {
	      MSD_DO( printf("Node %d. No work for node @ lvl %d!\n", my_node, l););
	    }
	  return;
	}

      // mctop_node_tree_barrier_wait(nt, l);
    }
}

