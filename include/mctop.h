#ifndef __H_MCTOP__
#define __H_MCTOP__

#include <stdio.h>
#include <stdlib.h>

#include <helper.h>
#include <cdf.h>

void mctop_topology_create(uint64_t** lat_table_norm, const size_t N, cdf_cluster_t* cc);

#endif	/* __H_MCTOP__ */
