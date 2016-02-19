#ifndef __H_MCTOP__
#define __H_MCTOP__

#include <stdio.h>
#include <stdlib.h>

#include <helper.h>
#include <cdf.h>

struct mctopo* mctopo_construct(uint64_t** lat_table_norm, const size_t N, cdf_cluster_t* cc);
void mctopo_print(struct mctopo* topo);

typedef enum
  {
    HW_CONTEXT,
    CORE,
    HWC_GROUP,
    SOCKET,
  } mctop_type_t;


typedef unsigned int uint;
typedef struct hwc_gs socket_t;
typedef struct hwc_gs hwc_group_t;

typedef struct mctopo
{
  uint n_sockets;		/* num. of sockets/nodes */
  socket_t* sockets;		/* pointer to sockets/nodes */
  uint is_smt;			/* is SMT enabled CPU */
  uint n_levels;		/* num. of latency lvls */
  uint* latencies;		/* latency per level */
  uint n_hwcs;			/* num. of hwcs in this machine */
  struct hw_context* hwcs;	/* pointers to hwcs */
} mctopo_t;

typedef struct hwc_gs		/* group / socket */
{
  uint id;			/* mctop id */
  uint lvl;			/* latency hierarchy lvl */
  mctop_type_t type;		/* HWC_GROUP or SOCKET */
  uint latency;			/* comm. latency within group */
  union
  {
    socket_t* socket;		/* Group: pointer to parent socket */
    uint node_id;		/* Socket: Glocal node id */
    uint is_smt;		/* Socket: is SMT enabled CPU */
  };
  uint n_hwcs;			/* num. of hwcs descendants */
  struct hw_context** hwcs;	/* descendant hwcs */
  uint n_children;		/* num. of hwc_group descendants */
  struct hwc_gs* children;	/* pointer to children hwcgroup */
  struct hwc_gs* parent;	/* Group: pointer to parent hwcgroup */
  uint n_siblings;		/* Socket: number of other sockets */
  struct sibling** siblings;	/* Group = NULL - no siblings for groups */
				/* Socket: pointers to other sockets, sorted closest 1st */
} hwc_gs_t;

typedef struct sibling
{
  uint id;			/* needed?? */
  uint lvl;			/* latency hierarchy lvl */
  uint latency;			/* comm. latency across this hop */
  socket_t* from;		/* from -->sibling--> to */
  socket_t* to;			/* to   -->sibling--> from */
} sibling_t;

typedef struct hw_context
{
  uint id;			/* mctop id */
  uint lvl;			/* latency hierarchy lvl */
  uint phy_id;			/* physical OS is */
  socket_t* socket;		/* pointer to parent socket */
  hwc_group_t* parent;		/* pointer to parent hwcgroup */
  mctop_type_t type;		/* HW_CONTEXT or CORE? */
} hw_context_t;


#endif	/* __H_MCTOP__ */
