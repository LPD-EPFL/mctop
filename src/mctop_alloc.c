#include <mctop_alloc.h>
#include <mctop_internal.h>
#include <darray.h>

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

//#define MA_DP(args...) printf(args)
#define MA_DP(args...) //printf(args)

void
mctop_alloc_help()
{
  printf("## MCTOP Allocator available polices:\n");
  for (int i = 0; i < MCTOP_ALLOC_NUM; i++)
    {
      printf("%-2d - %s\n", i, mctop_alloc_policy_desc[i]);
    }
}


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

/* smt_first <= 0  : physical cores first (i.e., c0_hwc0, c1_hwc0, ..., c1_hwc1, ...) */
/* smt_first > 0  : smt hw contexts first (i.e., c0_hwc0, c0_hwc1, c1_hwc0, c1_hwc1, ...) */
static uint
mctop_socket_get_nth_hwc_id(socket_t* socket, const uint nth_hwc, const int smt_first)
{
  hw_context_t* hwc;
  if (!socket->topo->is_smt)
    {
      hwc = mctop_socket_get_nth_hwc(socket, nth_hwc);
    }
  else if (smt_first > 0)
    {
      uint core = nth_hwc / socket->topo->n_hwcs_per_core;
      uint hwc_i = nth_hwc % socket->topo->n_hwcs_per_core;
      hwc_gs_t* gs = mctop_socket_get_nth_gs_core(socket, core);
      hwc = gs->hwcs[hwc_i];
    }
  else
    {
      uint core = nth_hwc % socket->n_cores;
      uint hwc_i = nth_hwc / socket->n_cores;
      hwc_gs_t* gs = mctop_socket_get_nth_gs_core(socket, core);
      hwc = gs->hwcs[hwc_i];
    }

  return hwc->id;
}

static void
mctop_alloc_fix_max_lat_bw_proportions(mctop_alloc_t* alloc, double tot_bw)
{
  mctop_t* topo = alloc->topo;
  uint max_lat = topo->latencies[topo->socket_level];
  for (int i = 0; i < alloc->n_sockets; i++)
    {
      double bw = mctop_socket_get_bw_local(alloc->sockets[i]);
      alloc->bw_proportions[i] = bw / tot_bw;
      for (int j = i + 1; j < alloc->n_sockets; j++)
	{
	  uint lat = mctop_ids_get_latency(topo, alloc->sockets[i]->id, alloc->sockets[j]->id);
	  if (lat > max_lat)
	    {
	      max_lat = lat;
	    }
	}
    }

  alloc->max_latency = max_lat;
}

static void
mctop_alloc_prep_sequential(mctop_alloc_t* alloc)
{
  mctop_t* topo = alloc->topo;
  darray_t* sockets = darray_create();
  darray_t* bws = darray_create();

  double min_bw = 1e9, tot_bw = 0;

  for (uint hwc_i = 0; hwc_i < alloc->n_hwcs; hwc_i++)
    {
      alloc->hwcs[hwc_i] = hwc_i;
      socket_t* socket = mctop_hwcid_get_socket(topo, hwc_i);
      if (darray_add_uniq(sockets, (uintptr_t) socket))
	{
	  double bw = mctop_socket_get_bw_local(socket);
	  tot_bw += bw;
	  if (bw < min_bw)
	    {
	      min_bw = bw;
	    }
	}
    }

  alloc->n_sockets = darray_get_num_elems(sockets);
  alloc->sockets = malloc_assert(alloc->n_sockets * sizeof(socket_t*));
  alloc->bw_proportions = malloc_assert(alloc->n_sockets * sizeof(socket_t*));
  DARRAY_FOR_EACH(sockets, i)
    {
      alloc->sockets[i] = (socket_t*) DARRAY_GET_N(sockets, i);
    }

  mctop_alloc_fix_max_lat_bw_proportions(alloc, tot_bw);
  alloc->min_bandwidth = min_bw;

  darray_free(sockets);
  darray_free(bws);
}

