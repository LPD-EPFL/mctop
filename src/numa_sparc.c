#ifdef __sparc__
#  include <stdio.h>
#  include <sys/lgrp_user.h>
#  include <malloc.h>
#  include <strings.h>
#  include <sys/types.h>
#  include <sys/mman.h>

#  include <numa_sparc.h>

extern int mctop_set_cpu(int cpu);

lgrp_cookie_t lgrp_cookie;
volatile uint lgrp_cookie_init = 0;

static int numa_to_core[64] = 
  {
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
  };

uint
numa_num_task_nodes()
{
  return lgrp_nlgrps(lgrp_cookie) - 1;
}

int 
numa_set_preferred(uint node)
{
  return 1;
}

void*
numa_alloc_onnode(size_t size, uint node)
{
  /* lgrp_id_t root = lgrp_root(lgrp_cookie); */
  /* lgrp_affinity_t current = lgrp_affinity_get(P_LWPID, P_MYID, root); */
  /* printf("--> I'm on %d\n", current); */
  numa_run_on_node(node);
  /* current = lgrp_affinity_get(P_LWPID, P_MYID, root); */
  /* printf("<-- I'm on %d\n", current); */

  void* m = malloc(size);
  if (m != NULL)
    {
      madvise(m, size, MADV_ACCESS_LWP);
      *(volatile int*) m = 0;
      bzero(m, size);
    }

  /* lgrp_affinity_set(P_LWPID, P_MYID, current, LGRP_AFF_STRONG); */
  return m;
}

void* 
numa_alloc_interleaved_subset(size_t size, void* mask)
{
  void* m = malloc(size);
  return m;
}

int
numa_run_on_node(uint node)
{
  lgrp_id_t root = lgrp_root(lgrp_cookie);
  lgrp_id_t lgrp_array[SPARC_LGRP_MAX_NODES];
  int ret = lgrp_children(lgrp_cookie, root, lgrp_array, SPARC_LGRP_MAX_NODES);
  int cret = ret;
  ret = ret && lgrp_affinity_set(P_LWPID, P_MYID, lgrp_array[node], LGRP_AFF_STRONG);
  return ret;
}

void
numa_free(void* m, size_t size)
{
  free(m);
}

#endif
