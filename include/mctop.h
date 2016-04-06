#ifndef __H_MCTOP__
#define __H_MCTOP__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

#ifdef __x86_64__
#  include <numa.h>
#elif defined(__sparc__)
#  include <sys/lgrp_user.h>
#  include <numa_sparc.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

#define MCTOP_LVL_ID_MULTI 10000

  typedef enum
    {
      HW_CONTEXT,
      CORE,
      HWC_GROUP,
      SOCKET,
      CROSS_SOCKET,
    } mctop_type_t;

  static const char* mctop_type_desc[] =
    {
      "HW Context",
      "Core",
      "HW Context group",
      "Socket",
      "Cross Socket",
    };

  typedef enum
    {
      NO_MEMORY,
      LATENCY,
      BANDWIDTH,
    } mctop_mem_type_t;

  static inline const char*
  mctop_get_type_desc(mctop_type_t type)
  {
    return mctop_type_desc[type];
  }
  typedef unsigned int uint;
  typedef struct hwc_gs socket_t;
  typedef struct hwc_gs hwc_group_t;

  typedef struct mctop_cache_info
  {
    uint n_levels;
    uint64_t* latencies;
    uint64_t* sizes_OS;
    uint64_t* sizes_estimated;
  } mctop_cache_info_t;

  typedef struct mctop
  {
    uint n_levels;		/* num. of latency lvls */
    uint* latencies;		/* latency per level */
    uint n_hwcs;			/* num. of hwcs in this machine */
    uint socket_level;		/* level of sockets */
    uint n_sockets;		/* num. of sockets/nodes */
    socket_t* sockets;		/* pointer to sockets/nodes */
    uint is_smt;		/* is SMT enabled CPU */
    uint n_hwcs_per_core;       /* if SMT, how many hw contexts per core? */
    uint has_mem;	        /* flag whether there are mem. latencies */
    uint* node_to_socket;       /* node-id to socket-id translation */
    struct hw_context* hwcs;	/* pointers to hwcs */
    uint n_siblings;		/* total number of sibling relationships */
    struct sibling** siblings;	/* pointers to sibling relationships */
    mctop_cache_info_t* cache;	/* pointer to cache information */
    double* mem_bandwidths_r;	/* Read mem. bandwidth of each socket, maximum */
    double* mem_bandwidths1_r;	/* Read mem. bandwidth of each socket, single threaded */
    double* mem_bandwidths_w;	/* Write mem. bandwidth of each socket, maximum */
    double* mem_bandwidths1_w;	/* Write mem. bandwidth of each socket, single threaded */
  } mctop_t;

  typedef struct hwc_gs		/* group / socket */
  {
    uint id;			/* mctop id */
    uint level;			/* latency hierarchy lvl */
    mctop_type_t type;		/* HWC_GROUP or SOCKET */
    uint latency;		/* comm. latency within group */
    socket_t* socket;		/* Group: pointer to parent socket */
    struct hwc_gs* parent;	/* Group: pointer to parent hwcgroup */
    uint n_hwcs;		/* num. of hwcs descendants */
    struct hw_context** hwcs;	/* descendant hwcs */
    uint n_cores;	        /* num. of physical cores (if !smt = n_hwcs */
    uint n_children;		/* num. of hwc_group descendants */
    struct hwc_gs** children;	/* pointer to children hwcgroup */
    struct hwc_gs* next;	/* link groups of a level to a list */
    mctop_t* topo;		/* link to topology socket only info */
    uint n_siblings;		/* number of other sockets */
    struct sibling** siblings;	/* pointers to other sockets, sorted closest 1st, max bw from this to sibling */
    struct sibling** siblings_in;	/* pointers to other sockets, sorted closest 1st, max bw sibling to this */
    uint local_node;		/* local NUMA mem. node */
    uint n_nodes;		/* num of nodes = topo->n_sockets */
    uint* mem_latencies;	/* mem. latencies to NUMA nodes */
    double* mem_bandwidths_r;	/* Read mem. bandwidth of each socket, maximum */
    double* mem_bandwidths1_r;	/* Read mem. bandwidth of each socket, single threaded */
    double* mem_bandwidths_w;	/* Write mem. bandwidth of each socket, maximum */
    double* mem_bandwidths1_w;	/* Write mem. bandwidth of each socket, single threaded */
  } hwc_gs_t;

  typedef struct sibling
  {
    uint id;			/* needed?? */
    uint level;			/* latency hierarchy lvl */
    uint latency;			/* comm. latency across this hop */
    socket_t* left;		/* left  -->sibling--> right */
    socket_t* right;		/* right -->sibling--> left */
    struct sibling* next;
  } sibling_t;

  typedef struct hw_context
  {
    uint id;			/* mctop id */
    uint level;			/* latency hierarchy lvl */
    mctop_type_t type;		/* HW_CONTEXT or CORE? */
    uint phy_id;			/* OS id (e.g., for set_cpu() */
    socket_t* socket;		/* pointer to parent socket */
    struct hwc_gs* parent;	/* pointer to parent hwcgroup */
    struct hw_context* next;	/* link hwcs to a list */
  } hw_context_t;


  /* ******************************************************************************** */
  /* CDF fucntions */
  /* ******************************************************************************** */

  typedef struct cdf_point
  {
    uint64_t val;
    double percentile;
  } cdf_point_t;

  typedef struct cdf
  {
    size_t n_points;
    cdf_point_t* points;
  } cdf_t;

  typedef struct cdf_cluster_point
  {
    int idx;
    size_t size;
    uint64_t val_min;
    uint64_t val_max;
    uint64_t median;
  } cdf_cluster_point_t;

  typedef struct cdf_cluster
  {
    size_t n_clusters;
    cdf_cluster_point_t* clusters;
  } cdf_cluster_t;


  /* ******************************************************************************** */
  /* MCTOP CONSTRUCTION IF */
  /* ******************************************************************************** */

  mctop_t* mctop_construct(uint64_t** lat_table_norm, const size_t N,
			   uint64_t** mem_lat_table, const uint n_sockets,
			   cdf_cluster_t* cc, const int is_smt);
  mctop_t* mctop_load(const char* mct_file);
  void mctop_free(mctop_t* topo);
  void mctop_mem_bandwidth_add(mctop_t* topo, double** mem_bw_r, double** mem_bw_r1, double** mem_bw_w, double** mem_bw_w1);
  void mctop_mem_latencies_add(mctop_t* topo, uint64_t** mem_lat_table);
  void mctop_cache_info_add(mctop_t* topo, mctop_cache_info_t* mci);

  void mctop_print(mctop_t* topo);
  void mctop_dot_graph_plot(mctop_t* topo,  const uint max_cross_socket_lvl);

  /* ******************************************************************************** */
  /* MCTOP CONTROL IF */
  /* ******************************************************************************** */

  /* topo getters ******************************************************************* */
  socket_t* mctop_get_socket(mctop_t* topo, const uint socket_n);
  socket_t* mctop_get_first_socket(mctop_t* topo);
  hwc_gs_t* mctop_get_first_gs_core(mctop_t* topo);
  hwc_gs_t* mctop_get_first_gs_at_lvl(mctop_t* topo, const uint lvl);
  sibling_t* mctop_get_first_sibling_lvl(mctop_t* topo, const uint lvl);
  sibling_t* mctop_get_sibling_with_sockets(mctop_t* topo, socket_t* s0, socket_t* s1);

  size_t mctop_get_num_nodes(mctop_t* topo);
  size_t mctop_get_num_cores_per_socket(mctop_t* topo);
  size_t mctop_get_num_hwc_per_socket(mctop_t* topo);
  size_t mctop_get_num_hwc_per_core(mctop_t* topo);

  /* cache */
  typedef enum 
    {
      L1I,			/* L1 instruction */
      L1D,			/* L1D = L1 */
      L2,
      L3,			/* LLC = L3 */
    } mctop_cache_level_t;
