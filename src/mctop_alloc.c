#include <mctop.h>
#include <darray.h>

#define MA_DP printf

const double mctop_bw_sensitivity = 1.05;

static int
mctop_find_double_max(double* arr, const uint n)
{
  if (arr != NULL)
    {
      double max = arr[0];
      uint max_i = 0;
      for (int i = 1; i < n; i++)
	{
	  if (arr[i] > (mctop_bw_sensitivity * max))
	    {
	      max = arr[i];
	      max_i = i;
	    }
	}
      MA_DP("-- mctop_find_double_max: choosing %d with bw %f GB/s\n", max_i, max);
      return max_i;
    }
  return 0;
}

/* sort an array of double, but do not modify the actual array. Instead, return
 an array with the indexes corresponding to the sorted double array. */
static uint*
mctop_sort_double_index(const double* arr, const uint n)
{
  uint* sorted = malloc_assert(n * sizeof(uint));
  for (uint i = 0; i < n; i++)
    {
      sorted[i] = i;
    }
  if (arr != NULL)
    {
      for (uint i = 0; i < n; i++)
	{
	  for (uint j = i + 1; j < n; j++)
	    {
	      if (arr[sorted[j]] > (mctop_bw_sensitivity * arr[sorted[i]]))
		{
		  uint tmp = sorted[j];
		  sorted[j] = sorted[i];
		  sorted[i] = tmp;
		}
	    }
	}
    }
  return sorted;
}

/* smt_first < 0  : only physical cores / hwc : shows which HWC to return from each core */
/* smt_first = 0  : physical cores first */
/* smt_first > 0  : all smt thread of a core first */
static uint
mctop_socket_get_hwc_ids(socket_t* socket, const uint n_hwcs, uint* hwc_ids, const int smt_first, const uint hwc)
{
  uint idx = 0;
  MA_DP("-- Socket #%u getting (smt %d): ", socket->id, smt_first);
  
  if (!socket->topo->is_smt) /* only physical cores by design */
    {
      for (int i = 0; idx < n_hwcs && i < socket->n_hwcs; i++)
	{
	  hwc_ids[idx] = socket->hwcs[idx]->id;
	  MA_DP("%-2u ", hwc_ids[idx]);
	  idx++;
	}
    }
  else if (smt_first < 0)	/* only physical cores */
    {
      hwc_gs_t* gs = mctop_socket_get_first_gs_core(socket);
      for (int i = 0; idx < n_hwcs && i < socket->n_cores; i++)
	{
	  hwc_ids[idx] = gs->hwcs[hwc]->id;
	  MA_DP("%-2u ", hwc_ids[idx]);
	  idx++;
	  gs = gs->next;
	}
    }
  else if (smt_first == 0)	/* physical cores first */
    {
      for (int ht = 0; idx < n_hwcs && ht < socket->topo->n_hwcs_per_core; ht++)
	{
	  hwc_gs_t* gs = mctop_socket_get_first_gs_core(socket);
	  for (int i = 0; idx < n_hwcs && i < socket->n_cores; i++)
	    {
	      hwc_ids[idx] = gs->hwcs[ht]->id;
	      MA_DP("%-2u ", hwc_ids[idx]);
	      idx++;
	      gs = gs->next;
	    }
	}
    }
  else				/* HWCs first */
    {
      hwc_gs_t* gs = mctop_socket_get_first_gs_core(socket);
      for (int i = 0; idx < n_hwcs && i < socket->n_cores; i++)
	{
	  for (int ht = 0; idx < n_hwcs && ht < socket->topo->n_hwcs_per_core; ht++)
	    {
	      hwc_ids[idx] = gs->hwcs[ht]->id;
	      MA_DP("%-2u ", hwc_ids[idx]);
	      idx++;
	    }
	  gs = gs->next;
	}
    }
  MA_DP("\n");

  return idx;
}

