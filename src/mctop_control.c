#include <mctop.h>
#include <darray.h>
#ifdef __x86_64__
#  include <numa.h>
#endif

inline uint
mctop_are_hwcs_same_core(hw_context_t* a, hw_context_t* b)
{
  return (a->type == HW_CONTEXT && b->type == HW_CONTEXT && a->parent == b->parent);
}

inline socket_t*
mctop_get_first_socket(mctopo_t* topo)
{
  return topo->sockets;
}

inline sibling_t*
mctop_get_first_sibling_lvl(mctopo_t* topo, const uint lvl)
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

inline hwc_gs_t*
mctop_get_first_gs_at_lvl(mctopo_t* topo, const uint lvl)
{
  hwc_gs_t* cur = mctop_get_first_socket(topo);
  while (cur != NULL && cur->level != lvl)
    {
      cur = cur->children[0];
    }
  return cur;
}

hwc_gs_t*
mctop_get_first_child_lvl(socket_t* socket, const uint lvl)
{
  hwc_gs_t* cur = socket->children[0];
  while (cur != NULL && cur->level != lvl)
    {
      cur = cur->children[0];
    }
  return cur;
}

inline socket_t*
mctop_get_socket(mctopo_t* topo, const uint socket_n)
{
  return topo->sockets + socket_n;
}

inline hw_context_t*
mctop_get_first_hwc_socket(socket_t* socket)
{
  return socket->hwcs[0];
}

inline size_t
mctop_get_num_cores_per_socket(mctopo_t* topo)
{
  if (topo->is_smt)
    {
      return (topo->n_hwcs / topo->n_sockets / topo->n_hwcs_per_core);
    }
  return (topo->n_hwcs / topo->n_sockets);
}

inline size_t
mctop_get_num_nodes(mctopo_t* topo)
{
  return topo->n_sockets;
}

size_t
mctop_get_num_hwc_per_socket(mctopo_t* topo)
{
  return topo->sockets[0].n_hwcs;
}




inline uint
mctop_has_mem_lat(mctopo_t* topo)
{
  return topo->has_mem >= LATENCY;
}

inline uint
mctop_has_mem_bw(mctopo_t* topo)
{
  return topo->has_mem == BANDWIDTH;
}



int
mctop_run_on_socket_ref(socket_t* socket)
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
  if (!ret && socket->topo->has_mem)
    {
      numa_set_preferred(socket->local_node);
    }
  numa_bitmask_free(bmask);
#else
#  warning mctop_run_on_socket_n fix me
#endif
  return ret;
}

int
mctop_run_on_socket(mctopo_t* topo, const uint socket_n)
{
  if (socket_n >= topo->n_sockets)
    {
      return -EINVAL;
    }
  socket_t* socket = &topo->sockets[socket_n];
  return mctop_run_on_socket_ref(socket);
}

int
mctop_run_on_node(mctopo_t* topo, const uint node_n)
{
  if (node_n >= topo->n_sockets)
    {
      return -EINVAL;
    }

  const uint socket_n = topo->node_to_socket[node_n];
  socket_t* socket = &topo->sockets[socket_n];
  return mctop_run_on_socket_ref(socket);
}