/* smt_first < 0  : only physical cores */
/* smt_first = 0  : physical cores first */
/* smt_first > 0  : all smt thread of a core first */
/* n_hwcs_per_socket == MCTOP_ALLOC_ALL : all per-socket according to policy*/
/* balance : balance or not the threads on sockets, overrides n_hwcs_per_socket */
static void
mctop_alloc_prep_min_lat(mctop_alloc_t* alloc, int n_hwcs_per_socket, int smt_first, const uint balance)
{
  mctop_t* topo = alloc->topo;
  uint alloc_full[topo->n_hwcs];

  uint socket_i = mctop_find_double_max(topo->mem_bandwidths_r, topo->n_sockets);
  socket_t* socket = &topo->sockets[socket_i];
  socket_t* socket_start = socket;
  darray_t* sockets = darray_create();
  darray_t* bws = darray_create();

  if (n_hwcs_per_socket == MCTOP_ALLOC_ALL || n_hwcs_per_socket > socket->n_hwcs || n_hwcs_per_socket <= 0)
    {
      n_hwcs_per_socket = socket->n_hwcs;
    }

  if (balance)
    {
      uint n_hwcs_avail = socket->n_hwcs;
      if (smt_first < 0)
	{
	  n_hwcs_avail /= topo->n_hwcs_per_core;
	}
      uint n_sockets = (alloc->n_hwcs / n_hwcs_avail) + ((alloc->n_hwcs % n_hwcs_avail) > 0);
      if (n_sockets > topo->n_sockets && smt_first < 0)
	{
	  n_sockets = topo->n_sockets;
	}
      n_hwcs_per_socket = (alloc->n_hwcs / n_sockets) + ((alloc->n_hwcs % n_sockets) > 0);

      /* printf("Balancing! %u hwcs, %u hwcs avail per socket -- Need #Sockets: %u, Put %u per socket\n", */
      /* 	     alloc->n_hwcs, n_hwcs_avail, n_sockets, n_hwcs_per_socket); */
    }

  const int n_hwcs = alloc->n_hwcs;
  if (unlikely((n_hwcs_per_socket * topo->n_sockets) < n_hwcs))
    {
      alloc->n_hwcs = n_hwcs_per_socket * topo->n_sockets;
      fprintf(stderr, "MCTOP Warning: Asking for %u hw contexts, %u per socket. Returning %u contexts!\n",
	      n_hwcs, n_hwcs_per_socket, alloc->n_hwcs);
    }

  uint max_lat = topo->latencies[topo->socket_level];
  double min_bw = mctop_socket_get_bw_local(socket), tot_bw = min_bw;
  darray_add_double(bws, min_bw);

  uint hwc_i = 0, sibling_i = 0;
  do
    {
      darray_add(sockets, (uintptr_t) socket);
      hwc_i += mctop_socket_get_hwc_ids(socket, n_hwcs_per_socket, alloc_full + hwc_i, smt_first, 0);
      if (hwc_i >= alloc->n_hwcs || sibling_i >= socket_start->n_siblings)
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

      double bw = socket->mem_bandwidths_r[socket_start->local_node];
      tot_bw += bw;
      darray_add_double(bws, bw);
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
      if (balance == 1)
	{
	  n_hwcs_per_socket = (alloc->n_hwcs - hwc_i) / darray_get_num_elems(sockets) || 1;
	}
      for (int hwc = 1; (hwc_i < alloc->n_hwcs) && (hwc < topo->n_hwcs_per_core); hwc++)
	{
	  MA_DP("---- Now getting the #%u HWCs of cores -- %u per socket\n", hwc, n_hwcs_per_socket);
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
  alloc->bw_proportions = malloc_assert(alloc->n_sockets * sizeof(socket_t*));
  DARRAY_FOR_EACH(sockets, i)
    {
      alloc->sockets[i] = (socket_t*) DARRAY_GET_N(sockets, i);
      alloc->bw_proportions[i] = DARRAY_GET_N_DOUBLE(bws, i) / tot_bw;
    }
  darray_free(sockets);
  darray_free(bws);
}

static void
mctop_alloc_prep_bw_round_robin(mctop_alloc_t* alloc, int n_sockets, const int smt_first)
 {
  mctop_t* topo = alloc->topo;
  uint alloc_full[topo->n_hwcs];

  uint* sockets_bw = mctop_sort_double_index(topo->mem_bandwidths_r, topo->n_sockets);
  if (n_sockets == MCTOP_ALLOC_ALL || n_sockets > topo->n_sockets || n_sockets <= 0)
    {
      n_sockets = topo->n_sockets;
    }

  if (n_sockets > alloc->n_hwcs)
    {
      n_sockets = alloc->n_hwcs;
    }

  alloc->n_sockets = n_sockets;
  alloc->sockets = malloc_assert(alloc->n_sockets * sizeof(socket_t*));
  alloc->bw_proportions = malloc_assert(alloc->n_sockets * sizeof(socket_t*));

  double min_bw = 1e9, tot_bw = 0;

  uint round = 0, n = 0;
  for (uint hwc_i = 0; hwc_i < alloc->n_hwcs; hwc_i += n_sockets)
    {
      for (uint socket_n = 0; socket_n < n_sockets; socket_n++)
	{
	  socket_t* socket = &topo->sockets[sockets_bw[socket_n]];

	  uint hwcid = mctop_socket_get_nth_hwc_id(socket, round, smt_first);
	  alloc_full[n++] = hwcid;

	  if (hwc_i < n_sockets)
	    {
	      alloc->sockets[socket_n] = socket;
	      min_bw = mctop_socket_get_bw_local(socket);
	      tot_bw += min_bw;
	    }

	  MA_DP("-- Socket #%u : bw %5.2f / bw1 %5.2f --> %u\n",
		socket->id,
		mctop_socket_get_bw_local(socket),
		mctop_socket_get_bw_local_one(socket),
		hwcid);
	}
      round++;
    }
  
  for (int i = 0; i < alloc->n_hwcs; i++)
    {
      alloc->hwcs[i] = alloc_full[i];
    }
  
 
  mctop_alloc_fix_max_lat_bw_proportions(alloc, tot_bw);
  alloc->min_bandwidth = min_bw;

  free(sockets_bw);
}

static void
mctop_alloc_prep_bw_bound(mctop_alloc_t* alloc, const uint n_hwcs_extra, int n_sockets)
{
  mctop_t* topo = alloc->topo;
  uint alloc_full[topo->n_hwcs];

  uint* sockets_bw = mctop_sort_double_index(topo->mem_bandwidths_r, topo->n_sockets);
  if (n_sockets == MCTOP_ALLOC_ALL || n_sockets > topo->n_sockets || n_sockets <= 0)
    {
      n_sockets = topo->n_sockets;
    }      

  alloc->n_sockets = n_sockets;
  alloc->sockets = malloc_assert(alloc->n_sockets * sizeof(socket_t*));
  alloc->bw_proportions = malloc_assert(alloc->n_sockets * sizeof(socket_t*));

  double min_bw = 1e9, tot_bw = 0;

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
      tot_bw += min_bw;
    }
  
  alloc->n_hwcs = hwc_i;
  alloc->hwcs = malloc_assert(alloc->n_hwcs * sizeof(uint));
  alloc->hwcs_used = calloc_assert(alloc->n_hwcs, sizeof(uint8_t));
  for (int i = 0; i < alloc->n_hwcs; i++)
    {
      alloc->hwcs[i] = alloc_full[i];
    }
  
   mctop_alloc_fix_max_lat_bw_proportions(alloc, tot_bw);
   alloc->min_bandwidth = min_bw;

  free(sockets_bw);
}

/* num cores
   hwcs per socket
   cores per socket
*/
static void
mctop_alloc_details_calc(mctop_alloc_t* alloc, uint* n_cores, uint* n_hwcs_socket, uint* n_cores_socket)
{
  darray_t* cores = darray_create();
  for (uint i = 0; i < alloc->n_hwcs; i++)
    {
      const uint hwcid = alloc->hwcs[i];
      socket_t* socket = mctop_hwcid_get_socket(alloc->topo, hwcid);
      const uint nth_socket = mctop_alloc_socket_seq_id(alloc, socket->id);
      n_hwcs_socket[nth_socket]++;

      hwc_gs_t* core = mctop_hwcid_get_core(alloc->topo, hwcid);
      if (darray_add_uniq(cores, core->id))
	{
	  n_cores_socket[nth_socket]++;
	}
    }
  uint n = darray_get_num_elems(cores);
  darray_free(cores);
  *n_cores = n;
}

static mctop_alloc_t*
mctop_alloc_create_config(mctop_t* topo, const int n_hwcs, const int n_config, mctop_alloc_policy policy,
			  const uint do_more)
{
  mctop_alloc_t* alloc = calloc_assert(1, sizeof(mctop_alloc_t));
  alloc->topo = topo;
  alloc->n_hwcs = n_hwcs;
  alloc->n_hwcs_used = 0;
  if (n_hwcs <= 0)
    {
      alloc->n_hwcs = topo->n_hwcs;
    }
  else if (((int) n_hwcs) > topo->n_hwcs)
    {
      fprintf(stderr, "MCTOP Warning: Asking for %u hw contexts. This processor has %u contexts.\n",
	      n_hwcs, topo->n_hwcs);
      alloc->n_hwcs = topo->n_hwcs;
    }
  

  if (policy <= MCTOP_ALLOC_BW_ROUND_ROBIN_CORES)
    {
      alloc->hwcs = malloc_assert(alloc->n_hwcs * sizeof(uint));
      alloc->hwcs_used = calloc_assert(alloc->n_hwcs, sizeof(uint8_t));
    }

  alloc->policy = policy;

#ifdef __x86_64__
  alloc->hwcs_all = numa_bitmask_alloc(topo->n_hwcs);
  for (int i = 0; i < topo->n_hwcs; i++) 
    {
      alloc->hwcs_all = numa_bitmask_setbit(alloc->hwcs_all, topo->hwcs[i].id);
    }
#elif defined(__sparc)
  alloc->hwcs_all = lgrp_root(lgrp_cookie);
#endif

  switch (policy)
    {
    case MCTOP_ALLOC_NONE:
      break;
    case MCTOP_ALLOC_SEQUENTIAL:
      mctop_alloc_prep_sequential(alloc);
      break;
    case MCTOP_ALLOC_MIN_LAT_HWCS:
      mctop_alloc_prep_min_lat(alloc, n_config, 1, 0);
      break;
    case MCTOP_ALLOC_MIN_LAT_CORES_HWCS:
      mctop_alloc_prep_min_lat(alloc, n_config, 0, 0);
      break;
    case MCTOP_ALLOC_MIN_LAT_CORES:
      mctop_alloc_prep_min_lat(alloc, n_config, -1, 0);
      break;
    case MCTOP_ALLOC_MIN_LAT_HWCS_BALANCE:
      mctop_alloc_prep_min_lat(alloc, n_config, 1, 1);
      break;
    case MCTOP_ALLOC_MIN_LAT_CORES_HWCS_BALANCE:
      mctop_alloc_prep_min_lat(alloc, n_config, 0, 1);
      break;
    case MCTOP_ALLOC_MIN_LAT_CORES_BALANCE:
      mctop_alloc_prep_min_lat(alloc, n_config, -1, 1);
      break;
    case MCTOP_ALLOC_BW_ROUND_ROBIN_HWCS:
      mctop_alloc_prep_bw_round_robin(alloc, n_config, 1);
      break;
    case MCTOP_ALLOC_BW_ROUND_ROBIN_CORES:
      mctop_alloc_prep_bw_round_robin(alloc, n_config, 0);
      break;
    case MCTOP_ALLOC_BW_BOUND:
      mctop_alloc_prep_bw_bound(alloc, alloc->n_hwcs, n_config);
      break;
    }

  if (do_more)
    {
      alloc->global_barrier = malloc_assert(sizeof(mctop_barrier_t));
      mctop_barrier_init(alloc->global_barrier, alloc->n_hwcs);
    }

  if (do_more && likely(alloc->policy != MCTOP_ALLOC_NONE))
    {
      alloc->core_sids = malloc_assert(alloc->n_hwcs * sizeof(uint));
      darray_t* cores = darray_create();
      for (int h = 0; h < alloc->n_hwcs; h++)
	{
	  hwc_gs_t* core = mctop_hwcid_get_core(topo, alloc->hwcs[h]);
	  uint pos;
	  if (darray_exists_pos(cores, core->id, &pos))
	    {
	      alloc->core_sids[h] = pos;
	    }
	  else
	    {
	      alloc->core_sids[h] = darray_get_num_elems(cores);
	      darray_add(cores, core->id);
	    }

	}
      darray_free(cores);


      alloc->n_hwcs_per_socket = calloc_assert(topo->n_sockets, sizeof(uint));
      alloc->n_cores_per_socket = calloc_assert(topo->n_sockets, sizeof(uint));
      mctop_alloc_details_calc(alloc, &alloc->n_cores, alloc->n_hwcs_per_socket, alloc->n_cores_per_socket);

      alloc->socket_barriers = malloc_assert(topo->n_sockets * sizeof(mctop_barrier_t*));
      alloc->socket_barriers_cores = malloc_assert(topo->n_sockets * sizeof(mctop_barrier_t*));
      
      alloc->node_to_nth_socket = calloc_assert(topo->n_sockets, sizeof(uint));
      for (int i = 0; i < alloc->n_sockets; i++)
	{
	  alloc->node_to_nth_socket[i] = alloc->sockets[i]->local_node;
	  alloc->socket_barriers[i] = numa_alloc_onnode(sizeof(mctop_barrier_t),
							alloc->sockets[i]->local_node);
	  mctop_barrier_init(alloc->socket_barriers[i], alloc->n_hwcs_per_socket[i]);
	  alloc->socket_barriers_cores[i] = numa_alloc_onnode(sizeof(mctop_barrier_t),
							      alloc->sockets[i]->local_node);
	  mctop_barrier_init(alloc->socket_barriers_cores[i], alloc->n_cores_per_socket[i]);
	}
    }
 
  return alloc;
}


mctop_alloc_t*
mctop_alloc_create(mctop_t* topo, const int n_hwcs, const int n_config, mctop_alloc_policy policy)
{
  return mctop_alloc_create_config(topo, n_hwcs, n_config, policy, 1);
}

mctop_alloc_t*
mctop_alloc_create_simple(mctop_t* topo, const int n_hwcs, const int n_config, mctop_alloc_policy policy)
{
  return mctop_alloc_create_config(topo, n_hwcs, n_config, policy, 0);
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
  printf("## # HW Ctx / socket : ");
  for (int i = 0; i < alloc->n_sockets; i++)
    {
      printf("%-5u ", alloc->n_hwcs_per_socket[i]);
    }
  printf("\n");
  printf("## # Cores / socket  : ");
  for (int i = 0; i < alloc->n_sockets; i++)
    {
      printf("%-5u ", alloc->n_cores_per_socket[i]);
    }
  printf("\n");
  if (alloc->bw_proportions != NULL)
    {
      printf("## BW proportions    : ");
      for (int i = 0; i < alloc->n_sockets; i++)
	{
	  printf("%.3f ", alloc->bw_proportions[i]);
	}
      printf(" \n");
    }
  printf("## # Cores           : %u\n", alloc->n_cores);
  printf("## HW Contexts (%-3u) : ", alloc->n_hwcs);
  for (int i = 0; i < alloc->n_hwcs; i++)
    {
      if (unlikely(alloc->policy == MCTOP_ALLOC_NONE))
	{
	  printf("? ");
	}
      else
	{
      printf("%u ", alloc->hwcs[i]);
	}
    }
  printf("\n");
  printf("## Max latency       : %-5u cycles\n", alloc->max_latency);
  printf("## Min bandwidth     : %-5.2f GB/s\n", alloc->min_bandwidth);
}

void
mctop_alloc_print_short(mctop_alloc_t* alloc)
{
  printf("%-33s | #HWCs %-3u | #Cores %-3u | Sockets (%u): ",
	 mctop_alloc_policy_desc[alloc->policy], alloc->n_hwcs, alloc->n_cores, alloc->n_sockets);
  for (int i = 0; i < alloc->n_sockets; i++)
    {
      printf("%u ", alloc->sockets[i]->id);
    }
  printf("\n");
}

void
mctop_alloc_free(mctop_alloc_t* alloc)
{
  free(alloc->hwcs);
  if (alloc->core_sids)
    {
      free(alloc->core_sids);
    }
  free((void*) alloc->hwcs_used);
#ifdef __x86_64__
  free(alloc->hwcs_all);
#endif
  if (alloc->sockets != NULL)
    {
      free(alloc->sockets);
    }
  if (alloc->n_hwcs_per_socket)
    {
      free(alloc->n_hwcs_per_socket);
    }
  if (alloc->n_cores_per_socket)
    {
      free(alloc->n_cores_per_socket);
    }
  if (alloc->node_to_nth_socket != NULL)
    {
      free(alloc->node_to_nth_socket);
    }
  if (alloc->bw_proportions != NULL)
    {
      free(alloc->bw_proportions);
    }

  if (alloc->global_barrier != NULL)
    {
      free(alloc->global_barrier);
    }

  if (alloc->socket_barriers != NULL)
    {
      for (int i = 0; i < alloc->n_sockets; i++)
	{
	  numa_free(alloc->socket_barriers[i], sizeof(mctop_barrier_t));
	}
      free(alloc->socket_barriers);
    }
  if (alloc->socket_barriers_cores != NULL)
    {
      for (int i = 0; i < alloc->n_sockets; i++)
	{
	  numa_free(alloc->socket_barriers_cores[i], sizeof(mctop_barrier_t));
	}
      free(alloc->socket_barriers_cores);
    }
  free(alloc);
}


/* ******************************************************************************** */
/* pinning */
/* ******************************************************************************** */

static __thread mctop_thread_info_t __mctop_thread_info = { .is_pinned = 0, .id = -1, .alloc = NULL };

static inline mctop_thread_info_t*
mctop_thread_get_info()
{
  return &__mctop_thread_info;
}

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


int
mctop_alloc_pin_nth_socket(mctop_alloc_t* alloc, const uint nth)
{
  if (unlikely(nth >= alloc->n_sockets))
    {
      return 0;
    }
  return mctop_run_on_socket_ref(alloc->sockets[nth], 0);
}

/* pin to ALL hw contexts contained in alloc */
int
mctop_alloc_pin_all(mctop_alloc_t* alloc)
{
#if __x86_64__
  int ret = numa_sched_setaffinity(0, alloc->hwcs_all);
#elif defined(__sparc)
  int ret = lgrp_affinity_set(P_LWPID, P_MYID, alloc->hwcs_all, LGRP_AFF_STRONG);
#endif
  return ret;
}

static uint
mctop_alloc_id_get_nth_hwc_in_socket(mctop_alloc_t* alloc, const uint id, const socket_t* socket)
{
  uint n_hwcs_socket = 0;
  for (int i = 0; i < id; i++)
    {
      socket_t* s = mctop_hwcid_get_socket(alloc->topo, alloc->hwcs[i]);
      if (socket == s)
	{
	  n_hwcs_socket++;
	}
    }
  return n_hwcs_socket;
}

static int
mctop_alloc_pin_prepare(mctop_alloc_t* alloc, const uint id)
{
  alloc->hwcs_used[id] = 1;
  __mctop_thread_info.is_pinned = 1;
  __mctop_thread_info.id = id;

  int ret = 0;
  if (likely(alloc->policy != MCTOP_ALLOC_NONE))
    {
      const uint hwcid = alloc->hwcs[id];
      __mctop_thread_info.hwc_id = hwcid;

      ret = mctop_set_cpu(alloc->topo, hwcid);

      __mctop_thread_info.local_node = mctop_hwcid_get_local_node(alloc->topo, hwcid);
      socket_t* socket = mctop_hwcid_get_socket(alloc->topo, hwcid);
      size_t n_hwcs_prev_sockets = 0;
      for (int s = 0; s < alloc->n_sockets; s++)
	{
	  if (alloc->sockets[s] == socket)
	    {
	      __mctop_thread_info.nth_socket = s;
	      break;
	    }
	  n_hwcs_prev_sockets += alloc->n_hwcs_per_socket[s];
	}

      __mctop_thread_info.nth_hwc_in_core = mctop_hwcid_get_nth_hwc_in_core(alloc->topo, hwcid);
      __mctop_thread_info.nth_core_socket = mctop_hwcid_get_nth_core_in_socket(alloc->topo, hwcid);
      __mctop_thread_info.nth_hwc_in_socket = mctop_alloc_id_get_nth_hwc_in_socket(alloc, id, socket);
      __mctop_thread_info.nth_core = alloc->core_sids[id];
    }
  return ret;
}

/* pin to ONE hw context contained in alloc -- Does not support repin() */
int
mctop_alloc_pin(mctop_alloc_t* alloc)
{
  __mctop_thread_info.alloc = alloc;
  const uint id = FAI_U32(&alloc->n_hwcs_used);
  return mctop_alloc_pin_prepare(alloc, id);
}

/* pin to ONE hw context contained in alloc -- Supports repin() */
int
mctop_alloc_pin_plus(mctop_alloc_t* alloc)
{
  __mctop_thread_info.alloc = alloc;
  if (unlikely(mctop_alloc_thread_is_pinned()))
    {
      mctop_alloc_unpin();
    }

  while (alloc->n_hwcs_used < alloc->n_hwcs)
    {
      for (uint i = 0; i < alloc->n_hwcs; i++)
	{
	  if (alloc->hwcs_used[i] == 0)
	    {
	      if (CAS_U8(&alloc->hwcs_used[i], 0, 1) == 0)
		{
		  UNUSED uint a = FAI_U32(&alloc->n_hwcs_used);
		  return mctop_alloc_pin_prepare(alloc, i);
 		}
	    }
	}
    }
  return 0;
}

/* no stats, nothing, just pin! */
int
mctop_alloc_pin_simple(mctop_alloc_t* alloc)
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
		  UNUSED uint a = FAI_U32(&alloc->n_hwcs_used);
		  alloc->hwcs_used[i] = 1;
		  __mctop_thread_info.is_pinned = 1;
		  __mctop_thread_info.id = i;
		  if (likely(alloc->policy != MCTOP_ALLOC_NONE))
		    {
		      const uint hwcid = alloc->hwcs[i];
		      __mctop_thread_info.hwc_id = hwcid;
		      return mctop_set_cpu(NULL, hwcid);
		    }
		  else
		    {
		      return 0;
		    }
 		}
	    }
	}
    }
  return 0;
}

