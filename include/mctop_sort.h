#ifndef __H_MCTOP_SORT__
#define __H_MCTOP_SORT__

#include <mctop_alloc.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MCTOP_SORT_TYPE                   uint
#define MCTOP_SORT_TYPE_FORMAT            "%u"
#define MCTOP_SORT_MIN_LEN_PARALLEL       (2 * 1024 * 1024LL / sizeof(MCTOP_SORT_TYPE))

  /* mctop sort versions */
#define MCTOP_SORT_NO_SSE_SMT             0
#define MCTOP_SORT_SSE_NO_SMT             1
#define MCTOP_SORT_SSE_SMT                2
#define MCTOP_SORT_NO_SSE_SMT_ALL_ND      3

#if !defined(MCTOP_SORT_SSE_HYPERTHREAD_RATIO)
#define MCTOP_SORT_SSE_HYPERTHREAD_RATIO  3
#endif

#if !defined(MCTOP_SORT_USE_SSE)
#define MCTOP_SORT_USE_SSE                2
#endif
#define MCTOP_SSE_K                       4
#define MCTOP_SORT_COPY_FIRST             1
#define MCTOP_NUM_CHUNKS_PER_THREAD       1

#define MCTOP_SORT_DEBUG                  0

  /* partition descriptor */
typedef struct mctop_sort_pd
{
  size_t start_index;
  size_t n_elems;
} mctop_sort_pd_t;

  /* node data descriptor */
typedef struct mctop_sort_nd 
{
  MCTOP_SORT_TYPE* array;
  size_t n_elems;
  MCTOP_SORT_TYPE* source;
  MCTOP_SORT_TYPE* destination;
  size_t n_chunks;
  mctop_sort_pd_t* partitions;
  const uint8_t padding[64 - 3 * sizeof(MCTOP_SORT_TYPE*) -
			sizeof(size_t) - sizeof(mctop_sort_pd_t*)];
} mctop_sort_nd_t;


  /* total data descriptor */
typedef MCTOP_ALIGNED(64) struct mctop_sort_td
{
  mctop_node_tree_t* nt;
  MCTOP_SORT_TYPE* array;
  size_t n_elems;
  uint8_t padding[64 - sizeof(mctop_node_tree_t*) - sizeof(MCTOP_SORT_TYPE*) - sizeof(size_t)];
  mctop_sort_nd_t node_data[0];
} mctop_sort_td_t;




#if MCTOP_SORT_DEBUG == 1
#  define MSD_DO(x) x
#else
#  define MSD_DO(x)
#endif



#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

  void mctop_sort(MCTOP_SORT_TYPE* array, const size_t n_elems, mctop_node_tree_t* nt);


#ifdef __cplusplus
}
#endif

#endif	/* __H_MCTOP_SORT__ */

