#include <mctop.h>
#include <getopt.h>

int
main(int argc, char **argv) 
{
  mctopo_t* topo = mctop_load(NULL);
  if (topo)
    {
      mctopo_print(topo);
      mctop_run_on_node(topo, 0);
    }

  return 0;
}