#define L1  L1D
#define LLC L3

  size_t mctop_get_cache_size_kb(mctop_t* topo, mctop_cache_level_t level);
  /* estimated size and latency not defined for L1I */
  size_t mctop_get_cache_size_estimated_kb(mctop_t* topo, mctop_cache_level_t level);
  size_t mctop_get_cache_latency(mctop_t* topo, mctop_cache_level_t level);

  /* socket getters ***************************************************************** */
  hw_context_t* mctop_socket_get_first_hwc(socket_t* socket);
  hw_context_t* mctop_socket_get_nth_hwc(socket_t* socket, const uint nth);
  hwc_gs_t* mctop_socket_get_first_gs_core(socket_t* socket);
  hwc_gs_t* mctop_socket_get_nth_gs_core(socket_t* socket, const uint nth);
  hwc_gs_t* mctop_socket_get_first_child_lvl(socket_t* socket, const uint lvl);
  size_t mctop_socket_get_num_cores(socket_t* socket);
  double mctop_socket_get_bw_local(socket_t* socket);
  double mctop_socket_get_bw_local_one(socket_t* socket);

  double mctop_socket_get_bw_to(socket_t* socket, socket_t* to);

  uint mctop_socket_get_local_node(socket_t* socket);

  /* hwcid ************************************************************************** */
  uint mctop_hwcid_get_local_node(mctop_t* topo, const uint hwcid);
  socket_t* mctop_hwcid_get_socket(mctop_t* topo, const uint hwcid);
  hwc_gs_t* mctop_hwcid_get_core(mctop_t* topo, const uint hwcid);
  uint mctop_hwcid_get_nth_hwc_in_core(mctop_t* topo, const uint hwcid);
  uint mctop_hwcid_get_nth_core_in_socket(mctop_t* topo, const uint hwcid);

  /* mctop id *********************************************************************** */
  hwc_gs_t* mctop_id_get_hwc_gs(mctop_t* topo, const uint id);

  /* queries ************************************************************************ */
  uint mctop_hwcs_are_same_core(hw_context_t* a, hw_context_t* b);
  uint mctop_has_mem_lat(mctop_t* topo);
  uint mctop_has_mem_bw(mctop_t* topo);
  uint mctop_ids_get_latency(mctop_t* topo, const uint id0, const uint id1);

  /* sibling getters ***************************************************************** */
  socket_t* mctop_sibling_get_other_socket(sibling_t* sibling, socket_t* socket);
  uint mctop_sibling_contains_sockets(sibling_t* sibling, socket_t* s0, socket_t* s1);

  /* optimizing ********************************************************************** */
  int mctop_hwcid_fix_numa_node(mctop_t* topo, const uint hwcid);



  static inline uint
  mctop_create_id(uint seq_id, uint lvl)
  {
    return ((lvl * MCTOP_LVL_ID_MULTI) + seq_id);
  }

  static inline uint
  mctop_id_no_lvl(uint id)
  {
    return (id % MCTOP_LVL_ID_MULTI);
  }

  static inline uint
  mctop_id_get_lvl(uint id)
  {
    return (id / MCTOP_LVL_ID_MULTI);
  }

  static inline void
  mctop_print_id(uint id)
  {
    uint sid = mctop_id_no_lvl(id);
    uint lvl = mctop_id_get_lvl(id);
    printf("%u-%04u", lvl, sid);
  }

