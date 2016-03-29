#ifndef __H_NUMA_SPARC__
#define __H_NUMA_SPARC__
#include <sys/lgrp_user.h>

#define  SPARC_LGRP_MAX_NODES 16
extern lgrp_cookie_t lgrp_cookie;

uint numa_num_task_nodes();
int numa_run_on_node(uint node);
int numa_set_preferred(uint node);

void* numa_alloc_interleaved_subset(size_t size, void* mask);
void* numa_alloc_onnode(size_t size, uint node);
void numa_free(void* m, size_t size);

#endif	/* __H_NUMA_SPARC__ */
