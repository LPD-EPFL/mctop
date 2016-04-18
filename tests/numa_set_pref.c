#include <mctop.h>
#include <getopt.h>

const size_t msize = (8 * 1024 * 1024 * 1024LL);

int
main(int argc, char **argv) 
{
  //numa_set_localalloc();

  mctop_t* topo = mctop_load(NULL);
  if (topo)
    {
      MCTOP_F_STEP(__steps, __a, __b);
      for (uint s = 0; s < mctop_get_num_nodes(topo); s++)
	{
	  mctop_run_on_node(topo, s);
	  numa_set_preferred(s);
	  printf("-- mctop_run_on_node(%u)\n", s);
	  printf("---- numa_preferred() = %u\n", numa_preferred());

	  MCTOP_P_STEP("pin", __steps, __a, __b, 1);
	  size_t* m = malloc(msize);
	  memset(m, 'f', msize);

	  MCTOP_P_STEP("alloc+access", __steps, __a, __b, 1);
	  /* numa_set_preferred(s); */
	  /* printf("---- numa_preferred() = %u\n", numa_preferred()); */
	}
    }
  return 0;
}