/* smt_first < 0  : only physical cores */
/* smt_first = 0  : physical cores first */
/* smt_first > 0  : all smt thread of a core first */
/* n_hwcs_per_socket == MCTOP_ALLOC_ALL : all per-socket according to policy*/
static void
mctop_alloc_prep_min_lat(mctop_alloc_t* alloc, int n_hwcs_per_socket, int smt_first)
{
  mctop_t* topo = alloc->topo;
  uint alloc_full[topo->n_hwcs];

  uint socket_i = mctop_find_double_max(topo->mem_bandwidths, topo->n_sockets);
  socket_t* socket = &topo->sockets[socket_i];
  socket_t* socket_start = socket;
  darray_t* sockets = darray_create();

  if (n_hwcs_per_socket == MCTOP_ALLOC_ALL || n_hwcs_per_socket > socket->n_hwcs || n_hwcs_per_socket < 0)
    {
      n_hwcs_per_socket = socket->n_hwcs;
    }      

  uint max_lat = topo->latencies[topo->socket_level];
  double min_bw = mctop_socket_get_bw_local(socket);

  uint hwc_i = 0, sibling_i = 0;
  do
    {
      darray_add(sockets, (uintptr_t) socket);
      hwc_i += mctop_socket_get_hwc_ids(socket, n_hwcs_per_socket, alloc_full + hwc_i, smt_first, 0);
      if (hwc_i >= alloc->n_hwcs)
	{
	  break;
	}

      if (sibling_i >= socket_start->n_siblings)
	{
	  break;
	}

      if (socket_start->siblings_in != NULL) /* sorted by incoming to socket_start bw */
	{
	  socket = mctop_sibling_get_other_socket(socket_start->siblings_in[sibling_i], socket_start);
	}
      else
	{
	  socket = mctop_sibling_get_other_socket(socket_start->siblings[sibling_i], socket_start);
	}

      uint lat = socket_start->siblings[sibling_i]->latency;
      if (lat > max_lat)
	{
	  max_lat = lat;
	}

      double bw = socket->mem_bandwidths[socket_start->local_node];
      if (bw < min_bw)
	{
	  min_bw = bw;
	}
      MA_DP("-- -> Lat %u / BW %f to %u\n",lat, bw, socket_start->id);
      sibling_i++;
    }
  while (1);

  if (smt_first < 0 && topo->is_smt) /* have gotten ONLY physical cores so far */
    {
      for (int hwc = 1; (hwc_i < alloc->n_hwcs) && (hwc < topo->n_hwcs_per_core); hwc++)
	{
	  MA_DP("---- Now getting the #%u HWCs of cores\n", hwc);
	  DARRAY_FOR_EACH(sockets, i)
	    {
	      socket_t* socket = (socket_t*) DARRAY_GET_N(sockets, i);
	      hwc_i += mctop_socket_get_hwc_ids(socket, n_hwcs_per_socket, alloc_full + hwc_i, smt_first, hwc);
	    }
	}
    }

  for (int i = 0; i < alloc->n_hwcs; i++)
    {
      alloc->hwcs[i] = alloc_full[i];
    }
  
  alloc->max_latency = max_lat;
  alloc->min_bandwidth = min_bw;

  alloc->n_sockets = darray_get_num_elems(sockets);
  alloc->sockets = malloc_assert(alloc->n_sockets * sizeof(socket_t*));
  DARRAY_FOR_EACH(sockets, i)
    {
      alloc->sockets[i] = (socket_t*) DARRAY_GET_N(sockets, i);
    }
  darray_free(sockets);
}

void
mctop_alloc_prep_bw_bound(mctop_alloc_t* alloc, const uint n_hwcs_extra, int n_sockets)
{
  mctop_t* topo = alloc->topo;
  uint alloc_full[topo->n_hwcs];

  uint* sockets_bw = mctop_sort_double_index(topo->mem_bandwidths, topo->n_sockets);
  if (n_sockets == MCTOP_ALLOC_ALL || n_sockets > topo->n_sockets || n_sockets < 0)
    {
      n_sockets = topo->n_sockets;
    }      

  alloc->n_sockets = n_sockets;
  alloc->sockets = malloc_assert(alloc->n_sockets * sizeof(socket_t*));

  double min_bw = 1e9;

  uint hwc_i = 0;
  for (uint socket_n = 0; socket_n < n_sockets; socket_n++)
    {
      socket_t* socket = &topo->sockets[sockets_bw[socket_n]];
      alloc->sockets[socket_n] = socket;

      uint n_hwcs = n_hwcs_extra + (0.5 + (mctop_socket_get_bw_local(socket) / mctop_socket_get_bw_local_one(socket)));
      if (n_hwcs > socket->n_hwcs)
	{
	  n_hwcs = socket->n_hwcs;
	}
      MA_DP("-- Socket #%u : bw %5.2f / bw1 %5.2f --> %u (+%u extra)\n",
	    socket->id,
	    mctop_socket_get_bw_local(socket),
	    mctop_socket_get_bw_local_one(socket),
	    n_hwcs, n_hwcs_extra);

      hwc_i += mctop_socket_get_hwc_ids(socket, n_hwcs, alloc_full + hwc_i, 0, 0);
      min_bw = mctop_socket_get_bw_local(socket);
    }
  
  alloc->n_hwcs = hwc_i;
  alloc->hwcs = malloc_assert(alloc->n_hwcs * sizeof(uint));
  alloc->hwcs_used = calloc_assert(alloc->n_hwcs, sizeof(uint8_t));
  for (int i = 0; i < alloc->n_hwcs; i++)
    {
      alloc->hwcs[i] = alloc_full[i];
    }
  
  uint max_lat = topo->latencies[topo->socket_level];
  for (int i = 0; i < n_sockets; i++)
    {
      for (int j = i + 1; j < n_sockets; j++)
	{
	  uint lat = mctop_ids_get_latency(topo, alloc->sockets[i]->id, alloc->sockets[j]->id);
	  if (lat > max_lat)
	    {
	      max_lat = lat;
	    }
	}
    }

  alloc->max_latency = max_lat;
  alloc->min_bandwidth = min_bw;

  free(sockets_bw);
}