#define MCTOP_ALLOC_PIN_ON_DEBUG    1

/* no stats, nothing, just pin! */
int
mctop_alloc_pin_on(mctop_alloc_t* alloc, const uint on)
{
  __mctop_thread_info.alloc = alloc;

#if MCTOP_ALLOC_PIN_ON_DEBUG == 1
  if (unlikely(on >= alloc->n_hwcs))
    {
      fprintf(stderr, "MCTOP Alloc Pool: Warning trying to pin on %u th hwc. Alloc %p has %u!\n",
	      on, alloc, alloc->n_hwcs);
      return 0;
    }

  if (unlikely(alloc->hwcs_used[on]))
    {
      fprintf(stderr, "MCTOP Alloc Pool: Warning trying to pin on %u th hwc. Already used!\n", on);
    }
#endif  /* MCTOP_ALLOC_PIN_ON_DEBUG == 1 */

  alloc->hwcs_used[on] = 1;
  UNUSED uint a = FAI_U32(&alloc->n_hwcs_used);
  __mctop_thread_info.is_pinned = 1;
  __mctop_thread_info.id = on;

  if (likely(alloc->policy != MCTOP_ALLOC_NONE))
    {
      const uint hwcid = alloc->hwcs[on];
      __mctop_thread_info.hwc_id = hwcid;
      return mctop_set_cpu(NULL, hwcid);
    }
  else
    {
      return 0;
    }
}

