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

socket_t*
mctop_get_socket(mctopo_t* topo, const uint socket_n)
{
  return topo->sockets + socket_n;
}

hw_context_t*
mctop_get_first_hwc_socket(socket_t* socket)
{
  return socket->hwcs[0];
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