/* n_config = num hwcs per socket for MIN_LAT policies*/
/* n_config = num sockets for BW_BOUND policies*/
mctop_alloc_t*
mctop_alloc_create(mctop_t* topo, const uint n_hwcs, const int n_config, mctop_alloc_policy policy)
{
  mctop_alloc_t* alloc = malloc_assert(sizeof(mctop_alloc_t));
  if (policy <= MCTOP_ALLOC_MIN_LAT_CORES)
    {
      alloc->hwcs = malloc_assert(n_hwcs * sizeof(uint));
      alloc->hwcs_used = calloc_assert(n_hwcs, sizeof(uint8_t));
    }

  alloc->topo = topo;
  alloc->n_hwcs = n_hwcs;
  alloc->n_hwcs_used = 0;
  if (n_hwcs > topo->n_hwcs)
    {
      fprintf(stderr, "MCTOP Warning: Asking for %u hw contexts. This processor has %u contexts.\n",
	      n_hwcs, topo->n_hwcs);
      alloc->n_hwcs = topo->n_hwcs;
    }
  alloc->policy = policy;

  alloc->hwcs_all = numa_bitmask_alloc(topo->n_hwcs);
  for (int i = 0; i < topo->n_hwcs; i++) 
    {
      alloc->hwcs_all = numa_bitmask_setbit(alloc->hwcs_all, topo->hwcs[i].id);
    }

  switch (policy)
    {
    case MCTOP_ALLOC_MIN_LAT_HWCS:
      mctop_alloc_prep_min_lat(alloc, n_config, 1);
      break;
    case MCTOP_ALLOC_MIN_LAT_CORES_HWCS:
      mctop_alloc_prep_min_lat(alloc, n_config, 0);
      break;
    case MCTOP_ALLOC_MIN_LAT_CORES:
      mctop_alloc_prep_min_lat(alloc, n_config, -1);
      break;
    case MCTOP_ALLOC_BW_BOUND:
      mctop_alloc_prep_bw_bound(alloc, n_hwcs, n_config);
      break;
    }
  return alloc;
}

void
mctop_alloc_print(mctop_alloc_t* alloc)
{
  printf("#### MCTOP Allocator\n");
  printf("## Policy            : %s\n", mctop_alloc_policy_desc[alloc->policy]);
  printf("## Sockets (%-3u)     : ", alloc->n_sockets);
  for (int i = 0; i < alloc->n_sockets; i++)
    {
      printf("%u ", alloc->sockets[i]->id);
    }
  printf("\n");
  printf("## HW Contexts (%-3u) : ", alloc->n_hwcs);
  for (int i = 0; i < alloc->n_hwcs; i++)
    {
      printf("%u ", alloc->hwcs[i]);
    }
  printf("\n");
  printf("## Max latency       : %-5u cycles\n", alloc->max_latency);
  printf("## Min bandwidth     : %-5.2f GB/s\n", alloc->min_bandwidth);
}

void
mctop_alloc_free(mctop_alloc_t* alloc)
{
  free(alloc->hwcs);
  free((void*) alloc->hwcs_used);
  free(alloc->hwcs_all);
  free(alloc->sockets);
  free(alloc);
}


/* ******************************************************************************** */
/* pinning */
/* ******************************************************************************** */

static __thread mctop_thread_info_t __mctop_thread_info = { .id = -1 };

#ifdef __sparc__		/* SPARC */
#  include <atomic.h>
#  define CAS_U8(a,b,c) atomic_cas_8(a,b,c)
#  define CAS_U64(a,b,c) atomic_cas_64(a,b,c)
#  define FAI_U32(a) (atomic_inc_32_nv(a) - 1)
#  define DAF_U32(a) atomic_dec_32_nv((volatile uint32_t*) a)
#elif defined(__tile__)		/* TILER */
#  include <arch/atomic.h>
#  include <arch/cycle.h>
#  define CAS_U8(a,b,c)  arch_atomic_val_compare_and_exchange(a,b,c)
#  define CAS_U64(a,b,c) arch_atomic_val_compare_and_exchange(a,b,c)
#  define FAI_U32(a) arch_atomic_increment(a)
#  define DAF_U32(a) (arch_atomic_decrement(a) - 1)
#elif __x86_64__
#  define CAS_U8(a,b,c) __sync_val_compare_and_swap(a,b,c)
#  define CAS_U64(a,b,c) __sync_val_compare_and_swap(a,b,c)
#  define FAI_U32(a) __sync_fetch_and_add(a, 1)
#  define DAF_U32(a) __sync_sub_and_fetch(a, 1)
#else
#  error "Unsupported Architecture"
#endif