#define MCTOP_ID_PRINTER "%u-%04u"
#define MCTOP_ID_PRINT(id)  mctop_id_get_lvl(id), mctop_id_no_lvl(id)


  /* ******************************************************************************** */
  /* MCTOP Scheduling */
  /* ******************************************************************************** */
  int mctop_run_on_socket(mctop_t* topo, const uint socket_n);
  int mctop_run_on_socket_nm(mctop_t* topo, const uint socket_n); /* doea not set preferred node */
  int mctop_run_on_node(mctop_t* topo, const uint node_n);

  int mctop_run_on_socket_ref(socket_t* socket, const uint fix_mem);

  extern int mctop_set_cpu(int cpu);


  typedef uint64_t mctop_ticks;

#if defined(__i386__)
  static inline mctop_ticks
  mctop_getticks(void)
  {
    mctop_ticks ret;

    __asm__ __volatile__("rdtsc" : "=A" (ret));
    return ret;
  }
#elif defined(__x86_64__)
  static inline mctop_ticks
  mctop_getticks(void)
  {
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
  }
#elif defined(__sparc__)
  static inline mctop_ticks
  mctop_getticks()
  {
    mctop_ticks ret = 0;
    __asm__ __volatile__ ("rd %%tick, %0" : "=r" (ret) : "0" (ret));
    return ret;
  }
#elif defined(__tile__)
#include <arch/cycle.h>
  static inline mctop_ticks
  mctop_getticks()
  {
    return get_cycle_count();
  }
#endif


#ifdef __cplusplus
}
#endif

#endif	/* __H_MCTOP__ */


