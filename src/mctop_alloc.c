#include <mctop.h>
#include <darray.h>

#define MA_DP printf

static int
mctop_find_double_max(double* arr, const uint n)
{
  if (arr != NULL)
    {
      double max = arr[0];
      uint max_i = 0;
      for (int i = 1; i < n; i++)
	{
	  if (arr[i] > (1.1 * max))
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

/* smt_first < 0  : only physical cores / hwc : shows which HWC to return from each core */
/* smt_first = 0  : physical cores first */
/* smt_first > 0  : all smt thread of a core first */
static uint
mctop_socket_get_hwc_ids(socket_t* socket, uint* hwc_ids, const int smt_first, const uint hwc)
{
  uint idx = 0;
  MA_DP("-- Socket #%u getting (smt %d): ", socket->id, smt_first);
  
  if (!socket->topo->is_smt) /* only physical cores by design */
    {
      for (int i = 0; i < socket->n_hwcs; i++)
	{
	  hwc_ids[idx] = socket->hwcs[hwc]->id;
	  MA_DP("%-2u ", hwc_ids[idx]);
	  idx++;
	}
    }
  else if (smt_first < 0)	/* only physical cores */
    {
      hwc_gs_t* gs = mctop_socket_get_first_gs_core(socket);
      for (int i = 0; i < socket->n_cores; i++)
	{
	  hwc_ids[idx] = gs->hwcs[hwc]->id;
	  MA_DP("%-2u ", hwc_ids[idx]);
	  idx++;
	  gs = gs->next;
	}
    }
  else if (smt_first == 0)	/* physical cores first */
    {
      for (int ht = 0; ht < socket->topo->n_hwcs_per_core; ht++)
	{
	  hwc_gs_t* gs = mctop_socket_get_first_gs_core(socket);
	  for (int i = 0; i < socket->n_cores; i++)
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
      for (int i = 0; i < socket->n_cores; i++)
	{
	  for (int ht = 0; ht < socket->topo->n_hwcs_per_core; ht++)
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
static void
mctop_alloc_prep_min_lat(mctop_alloc_t* alloc, int smt_first)
{
  mctop_t* topo = alloc->topo;
  uint alloc_full[topo->n_hwcs];

  uint socket_i = mctop_find_double_max(topo->mem_bandwidths, topo->n_sockets);
  socket_t* socket = &topo->sockets[socket_i];
  socket_t* socket_start = socket;
  darray_t* sockets = darray_create();

  uint max_lat = topo->latencies[topo->socket_level];
  double min_bw = 1e9;

  uint hwc_i = 0, sibling_i = 0;
  do
    {
      darray_add(sockets, (uintptr_t) socket);
      hwc_i += mctop_socket_get_hwc_ids(socket, alloc_full + hwc_i, smt_first, 0);
      if (hwc_i >= alloc->n_hwcs)
	{
	  break;
	}

      if (sibling_i >= socket_start->n_siblings)
	{
	  break;
	}
      socket = mctop_sibling_get_other_socket(socket_start->siblings[sibling_i], socket_start);
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
	      hwc_i += mctop_socket_get_hwc_ids(socket, alloc_full + hwc_i, smt_first, hwc);
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

mctop_alloc_t*
mctop_alloc_create(mctop_t* topo, const uint n_hwcs, mctop_alloc_policy policy)
{
  mctop_alloc_t* alloc = malloc(sizeof(mctop_alloc_t) + (n_hwcs * sizeof(uint)));
  alloc->topo = topo;
  alloc->n_hwcs = n_hwcs;
  if (n_hwcs > topo->n_hwcs)
    {
      fprintf(stderr, "MCTOP Warning: Asking for %u hw contexts. This processor has %u contexts.\n",
	      n_hwcs, topo->n_hwcs);
      alloc->n_hwcs = topo->n_hwcs;
    }
  alloc->policy = policy;
  alloc->cur = 0;
  switch (policy)
    {
    case MCTOP_ALLOC_MIN_LAT:
      mctop_alloc_prep_min_lat(alloc, 1);
      break;
    case MCTOP_ALLOC_MIN_LAT_CORES_HWCS:
      mctop_alloc_prep_min_lat(alloc, 0);
      break;
    case MCTOP_ALLOC_MIN_LAT_CORES:
      mctop_alloc_prep_min_lat(alloc, -1);
      break;
    }
  return alloc;
}

void
mctop_alloc_print(mctop_alloc_t* alloc)
{
  printf("#### MCTOP Allocator\n");
  printf("## Policy          : %s\n", mctop_alloc_policy_desc[alloc->policy]);
  printf("## HW Contexts     : %u\n", alloc->n_hwcs);
  printf("## Max latency     : %-5u cycles\n", alloc->max_latency);
  printf("## Min bandwidth   : %-5.2f GB/s\n", alloc->min_bandwidth);
  printf("## Sockets         : ");
  for (int i = 0; i < alloc->n_sockets; i++)
    {
      printf("%u ", alloc->sockets[i]->id);
    }
  printf("\n");
  printf("## HW contexts     : ");
  for (int i = 0; i < alloc->n_hwcs; i++)
    {
      printf("%u ", alloc->hwcs[i]);
    }
  printf("\n");
}

void
mctop_alloc_free(mctop_alloc_t* alloc)
{
  free(alloc);
}


static __thread int __mctop_hwc_id = -1;

inline int
mctop_alloc_get_hwc_id()
{
  return __mctop_hwc_id;
}

#ifdef __sparc__		/* SPARC */
#  include <atomic.h>
#  define CAS_U64(a,b,c) atomic_cas_64(a,b,c)
#  define FAI_U32(a) (atomic_inc_32_nv(a) - 1)
#elif defined(__tile__)		/* TILER */
#  include <arch/atomic.h>
#  include <arch/cycle.h>
#  define FAI_U32(a) arch_atomic_increment(a)
#elif __x86_64__
#  define FAI_U32(a) __sync_fetch_and_add(a, 1)
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
  uint hwcid = FAI_U32(&alloc->cur);
  if (hwcid < alloc->n_hwcs)
    {
      __mctop_hwc_id = hwcid;
      hwcid = alloc->hwcs[hwcid];
      int ret = mctop_set_cpu(hwcid);
      mctop_hwcid_fix_numa_node(alloc->topo, hwcid);
      return ret;
    }
  return 0;
}
