#ifndef __H_MCTOP__
#define __H_MCTOP__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>

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

  typedef struct mctop
  {
    uint n_levels;		/* num. of latency lvls */
    uint* latencies;		/* latency per level */
    uint n_hwcs;			/* num. of hwcs in this machine */
    uint socket_level;		/* level of sockets */
    uint n_sockets;		/* num. of sockets/nodes */
    socket_t* sockets;		/* pointer to sockets/nodes */
    uint is_smt;			/* is SMT enabled CPU */
    uint n_hwcs_per_core;		/* if SMT, how many hw contexts per core? */
    uint has_mem;			/* flag whether there are mem. latencies */
    uint* node_to_socket;		/* node-id to socket-id translation */
    struct hw_context* hwcs;	/* pointers to hwcs */
    uint n_siblings;		/* total number of sibling relationships */
    struct sibling** siblings;	/* pointers to sibling relationships */
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
    uint latency;			/* comm. latency within group */
    socket_t* socket;		/* Group: pointer to parent socket */
    struct hwc_gs* parent;	/* Group: pointer to parent hwcgroup */
    uint n_hwcs;			/* num. of hwcs descendants */
    struct hw_context** hwcs;	/* descendant hwcs */
    uint n_cores;			/* num. of physical cores (if !smt = n_hwcs */
    uint n_children;		/* num. of hwc_group descendants */
    struct hwc_gs** children;	/* pointer to children hwcgroup */
    struct hwc_gs* next;		/* link groups of a level to a list */
    mctop_t* topo;		/* link to topology */
    /* socket only info */
    uint n_siblings;		/* number of other sockets */
    struct sibling** siblings;	/* pointers to other sockets, sorted closest 1st, max bw from this to sibling */
    struct sibling** siblings_in;	/* pointers to other sockets, sorted closest 1st, max bw sibling to this */
    uint local_node;		/* local NUMA mem. node */
    uint n_nodes;			/* num of nodes = topo->n_sockets */
    uint* mem_latencies;		/* mem. latencies to NUMA nodes */
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

  /* socket getters ***************************************************************** */
  hw_context_t* mctop_socket_get_first_hwc(socket_t* socket);
  hw_context_t* mctop_socket_get_nth_hwc(socket_t* socket, const uint nth);
  hwc_gs_t* mctop_socket_get_first_gs_core(socket_t* socket);
  hwc_gs_t* mctop_socket_get_nth_gs_core(socket_t* socket, const uint nth);
  hwc_gs_t* mctop_socket_get_first_child_lvl(socket_t* socket, const uint lvl);
  size_t mctop_socket_get_num_cores(socket_t* socket);
  double mctop_socket_get_bw_local(socket_t* socket);
  double mctop_socket_get_bw_local_one(socket_t* socket);

  /* hwcid ************************************************************************** */
  uint mctop_hwcid_get_local_node(mctop_t* topo, const uint hwcid);
  socket_t* mctop_hwcid_get_socket(mctop_t* topo, const uint hwcid);
  hwc_gs_t* mctop_hwcid_get_core(mctop_t* topo, const uint hwcid);

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


  /* ******************************************************************************** */
  /* AUX functions */
  /* ******************************************************************************** */
  void** table_malloc(const size_t rows, const size_t cols, const size_t elem_size);
  void** table_calloc(const size_t rows, const size_t cols, const size_t elem_size);
  void table_free(void** m, const size_t cols);

  static inline void*
  malloc_assert(size_t size)
  {
    void* m = malloc(size);
    assert(m != NULL);
    return m;
  }

  static inline void*
  realloc_assert(void* old, size_t size)
  {
    void* m = realloc(old, size);
    assert(m != NULL);
    return m;
  }

  static inline void*
  calloc_assert(size_t n, size_t size)
  {
    void* m = calloc(n, size);
    assert(m != NULL);
    return m;
  }

  extern int mctop_set_cpu(int cpu);

  /* ******************************************************************************** */
  /* MCTOP Allocator */
  /* ******************************************************************************** */

#define MCTOP_ALLOC_NUM 11
  typedef enum 
    {
      MCTOP_ALLOC_NONE,		    /* Do not pin anything! */
      MCTOP_ALLOC_SEQUENTIAL,	    /* Return HWC ids 0, 1, 2, 3, ... */
      MCTOP_ALLOC_MIN_LAT_HWCS,       /* Minimize latency across used sockets. Use HWCs of same core first  */
      MCTOP_ALLOC_MIN_LAT_CORES_HWCS, /* Minimize latency across used sockets. Use physical cores of a socket first,
					 HWCs of that socket after and then proceed to the next socket. */
      MCTOP_ALLOC_MIN_LAT_CORES,      /* Minimize latency across used sockets. Use physical cores first and once all 
					 of them have been used start using HWCs */
      MCTOP_ALLOC_MIN_LAT_HWCS_BALANCE, /* Same as MCTOP_ALLOC_MIN_LAT_HWCS, but balances hwcs across the used sockets. */
      MCTOP_ALLOC_MIN_LAT_CORES_HWCS_BALANCE, /* Same as MCTOP_ALLOC_MIN_LAT_CORES_HWCS, 
						 but balances hwcs across the used sockets.
						 HWCs of that socket after and then proceed to the next socket. */
      MCTOP_ALLOC_MIN_LAT_CORES_BALANCE, /* Same as MCTOP_ALLOC_MIN_LAT_CORES, but balances cores across the used sockets. */
      MCTOP_ALLOC_BW_ROUND_ROBIN_HWCS,  /* Maximize bandwidth to local nodes. Allocate HWCs round robin in terms of sockets. 
					   Use HWCs of the same core first.*/
      MCTOP_ALLOC_BW_ROUND_ROBIN_CORES, /* Maximize bandwidth to local nodes. Allocate HWCs round robin in terms of sockets.
					   Use physical cores first.*/
      MCTOP_ALLOC_BW_BOUND,	    /* Maximize bandwidth to local nodes and calculates the number of cores that are 
				       required to saturate the bandwidth on each node. */
    } mctop_alloc_policy;

  typedef struct mctop_alloc
  {
    mctop_t* topo;
    mctop_alloc_policy policy;
    uint n_hwcs;
    uint n_cores;
    uint n_sockets;
    socket_t** sockets;
    double* bw_proportions;
    uint max_latency;
    double min_bandwidth;
    uint* hwcs;
    volatile uint n_hwcs_used;
    volatile uint8_t* hwcs_used;
    struct bitmask* hwcs_all;
  } mctop_alloc_t;

  typedef struct mctop_thread_info
  {
    mctop_alloc_t* alloc;
    uint is_pinned;
    int id;
    uint hwc_id;
    uint local_node;
    uint nth_socket;
  } mctop_thread_info_t;

  __attribute__((unused)) static const char* mctop_alloc_policy_desc[MCTOP_ALLOC_NUM] = 
    { 
      "MCTOP_ALLOC_NONE",
      "MCTOP_ALLOC_SEQUENTIAL",
      "MCTOP_ALLOC_MIN_LAT_HWCS",
      "MCTOP_ALLOC_MIN_LAT_CORES_HWCS",
      "MCTOP_ALLOC_MIN_LAT_CORES",
      "MCTOP_ALLOC_MIN_LAT_HWCS_BALANCE",
      "MCTOP_ALLOC_MIN_LAT_CORES_HWCS_BALANCE",
      "MCTOP_ALLOC_MIN_LAT_CORES_BALANCE",
      "MCTOP_ALLOC_BW_ROUND_ROBIN_HWCS",
      "MCTOP_ALLOC_BW_ROUND_ROBIN_CORES",
      "MCTOP_ALLOC_BW_BOUND",
    };

#define MCTOP_ALLOC_ALL           -1

  /* Alloc structs manipulation *************************************************************************************** */

  /* mctop_alloc_create params 
   *
   * n_hwcs == MCTOP_ALLOC_ALL : set n_hwcs to the # of hw contexts of the processor
   *
   * MCTOP_ALLOC_SEQUENTIAL           : n_hwcs = total # hw contexts
   *
   * MCTOP_ALLOC_MIN_LAT_HWCS       
   * MCTOP_ALLOC_MIN_LAT_CORES_HWCS
   * MCTOP_ALLOC_MIN_LAT_CORES        : n_hwcs = total # hw contexts / n_config = limit the # of hw contexts per socket
   *                                    pass MCTOP_ALLOC_ALL to get all hw contexts per socket
   *
   * MCTOP_ALLOC_MIN_LAT_HWCS_BALANCE          
   * MCTOP_ALLOC_MIN_LAT_CORES_BALANCE : n_hwcs = total # hw contexts / n_config = ignored
   *                                     pass MCTOP_ALLOC_ALL to get all hw contexts per socket
   *
   * MCTOP_ALLOC_BW_ROUND_ROBIN_HWCS 
   * MCTOP_ALLOC_BW_ROUND_ROBIN_CORES : n_hwcs = total # hw contexts / n_config = how many sockets to use
   * 
   * MCTOP_ALLOC_BW_BOUND             : n_hwcs = how many extra hw contexts to allocate per socket
   *                                    n_config = how many sockets to use
   */
  mctop_alloc_t* mctop_alloc_create(mctop_t* topo, const int n_hwcs, const int n_config, mctop_alloc_policy policy);
  void mctop_alloc_free(mctop_alloc_t* alloc);
  void mctop_alloc_print(mctop_alloc_t* alloc);
  void mctop_alloc_print_short(mctop_alloc_t* alloc);
  void mctop_alloc_help();

  /* Thread functions ************************************************************************************************** */

  int mctop_alloc_pin(mctop_alloc_t* alloc);
  int mctop_alloc_unpin();
  int mctop_alloc_pin_nth_socket(mctop_alloc_t* alloc, const uint nth);
  int mctop_alloc_pin_all(mctop_alloc_t* alloc);

  void mctop_alloc_thread_print();     /* print current threads pin details */
  uint mctop_alloc_is_pinned();	     /* is thread pinned? */
  int mctop_alloc_get_id();	     /* thread id (NOT hw context id). -1 if thread is not pinned. */
  int mctop_alloc_get_hw_context_id(); /* hw context id (the id the we use for set_cpu() */
  int mctop_alloc_get_local_node();    /* local NUMA node of thread */
  int mctop_alloc_get_node_seq_id();   /* sequence id of the node that this thread is using. For example, the allocator
					  could be using sockets [3, 7]. Socket 3 is node seq id 0 and 7 seq id 1. */


  /* Queries *********************************************************************************************************** */

  mctop_alloc_policy mctop_alloc_get_policy(mctop_alloc_t* alloc);
  uint mctop_alloc_get_num_hw_contexts(mctop_alloc_t* alloc);
  const char* mctop_alloc_get_policy_desc(mctop_alloc_t* alloc);
  double mctop_alloc_get_min_bandwidth(mctop_alloc_t* alloc);
  uint mctop_alloc_get_max_latency(mctop_alloc_t* alloc);
  uint mctop_alloc_get_nth_hw_context(mctop_alloc_t* alloc, const uint nth);

  /* # of sockets / nodes that the allocator uses */
  uint mctop_alloc_get_num_sockets(mctop_alloc_t* alloc);
  /* get the OS NUMA node id for the nth socket of the allocator */
  uint mctop_alloc_get_nth_node(mctop_alloc_t* alloc, const uint nth);
  double mctop_alloc_get_nth_socket_bandwidth_proportion(mctop_alloc_t* alloc, const uint nth);

  uint mctop_alloc_ids_get_latency(mctop_alloc_t* alloc, const uint id0, const uint id1);

  /* allocate with libnuma on the node of the nth socket of the allocator */
  void* mctop_alloc_malloc_on_nth_socket(mctop_alloc_t* alloc, const uint nth, const size_t size);
  void mctop_alloc_malloc_free(void* mem, const size_t size);



#ifdef __cplusplus
}
#endif

#endif	/* __H_MCTOP__ */

