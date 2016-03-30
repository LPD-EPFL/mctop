#include <mctop.h>
#include <getopt.h>

const size_t msize = (2 * 1024 * 1024LL);

int
main(int argc, char **argv) 
{
  int on = 0;
  if (argc > 1)
    {
      on = atoi(argv[1]);
    }

  printf("On node %d\n", on);

  // NULL for automatically loading the MCT file based on the hostname of the machine
  mctop_t* topo = mctop_load(NULL);
  if (topo)
    {
      mctop_run_on_node(topo, on);
      volatile uint** mem[mctop_get_num_nodes(topo)];
      for (int i = 0; i < mctop_get_num_nodes(topo); i++)
	{
	  mem[i] = numa_alloc_onnode(msize, i);
	  assert(mem[i] != NULL);
	}

      printf(" -- Mem intialized\n");
      mctop_run_on_node(topo, on);

      //      mctop_print(topo);
      volatile size_t i = 20e9;
      while (i--)
	{
	  __asm volatile ("nop");
	}

      for (int i = 0; i < mctop_get_num_nodes(topo); i++)
	{
	  numa_free(mem[i], msize);
	}

      mctop_free(topo);
    }
  return 0;
}
