#include <mctop.h>
#include <getopt.h>

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
      mctop_print(topo);
      mctop_run_on_node(topo, on);
      volatile long i = 10e9;
      while (i--)
	{
	  __asm volatile ("nop");
	}
      mctop_free(topo);
    }
  return 0;
}
