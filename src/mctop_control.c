#include <mctop.h>
#include <darray.h>
#ifdef __x86_64__
#  include <numa.h>
#endif

/* topo getters ******************************************************************* */

inline socket_t*
mctop_get_socket(mctop_t* topo, const uint socket_n)
{
  return topo->sockets + socket_n;
}

inline socket_t*
mctop_get_first_socket(mctop_t* topo)
{
  return topo->sockets;
}

hwc_gs_t*
mctop_get_first_gs_core(mctop_t* topo)
{
  hwc_gs_t* gs = topo->sockets[0].children[0];
  while (gs && gs->type != CORE)
    {
      gs = gs->children[0];
    }
  return gs;
}

inline hwc_gs_t*
mctop_get_first_gs_at_lvl(mctop_t* topo, const uint lvl)
{
  hwc_gs_t* cur = mctop_get_first_socket(topo);
  while (cur != NULL && cur->level != lvl)
    {
      cur = cur->children[0];
    }
  return cur;
}


inline sibling_t*
mctop_get_first_sibling_lvl(mctop_t* topo, const uint lvl)
{
  for (int i = 0; i < topo->n_siblings; i++)
    {
      if (topo->siblings[i]->level == lvl)
	{
	  return topo->siblings[i];
	}
    }
  return NULL;
}


inline size_t
mctop_get_num_nodes(mctop_t* topo)
{
  return topo->n_sockets;
}

inline size_t
mctop_get_num_cores_per_socket(mctop_t* topo)
{
  return topo->sockets[0].n_cores;
}

size_t
mctop_get_num_hwc_per_socket(mctop_t* topo)
{
  return topo->sockets[0].n_hwcs;
}

sibling_t*
mctop_get_sibling_with_sockets(mctop_t* topo, socket_t* s0, socket_t* s1)
{
  for (int i = 0; i < topo->n_siblings; i++)
    {
      sibling_t* sibling = topo->siblings[i];
      if (mctop_sibling_contains_sockets(sibling, s0, s1))
	{
	  return sibling;
	}
    }

  return NULL;
}

/* socket getters ***************************************************************** */

inline hw_context_t*
mctop_socket_get_first_hwc(socket_t* socket)
{
  return socket->hwcs[0];
}

inline hw_context_t*
mctop_socket_get_nth_hwc(socket_t* socket, const uint nth)
{
  return socket->hwcs[nth];
}

hwc_gs_t*
mctop_socket_get_first_gs_core(socket_t* socket)
{
  hwc_gs_t* gs = socket->children[0];
  while (gs && gs->type != CORE)
    {
      gs = gs->children[0];
    }
  return gs;
}

hwc_gs_t*
mctop_socket_get_nth_gs_core(socket_t* socket, const uint nth)
{
  hwc_gs_t* gs = socket->children[0];
  while (gs && gs->type != CORE)
    {
      gs = gs->children[0];
    }
  for (int i = 0; i < nth; i++)
    {
      gs = gs->next;
    }
  return gs;
}

hwc_gs_t*
mctop_socket_get_first_child_lvl(socket_t* socket, const uint lvl)
{
  hwc_gs_t* cur = socket->children[0];
  while (cur != NULL && cur->level != lvl)
    {
      cur = cur->children[0];
    }
  return cur;
}

size_t
mctop_socket_get_num_cores(socket_t* socket)
{
  return socket->n_cores;
}

inline double
mctop_socket_get_bw_local(socket_t* socket)
{
  return socket->mem_bandwidths_r[socket->local_node];
}

inline double
mctop_socket_get_bw_local_one(socket_t* socket)
{
  return socket->mem_bandwidths1_r[socket->local_node];
}

/* sibling getters ***************************************************************** */

socket_t*
mctop_sibling_get_other_socket(sibling_t* sibling, socket_t* socket)
{
  if (sibling->left == socket)
    {
      return sibling->right;
    }
  return sibling->left;
}

uint
mctop_sibling_contains_sockets(sibling_t* sibling, socket_t* s0, socket_t* s1)
{
  if ((sibling->left == s0 && sibling->right == s1) ||
      (sibling->left == s1 && sibling->right == s0))
    {
      return 1;
    }
  return 0;
}



/* hwcid ************************************************************************ */

int
mctop_hwcid_fix_numa_node(mctop_t* topo, const uint hwcid)
{
  if (likely(topo->has_mem))
    {
      /* printf("# HWID %-3u, SOCKET %-3u, numa_set_preferred(%u)\n", */
      /* 	     hwcid, hwc->socket->id, hwc->socket->local_node); */
      numa_set_preferred(mctop_hwcid_get_local_node(topo, hwcid));
      return 1;
    }

  return 0;
}

inline uint
mctop_hwcid_get_local_node(mctop_t* topo, uint hwcid)
{
  return mctop_hwcid_get_socket(topo, hwcid)->local_node;
}

inline socket_t*
mctop_hwcid_get_socket(mctop_t* topo, uint hwcid)
{
  return topo->hwcs[hwcid].socket;
}


/* queries ************************************************************************ */

inline uint
mctop_hwcs_are_same_core(hw_context_t* a, hw_context_t* b)
{
  return (a->type == HW_CONTEXT && b->type == HW_CONTEXT && a->parent == b->parent);
}

