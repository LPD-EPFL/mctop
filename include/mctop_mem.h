#ifndef __H_MCTOP_MEM_H_
#define __H_MCTOP_MEM_H_

#ifdef __sparc__
#  include "numa_sparc.h"
#endif

void* mctop_mem_alloc_local(size_t size, int node);
void mctop_mem_free(void* mem, size_t size, int numa_lib);


#endif	/* __H_MCTOP_MEM_H_ */
