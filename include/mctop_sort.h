#ifndef __H_MCTOP_SORT__
#define __H_MCTOP_SORT__

#include <mctop_alloc.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MCTOP_SORT_TYPE uint

  void mctop_sort(MCTOP_SORT_TYPE* array, const size_t n_elems, mctop_node_tree_t* nt);


#ifdef __cplusplus
}
#endif

#endif	/* __H_MCTOP_SORT__ */