inline uint
mctop_has_mem_lat(mctop_t* topo)
{
  return topo->has_mem >= LATENCY;
}

inline uint
mctop_has_mem_bw(mctop_t* topo)
{
  return topo->has_mem == BANDWIDTH;
}

static hwc_gs_t*
mctop_id_get_hwc_gs(mctop_t* topo, const uint id)
{
  uint lvl = mctop_id_get_lvl(id);
  hwc_gs_t* gs = NULL;
  if (lvl == 0)
    {
      gs = (hwc_gs_t*) &topo->hwcs[id];
    }
  else if (lvl == topo->socket_level)
    {
      gs = (hwc_gs_t*) &topo->sockets[mctop_id_no_lvl(id)];
    }
  else
    {
      hwc_gs_t* _gs = mctop_get_first_gs_at_lvl(topo, lvl);
      while (_gs != NULL)
	{
	  if (unlikely(_gs->id == id))
	    {
	      gs = _gs;
	      break;
	    }
	  _gs = _gs->next;
	}
    }

  return gs;
}

uint
mctop_ids_get_latency(mctop_t* topo, const uint id0, const uint id1)
{
  hwc_gs_t* gs0 = mctop_id_get_hwc_gs(topo, id0);
  hwc_gs_t* gs1 = mctop_id_get_hwc_gs(topo, id1);

  while (gs0->level < gs1->level)
    {
      gs0 = gs0->parent;
    }
  while (gs1->level < gs0->level)
    {
      gs1 = gs1->parent;
    }

  if (unlikely(gs0->id == gs1->id))
    {
      if (gs0->level == 0)
	{
	  return 0;
	}
      else
	{
	  return gs0->latency;
	}
    }

  while (gs0->type != SOCKET && gs1->type != SOCKET)
    {
      gs0 = gs0->parent;
      gs1 = gs1->parent;
      if (gs0->id == gs1->id)
	{
	  return gs0->latency;
	}
    }


  sibling_t* sibling = mctop_get_sibling_with_sockets(topo, gs0, gs1);
  return sibling->latency;
}



/* pining ************************************************************************ */

int
mctop_run_on_socket_ref(socket_t* socket, const uint fix_mem)
{
  int ret = 0;
  if (socket == NULL)
    {
      return -EINVAL;
    }

#ifdef __x86_64__
  struct bitmask* bmask = numa_bitmask_alloc(socket->topo->n_hwcs);
  for (int i = 0; i < socket->n_hwcs; i++) 
    {
      bmask = numa_bitmask_setbit(bmask, socket->hwcs[i]->id);
    }

  ret = numa_sched_setaffinity(0, bmask);
  if (fix_mem && !ret && socket->topo->has_mem)
    {
      numa_set_preferred(socket->local_node);
    }
  numa_bitmask_free(bmask);
#else
  ret = mctop_run_on_node(socket->topo, socket->local_node);
#endif
  return ret;
}

int
mctop_run_on_socket(mctop_t* topo, const uint socket_n)
{
  if (socket_n >= topo->n_sockets)
    {
      return -EINVAL;
    }
  socket_t* socket = &topo->sockets[socket_n];
  return mctop_run_on_socket_ref(socket, 1);
}

int
mctop_run_on_socket_nm(mctop_t* topo, const uint socket_n)
{
  if (socket_n >= topo->n_sockets)
    {
      return -EINVAL;
    }
  socket_t* socket = &topo->sockets[socket_n];
  return mctop_run_on_socket_ref(socket, 0);
}

int
mctop_run_on_node(mctop_t* topo, const uint node_n)
{
#if __x86_64__
  if (node_n >= topo->n_sockets)
    {
      return -EINVAL;
    }

  const uint socket_n = topo->node_to_socket[node_n];
  socket_t* socket = &topo->sockets[socket_n];
  return mctop_run_on_socket_ref(socket, 1);
#elif __sparc
  lgrp_id_t root = lgrp_root(lgrp_cookie);
  lgrp_id_t lgrp_array[SPART_LGRP_MAX_NODES];
  int ret = lgrp_children(lgrp_cookie, root, lgrp_array, SPART_LGRP_MAX_NODES);
  ret = ret && lgrp_affinity_set(P_LWPID, P_MYID, lgrp_array[node_n], LGRP_AFF_STRONG);
  return ret;
#endif
}



/*  */

int
mctop_set_cpu(int cpu) 
{
  int ret = 1;
#if defined(__sparc__)
  if (processor_bind(P_LWPID, P_MYID, cpu, NULL) == -1)
    {
      /* printf("Problem with setting processor affinity: %s\n", */
      /* 	     strerror(errno)); */
      ret = 0;
    }
#elif defined(__tile__)
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, cpu)) < 0)
    {
      tmc_task_die("Failure in 'tmc_cpus_set_my_cpu()'.");
    }

  if (cpu != tmc_cpus_get_my_cpu())
    {
      PRINT("******* i am not CPU %d", tmc_cpus_get_my_cpu());
    }

#else
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(cpu, &mask);
  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0)
    {
      /* printf("Problem with setting processor affinity: %s\n", */
      /* 	     strerror(errno)); */
      ret = 0;
    }
#endif

  return ret;
}
