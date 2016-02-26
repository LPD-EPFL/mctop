#ifndef __H_MCTOP__
#define __H_MCTOP__

#include <stdio.h>
#include <stdlib.h>

#include <helper.h>
#include <cdf.h>

struct mctopo* mctopo_construct(uint64_t** lat_table_norm, const size_t N,
				uint64_t** mem_lat_table, const uint n_sockets,
				cdf_cluster_t* cc, const int is_smt);
void mctopo_mem_latencies_calc(struct mctopo* topo, uint64_t** mem_lat_table);
void mctopo_mem_bandwidth_add(struct mctopo* topo, double** mem_bw_table);

void mctopo_print(struct mctopo* topo);

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
    NONE,
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
  uint n_siblings;
  struct sibling** siblings;
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
  double* mem_bandwidths;	/* mem. bandwidts to NUMA nodes */
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
/* MCTOP CONTROL IF */
/* ******************************************************************************** */

socket_t* mctop_get_socket(mctopo_t* topo, const uint socket_n);
uint mctop_are_hwcs_same_core(hw_context_t* a, hw_context_t* b);
socket_t* mctop_get_first_socket(mctopo_t* topo);
hwc_gs_t* mctop_get_first_gs_at_lvl(mctopo_t* topo, const uint lvl);
hw_context_t* mctop_get_first_hwc_socket(socket_t* socket);
sibling_t* mctop_get_first_sibling_lvl(mctopo_t* topo, const uint lvl);

size_t mctop_get_num_nodes(mctopo_t* topo);
size_t mctop_get_num_cores_per_socket(mctopo_t* topo);
size_t mctop_get_num_hwc_per_socket(mctopo_t* topo);


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
int mctop_run_on_socket_ref(socket_t* socket);
int mctop_run_on_socket(mctopo_t* topo, const uint socket_n);
int mctop_run_on_node(mctopo_t* topo, const uint node_n);

#endif	/* __H_MCTOP__ */
