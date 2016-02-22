#include <mctop.h>
#include <darray.h>
#include <numa.h>

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