static inline int
mctop_alloc_hwctx_release(mctop_alloc_t* alloc)
{
  if (likely(mctop_alloc_thread_is_pinned()))
    {
      DAF_U32(&alloc->n_hwcs_used);
      alloc->hwcs_used[mctop_alloc_thread_id()] = 0;
      __mctop_thread_info.is_pinned = 0;
      return 1;
    }
  return 0;
}


int
mctop_alloc_unpin()
{
  mctop_alloc_t* alloc = __mctop_thread_info.alloc;
  if (likely(mctop_alloc_hwctx_release(alloc)))
    {
      return mctop_alloc_pin_all(alloc);
    }
  return 1;
}

void
mctop_alloc_thread_print()
{
  if (mctop_alloc_thread_is_pinned() &&
      mctop_alloc_thread_get_alloc()->policy != MCTOP_ALLOC_NONE)
    {
      printf("[MCTOP ALLOC]     pinned : id %-3d / hwc id %-3u / node %-3u | "
	     "SEQ ids: in-nd %-2u in-co %-2u co-in-so %-2u co %-2u node %-2u\n",
	     mctop_alloc_thread_id(),
	     mctop_alloc_thread_hw_context_id(),
	     mctop_alloc_thread_local_node(),
	     mctop_alloc_thread_insocket_id(),
	     mctop_alloc_thread_incore_id(),
	     mctop_alloc_thread_core_insocket_id(),
	     mctop_alloc_thread_core_id(),
	     mctop_alloc_thread_node_id());
    }
  else
    {
      printf("[MCTOP ALLOC] NOT pinned : id %-3d / hwc id --- / node --- / node seq id ---\n",
	     mctop_alloc_thread_id());
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

inline uint
mctop_alloc_get_num_hw_contexts_node(mctop_alloc_t* alloc, const uint sid)
{
  return alloc->n_hwcs_per_socket[sid];
}

inline uint
mctop_alloc_get_num_cores_node(mctop_alloc_t* alloc, const uint sid)
{
  return alloc->n_cores_per_socket[sid];
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
mctop_alloc_get_nth_node(mctop_alloc_t* alloc, const uint nth)
{
  return alloc->sockets[nth]->local_node;
}

inline uint
mctop_alloc_node_to_nth_socket(mctop_alloc_t* alloc, const uint node)
{
  return alloc->node_to_nth_socket[node];
}

/* get the seq id in mctop of the mctop id of a socket */
int
mctop_alloc_socket_seq_id(mctop_alloc_t* alloc, const uint socket_mctop_id)
{
  for (int i = 0; i < alloc->n_sockets; i++)
    {
      if (alloc->sockets[i]->id == socket_mctop_id)
	{
	  return i;
	}
    }
  return -1;
}


struct bitmask* 
mctop_alloc_create_nodemask(mctop_alloc_t* alloc)
{
#if __x86_64__
  struct bitmask* nodemask = numa_allocate_nodemask();
  for (int n = 0; n < alloc->n_sockets; n++)
    {
      uint node = mctop_alloc_get_nth_node(alloc, n);
      nodemask = numa_bitmask_setbit(nodemask, node);
    }
  return nodemask;
#else
  return NULL;
#endif
}

inline double
mctop_alloc_get_nth_socket_bandwidth_proportion(mctop_alloc_t* alloc, const uint nth)
{
  return alloc->bw_proportions[nth];
}

inline uint
mctop_alloc_get_nth_hw_context(mctop_alloc_t* alloc, const uint nth)
{
  return alloc->hwcs[nth];
}

socket_t*
mctop_alloc_get_nth_socket(mctop_alloc_t* alloc, const uint nth)
{
  return alloc->sockets[nth];
}

inline uint
mctop_alloc_ids_get_latency(mctop_alloc_t* alloc, const uint id0, const uint id1)
{
  return mctop_ids_get_latency(alloc->topo, id0, id1);
}


/* thread ******************************************************************************** */

uint
mctop_alloc_thread_is_pinned()
{
  return __mctop_thread_info.is_pinned;
}

mctop_alloc_t*
mctop_alloc_thread_get_alloc()
{
  return __mctop_thread_info.alloc;
}

int
mctop_alloc_thread_id()
{
  return __mctop_thread_info.id;
}

int
mctop_alloc_thread_hw_context_id()
{
  if (likely(mctop_alloc_thread_is_pinned()))
    {
      return __mctop_thread_info.hwc_id;
    }
  return 0;
}

int
mctop_alloc_thread_core_id()
{
  if (likely(mctop_alloc_thread_is_pinned()))
    {
      return __mctop_thread_info.nth_core;
    }
  return 0;
}

uint
mctop_alloc_thread_incore_id()
{
  if (likely(mctop_alloc_thread_is_pinned()))
    {
      return __mctop_thread_info.nth_hwc_in_core;
    }
  return 0;
}

uint
mctop_alloc_thread_insocket_id()
{
  if (likely(mctop_alloc_thread_is_pinned()))
    {
      return __mctop_thread_info.nth_hwc_in_socket;
    }
  return 0;
}

/* mctop_alloc_thread_insocket_id() == 0 */
uint
mctop_alloc_thread_is_node_leader()
{
  if (likely(mctop_alloc_thread_is_pinned()))
    {
      return mctop_alloc_thread_insocket_id() == 0;
    }
  return 0;
}

/* mctop_alloc_thread_insocket_id() == (n_hwcs in socket - 1) */
uint
mctop_alloc_thread_is_node_last()
{
  if (likely(mctop_alloc_thread_is_pinned()))
    {
      mctop_alloc_t* alloc = __mctop_thread_info.alloc;
      const uint node = mctop_alloc_thread_node_id();
      int n_hwcs_node = mctop_alloc_get_num_hw_contexts_node(alloc, node);
      return mctop_alloc_thread_insocket_id() == (n_hwcs_node - 1);
    }
  return 0;
}

uint
mctop_alloc_thread_core_insocket_id()
{
  if (likely(mctop_alloc_thread_is_pinned()))
    {
      return __mctop_thread_info.nth_core_socket;
    }
  return 0;
}

int
mctop_alloc_thread_local_node()
{
  if (likely(mctop_alloc_thread_is_pinned()))
    {
      return __mctop_thread_info.local_node;
    }
  return 0;
}

int
mctop_alloc_thread_node_id()
{
  if (likely(mctop_alloc_thread_is_pinned()))
    {
      return __mctop_thread_info.nth_socket;
    }
  return 0;
}


/* malloc ******************************************************************************** */

void*
mctop_alloc_malloc_on_nth_socket(mctop_alloc_t* alloc, const uint nth, const size_t size)
{
  const uint node = mctop_alloc_get_nth_node(alloc, nth);
  return numa_alloc_onnode(size, node);
}

void
mctop_alloc_malloc_free(void* mem, const size_t size)
{
  numa_free(mem, size);
}

/* barrier ******************************************************************************* */

void
mctop_alloc_barrier_wait_all(mctop_alloc_t* alloc)
{
  mctop_barrier_wait(alloc->global_barrier);
}

void
mctop_alloc_barrier_wait_node(mctop_alloc_t* alloc)
{
  if (mctop_alloc_thread_is_pinned())
    {
      const uint on = mctop_alloc_thread_node_id();
      mctop_barrier_wait(alloc->socket_barriers[on]);
    }
}

void
mctop_alloc_barrier_wait_node_cores(mctop_alloc_t* alloc)
{
  if (mctop_alloc_thread_is_pinned())
    {
      const uint on = mctop_alloc_thread_node_id();
      mctop_barrier_wait(alloc->socket_barriers_cores[on]);
    }
}


/* pool ********************************************************************************** */


mctop_alloc_pool_t*
mctop_alloc_pool_create_empty(mctop_t* topo)
{
  mctop_alloc_pool_t* ap = calloc_assert(1, sizeof(mctop_alloc_pool_t));
  ap->topo = topo;
  return ap;
}

mctop_alloc_pool_t*
mctop_alloc_pool_create(mctop_t* topo, const int n_hwcs, const int n_config, mctop_alloc_policy policy)
{
  mctop_alloc_pool_t* ap = calloc_assert(1, sizeof(mctop_alloc_pool_t));
  ap->topo = topo;
  for (mctop_alloc_policy p = 0; p < MCTOP_ALLOC_NUM; p++)
    {
      mctop_alloc_pool_set_alloc(ap, n_hwcs, n_config, p);
    }

  mctop_alloc_pool_set_alloc(ap, n_hwcs, n_config, policy);
  return ap;
}

static void
mctop_alloc_args_free(mctop_alloc_args_t* aa)
{
  mctop_alloc_free(aa->alloc);
  free(aa);
}

void
mctop_alloc_pool_free(mctop_alloc_pool_t* ap)
{
  for (uint i = 0; i < MCTOP_ALLOC_NUM; i++)
    {
      mctop_alloc_args_t** alloc_args = ap->allocs[i];
      for (uint i = 0; i < MCTOP_ALLOC_POOL_MAX_N; i++)
	{
	  mctop_alloc_args_t* aa = alloc_args[i];
	  if (aa == NULL)
	    {
	      break;
	    }
	  mctop_alloc_args_free(aa);
	}
    }
  free(ap);
}

static inline mctop_alloc_args_t*
mctop_alloc_args_create(mctop_t* topo, const int n_hwcs, const int n_config, mctop_alloc_policy policy)
{
  mctop_alloc_args_t* aa = malloc_assert(sizeof(mctop_alloc_args_t));
  aa->n_hwcs = n_hwcs;
  aa->n_config = n_config;
  aa->alloc = mctop_alloc_create_simple(topo, n_hwcs, n_config, policy);
  return aa;
}


void
mctop_alloc_pool_set_alloc(mctop_alloc_pool_t* ap, const int n_hwcs, const int n_config, mctop_alloc_policy policy)
{
  int n_hwcs_actual = n_hwcs, n_config_actual = n_config;
  switch (policy)
    {
    case MCTOP_ALLOC_NONE:
    case MCTOP_ALLOC_SEQUENTIAL:
      n_hwcs_actual = MCTOP_ALLOC_ALL;
      n_config_actual = MCTOP_ALLOC_ALL;
      break;
    case MCTOP_ALLOC_MIN_LAT_HWCS:
    case MCTOP_ALLOC_MIN_LAT_CORES_HWCS:
    case MCTOP_ALLOC_MIN_LAT_CORES:
    case MCTOP_ALLOC_BW_ROUND_ROBIN_HWCS:
    case MCTOP_ALLOC_BW_ROUND_ROBIN_CORES:
      n_hwcs_actual = MCTOP_ALLOC_ALL;
      break;
    default:
      break;
    }

  mctop_dprint("MCTOP Alloc Pool: Get %-s allocator [#n_hwcs %-3d / #n_config %-2d]!\n",
	       mctop_alloc_policy_desc[policy], n_hwcs, n_config);


  mctop_alloc_args_t** alloc_args = ap->allocs[policy];
  mctop_alloc_args_t* aa = NULL;
  for (uint i = 0; i < MCTOP_ALLOC_POOL_MAX_N; i++)
    {
      aa = alloc_args[i];
      if (aa == NULL)
	{
	  aa = alloc_args[i] = mctop_alloc_args_create(ap->topo, n_hwcs_actual, n_config_actual, policy);
	  mctop_dprint("MCTOP Alloc Pool: Created %p - %-s allocator [#n_hwcs %-3d / #n_config %-2d]!\n",
		       aa, mctop_alloc_policy_desc[policy], aa->n_hwcs, aa->n_config);
	  break;
	}
      else if ((aa->n_hwcs == n_hwcs_actual) && (aa->n_config == n_config_actual))
	{
	  mctop_dprint("MCTOP Alloc Pool: Found   %p - %-s allocator! [#n_hwcs %-3d / #n_config %-2d]!\n",
		       aa, mctop_alloc_policy_desc[policy], aa->n_hwcs, aa->n_config);
	  break;
	}
    }

  if (unlikely(aa == NULL))
    {
      fprintf(stderr, "MCTOP Warning: Out of pool space for new %s alloctors! \n\tReturning %s\n",
	      mctop_alloc_policy_desc[policy], mctop_alloc_policy_desc[MCTOP_ALLOC_SEQUENTIAL]);
      if (ap->allocs[MCTOP_ALLOC_SEQUENTIAL][0] == NULL)
	{
	  ap->allocs[MCTOP_ALLOC_SEQUENTIAL][0] = mctop_alloc_args_create(ap->topo,
									  MCTOP_ALLOC_ALL,
									  MCTOP_ALLOC_ALL,
									  MCTOP_ALLOC_SEQUENTIAL);
	}
    }

  ap->current_alloc = aa->alloc;
  MFENCE();
}

int
mctop_alloc_pool_pin_on_nth_socket(mctop_alloc_pool_t* ap, const uint n)
{
  mctop_thread_get_info()->alloc_pool = ap;
  return mctop_alloc_pin_nth_socket((mctop_alloc_t*) ap->current_alloc, n);
}

int
mctop_alloc_pool_pin(mctop_alloc_pool_t* ap)
{
  mctop_thread_get_info()->alloc_pool = ap;
  mctop_alloc_t* ca = (mctop_alloc_t*) ap->current_alloc;
  /* if the allocator has changed or the thread is not pinned */
  if (unlikely(ca != mctop_alloc_thread_get_alloc() || !mctop_alloc_thread_is_pinned()))
    {
      mctop_alloc_hwctx_release(mctop_alloc_thread_get_alloc());
      return mctop_alloc_pin_simple(ca);
    }
  return 0;
}

int
mctop_alloc_pool_pin_on(mctop_alloc_pool_t* ap, const uint seq_id)
{
  mctop_thread_get_info()->alloc_pool = ap;
  mctop_alloc_t* ca = (mctop_alloc_t*) ap->current_alloc;
  /* if the allocator has changed or the thread is not pinned */
  if (unlikely(ca != mctop_alloc_thread_get_alloc() || !mctop_alloc_thread_is_pinned()))
    {
      mctop_alloc_hwctx_release(mctop_alloc_thread_get_alloc());
      return mctop_alloc_pin_on(ca, seq_id);
    }
  return 0;
}