/* pin to ALL hw contexts contained in alloc */
int
mctop_alloc_pin_all(mctop_alloc_t* alloc)
{
  struct bitmask* bmask = numa_bitmask_alloc(alloc->topo->n_hwcs);
  for (int i = 0; i < alloc->n_hwcs; i++)
    {
      bmask = numa_bitmask_setbit(bmask, alloc->hwcs[i]);
    }

  int ret = numa_sched_setaffinity(0, bmask);
  numa_bitmask_free(bmask);
  return ret;
}


/* pin to ONE hw context contained in alloc */
int
mctop_alloc_pin(mctop_alloc_t* alloc)
{
  __mctop_thread_info.alloc = alloc;
  while (alloc->n_hwcs_used < alloc->n_hwcs)
    {
      for (uint i = 0; i < alloc->n_hwcs; i++)
	{
	  if (alloc->hwcs_used[i] == 0)
	    {
	      if (CAS_U8(&alloc->hwcs_used[i], 0, 1) == 0)
		{
		  uint hwcid = i;
		  FAI_U32(&alloc->n_hwcs_used);
		  alloc->hwcs_used[hwcid] = 1;
		  __mctop_thread_info.id = hwcid;
		  hwcid = alloc->hwcs[hwcid];
		  __mctop_thread_info.hwc_id = hwcid;
		  int ret = mctop_set_cpu(hwcid);
		  mctop_hwcid_fix_numa_node(alloc->topo, hwcid);
		  __mctop_thread_info.local_node = mctop_hwcid_get_local_node(alloc->topo, hwcid);
		  return ret;
 		}
	    }
	}
    }
  return 0;
}

int
mctop_alloc_unpin()
{
  mctop_alloc_t* alloc = __mctop_thread_info.alloc;
  if (mctop_alloc_is_pinned())
    {
      DAF_U32(&alloc->n_hwcs_used);
      alloc->hwcs_used[mctop_alloc_get_id()] = 0;
      int ret = numa_sched_setaffinity(0, alloc->hwcs_all);
      __mctop_thread_info.id = -1;
      __asm volatile ("mfence");
      return ret;
    }
  return 1;
}

void
mctop_alloc_thread_print()
{
  if (mctop_alloc_is_pinned())
    {
      printf("[MCTOP ALLOC]     pinned : id %-3d / hwc id %-3u / node %-3u\n",
	     mctop_alloc_get_id(),
	     mctop_alloc_get_hwc_id(),
	     mctop_alloc_get_local_node());
    }
  else
    {
      printf("[MCTOP ALLOC] NOT pinned : id --- / hwc id --- / node ---\n");
    }
}

/* ******************************************************************************** */
/* queries */
/* ******************************************************************************** */

inline mctop_alloc_policy
mctop_alloc_get_policy(mctop_alloc_t* alloc)
{
  return alloc->policy;
}

inline uint
mctop_alloc_get_num_hw_contexts(mctop_alloc_t* alloc)
{
  return alloc->n_hwcs;
}

const char* 
mctop_alloc_get_policy_desc(mctop_alloc_t* alloc)
{
  return mctop_alloc_policy_desc[alloc->policy];
}

inline double
mctop_alloc_get_min_bandwidth(mctop_alloc_t* alloc)
{
  return alloc->min_bandwidth;
}

inline uint
mctop_alloc_get_max_latency(mctop_alloc_t* alloc)
{
  return alloc->max_latency;
}

inline uint
mctop_alloc_get_num_sockets(mctop_alloc_t* alloc)
{
  return alloc->n_sockets;
}

inline uint
mctop_alloc_get_nth_hw_context(mctop_alloc_t* alloc, const uint nth)
{
  return alloc->hwcs[nth];
}

inline uint
mctop_alloc_ids_get_latency(mctop_alloc_t* alloc, const uint id0, const uint id1)
{
  return mctop_ids_get_latency(alloc->topo, id0, id1);
}


/* thread ******************************************************************************** */

uint
mctop_alloc_is_pinned()
{
  return __mctop_thread_info.id >= 0;
}

int
mctop_alloc_get_id()
{
  return __mctop_thread_info.id;
}

int
mctop_alloc_get_hwc_id()
{
  assert(mctop_alloc_is_pinned());
  return __mctop_thread_info.hwc_id;
}

int
mctop_alloc_get_local_node()
{
  assert(mctop_alloc_is_pinned());
  return __mctop_thread_info.local_node;
}
