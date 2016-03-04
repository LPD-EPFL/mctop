#include <assert.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <malloc.h>
#ifdef __x86_64__
#  include <numa.h>
#endif

#include <mctop_mem.h>

void*
mctop_mem_alloc_local(size_t size, int node) 
{
  void* mem;
  if (node >= 0)
    {
      mem = numa_alloc_onnode(size, node);
    }
  else
    {
      mem = malloc(size);
/*       mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0); */
/* #ifdef __sparc__ */
/*       if (mem != MAP_FAILED) */
/* 	{ */
/* 	  int r = madvise((caddr_t) mem, size, MADV_ACCESS_LWP); */
/* 	  assert(r == 0 && "madvise failed"); */
/* 	} */
/* #endif */
//      assert(mem != MAP_FAILED);
    }

  assert(mem != NULL);
  bzero(mem, size);
  return mem;
}

 
void
mctop_mem_free(void* mem, size_t size, int numa_lib) 
{
  if (numa_lib)
    {
      numa_free(mem, size);
    }
  else
    {
      //      int r = munmap(mem, size);
      //      assert(r == 0 && "munmap failed");
      free(mem);
    }
}
