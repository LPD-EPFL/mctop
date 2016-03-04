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

typedef struct mctopo
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
  double* mem_bandwidths;	/* Mem. bandwidth of each socket, maximum */
  double* mem_bandwidths1;	/* Mem. bandwidth of each socket, single threaded */
} mctopo_t;

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
  mctopo_t* topo;		/* link to topology */
  /* socket only info */
  uint n_siblings;		/* number of other sockets */
  struct sibling** siblings;	/* pointers to other sockets, sorted closest 1st */
  uint local_node;		/* local NUMA mem. node */
  uint n_nodes;			/* num of nodes = topo->n_sockets */
  uint* mem_latencies;		/* mem. latencies to NUMA nodes */
  double* mem_bandwidths;	/* mem. bandwidths to NUMA nodes, maximum */
  double* mem_bandwidths1;	/* mem. bandwidths to NUMA nodes, single threaded */
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

mctopo_t* mctopo_construct(uint64_t** lat_table_norm, const size_t N,
			   uint64_t** mem_lat_table, const uint n_sockets,
			   cdf_cluster_t* cc, const int is_smt);
mctopo_t* mctopo_load(const char* mct_file);
void mctopo_free(mctopo_t* topo);
void mctopo_mem_bandwidth_add(mctopo_t* topo, double** mem_bw_table, double** mem_bw_table1);
void mctopo_mem_latencies_add(mctopo_t* topo, uint64_t** mem_lat_table);
void mctopo_print(mctopo_t* topo);
void mctopo_dot_graph_plot(mctopo_t* topo,  const uint max_cross_socket_lvl);


/* ******************************************************************************** */
/* MCTOP CONTROL IF */
/* ******************************************************************************** */

/* topo getters ******************************************************************* */
socket_t* mctop_get_socket(mctopo_t* topo, const uint socket_n);
socket_t* mctop_get_first_socket(mctopo_t* topo);
hwc_gs_t* mctop_get_first_gs_core(mctopo_t* topo);
hwc_gs_t* mctop_get_first_gs_at_lvl(mctopo_t* topo, const uint lvl);
sibling_t* mctop_get_first_sibling_lvl(mctopo_t* topo, const uint lvl);

size_t mctop_get_num_nodes(mctopo_t* topo);
size_t mctop_get_num_cores_per_socket(mctopo_t* topo);
size_t mctop_get_num_hwc_per_socket(mctopo_t* topo);

/* socket getters ***************************************************************** */
hw_context_t* mctop_socket_get_first_hwc(socket_t* socket);
hwc_gs_t* mctop_socket_get_first_gs_core(socket_t* socket);
hwc_gs_t* mctop_socket_get_first_child_lvl(socket_t* socket, const uint lvl);
size_t mctop_socket_get_num_cores(socket_t* socket);

/* queries ************************************************************************ */
uint mctop_are_hwcs_same_core(hw_context_t* a, hw_context_t* b);
uint mctop_has_mem_lat(mctopo_t* topo);
uint mctop_has_mem_bw(mctopo_t* topo);

/* sibling getters ***************************************************************** */
socket_t* mctop_sibling_get_other_socket(sibling_t* sibling, socket_t* socket);

/* optimizing ********************************************************************** */
int mctop_hwcid_fix_numa_node(mctopo_t* topo, const uint hwcid);


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
int mctop_run_on_socket(mctopo_t* topo, const uint socket_n);
int mctop_run_on_socket_nm(mctopo_t* topo, const uint socket_n); /* doea not set preferred node */
int mctop_run_on_node(mctopo_t* topo, const uint node_n);

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

typedef enum 
  {
    MCTOPO_ALLOC_MIN_LAT,
    MCTOPO_ALLOC_MIN_LAT_CORES,
  } mctopo_alloc_policy;

static const char* mctopo_alloc_policy_desc[] = 
{ 
  "MCTOPO_ALLOC_MIN_LAT",
  "MCTOPO_ALLOC_MIN_LAT_CORES",
};

typedef struct mctopo_alloc
{
  mctopo_t* topo;
  mctopo_alloc_policy policy;
  uint n_hwcs;
  uint n_sockets;
  socket_t** sockets;
  uint max_latency;
  double min_bandwidth;
  uint32_t cur;
  uint hwcs[0];
} mctopo_alloc_t;

mctopo_alloc_t* mctopo_alloc_create(mctopo_t* topo, const uint n_hwcs, mctopo_alloc_policy policy);
void mctopo_alloc_free(mctopo_alloc_t* alloc);
void mctopo_alloc_print(mctopo_alloc_t* alloc);


int mctopo_alloc_pin(mctopo_alloc_t* alloc);
int mctopo_alloc_pin_all(mctopo_alloc_t* alloc);
int mctopo_alloc_get_hwc_id();

#endif	/* __H_MCTOP__ */
