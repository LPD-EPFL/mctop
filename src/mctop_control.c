#include <mctop.h>
#include <darray.h>
#include <numa.h>

inline uint
mctop_are_hwcs_same_core(hw_context_t* a, hw_context_t* b)
{
  return (a->type == HW_CONTEXT && b->type == HW_CONTEXT && a->parent == b->parent);
}
