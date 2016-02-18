#ifndef __H_MCTOP__
#define __H_MCTOP__

#include <stdio.h>
#include <stdlib.h>

#include <helper.h>
#include <cdf.h>

struct mctopo* mctopo_construct(uint64_t** lat_table_norm, const size_t N, cdf_cluster_t* cc);
void mctopo_print(struct mctopo* topo);


typedef unsigned int uint;

typedef struct mctopo
{
  uint n_sockets;		/* num. of sockets/nodes */
  struct socket* sockets;	/* pointer to sockets/nodes */
  uint is_smt;			/* is SMT enabled CPU */
  uint n_levels;		/* num. of latency lvls */
  uint* latencies;		/* latency per level */
  uint n_hwcs;			/* num. of hwcs in this machine */
  struct hw_context* hwcs;	/* pointers to hwcs */
} mctopo_t;

typedef struct socket
{
  uint id;			/* mctop id */
  uint lvl;			/* latency hierarchy lvl */
  uint node_id;			/* local node id */
  uint latency;			/* comm. latency within socket */
  uint is_smt;			/* is SMT enabled CPU */
  uint n_hwcs;			/* num. of hwcs descendants */
  struct hw_context** hwcs;	/* descendant hwcs */
  uint n_children;		/* num. of hwc_group descendants */
  struct hwc_group* children;	/* pointer to children hwcgroup */
  uint n_siblings;		/* number of other sockets */
  struct sibling** siblings;	/* pointers to other sockets, sorted from the closest */
} socket_t;

typedef struct sibling
{
  uint id;			/* needed?? */
  uint lvl;			/* latency hierarchy lvl */
  uint latency;			/* comm. latency across this hop */
  socket_t* from;		/* from -->sibling--> to */
  socket_t* to;			/* to   -->sibling--> from */
} sibling_t;

typedef struct hwc_group
{
  uint id;			/* mctop id */
  uint lvl;			/* latency hierarchy lvl */
  uint latency;			/* comm. latency within group */
  socket_t* socket;		/* pointer to parent socket */
  uint n_hwcs;			/* num. of hwcs descendants */
  struct hw_context** hwcs;	/* descendant hwcs */
  struct hwc_group* parent;	/* pointer to parent hwcgroup */
  uint n_children;		/* num. of hwc_group descendants */
  struct hwc_group* children;	/* pointer to children hwcgroup */
  uint is_core;			/* is it a physical core (if SMT) */
} hwc_group_t;

typedef struct hw_context
{
  uint id;			/* mctop id */
  uint lvl;			/* latency hierarchy lvl */
  uint phy_id;			/* physical OS is */
  socket_t* socket;		/* pointer to parent socket */
  hwc_group_t* parent;		/* pointer to parent hwcgroup */
  uint is_smt;			/* is it SMT hw context? */
} hw_context_t;


#endif	/* __H_MCTOP__ */
