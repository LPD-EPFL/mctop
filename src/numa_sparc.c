#ifdef __sparc__
#  include <stdio.h>
#  include <sys/lgrp_user.h>
#  include <malloc.h>
#  include <strings.h>

#include <numa_sparc.h>

extern int mctop_set_cpu(int cpu);

lgrp_cookie_t lgrp_cookie;

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
  /* int core = numa_to_core[node]; */
  /* uint core_prev = getcpuid(); */

  /* if (core < 0) */
  /*   { */
  /*     int c = 0; */
  /*     while (1) */
  /* 	{ */
  /* 	  if (!set_cpu(c)) */
  /* 	    { */
  /* 	      break; */
  /* 	    } */

  /* 	  int node_home = gethomelgroup() - 1; */
  /* 	  if (node_home == node) */
  /* 	    { */
  /* 	      numa_to_core[node] = c; */
  /* 	      set_cpu(c); */
  /* 	      break; */
  /* 	    } */
  /* 	  c++; */
  /* 	} */
  /*   } */
  /* else */
  /*   { */
  /*     set_cpu(core); */
  /*   } */
  /* void* m = malloc(size); */
  /* if (m != NULL) */
  /*   { */
  /*     bzero(m, size); */
  /*   } */
  /* set_cpu(core_prev); */
  void* m = malloc(size);
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
  int core = numa_to_core[node];
  if (core < 0)
    {
      int c = 0;
      while (1)
	{
	  if (!mctop_set_cpu(c))
	    {
	      break;
	    }

	  int node_home = gethomelgroup() - 1;
	  if (node_home == node)
	    {
	      numa_to_core[node] = c;
	      mctop_set_cpu(c);
	      break;
	    }
	  c++;
	}
    }
  else
    {
      mctop_set_cpu(core);
    }

  return 1;
}

void
numa_free(void* m, size_t size)
{
  free(m);
}

#endif
