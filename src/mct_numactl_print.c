#include <mctop.h>
#include <getopt.h>
#include <string.h>

int
main(int argc, char **argv) 
{
  char mct_file[100];
  uint manual_file = 0;

  struct option long_options[] = 
    {
      // These options don't set a flag
      {"help",                      no_argument,             NULL, 'h'},
      {"mct",                       required_argument,       NULL, 'm'},
      {NULL, 0, NULL, 0}
    };

  int i;
  char c;
  while(1) 
    {
      i = 0;
      c = getopt_long(argc, argv, "hm:", long_options, &i);

      if(c == -1)
	break;

      if(c == 0 && long_options[i].flag == 0)
	c = long_options[i].val;

      switch(c) 
	{
	case 0:
	  /* Flag is automatically set */
	  break;
	case 'm':
	  sprintf(mct_file, "%s", optarg);
	  manual_file = 1;
	  break;
	case 'h':
	  printf("mctop_load  Copyright (C) 2016  Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>\n"
		 "This program comes with ABSOLUTELY NO WARRANTY.\n"
		 "mctop_load loads and prints the graphs of multi-core MCT files.\n"
		 ""
		 "Usage: ./mctop_load [options...]\n"
		 "\n"
		 "Options:\n"
		 "  -m, --mct <string>\n"
		 "        Path to MCT file to print. By default, the file corresponding to the hostname is printed.\n"
		 );
	  exit(0);
	case '?':
	  printf("Use -h or --help for help\n");
	  exit(0);
	default:
	  exit(1);
	}
    }

  mctop_t* topo;
  if (manual_file)
    {
      topo = mctop_load(mct_file);
    }
  else
    {
      topo = mctop_load(NULL);
    }

  if (topo != NULL)
    {
      const uint num_nodes = mctop_get_num_nodes(topo);
      printf("available: %u nodes (0-%u)\n", num_nodes, num_nodes - 1);
      for (uint n = 0; n < num_nodes; n++)
	{
	  printf("node %u cpus:", n);
	  socket_t* socket = mctop_node_to_socket(topo, n);
	  const size_t n_hwcs = mctop_socket_get_num_hw_contexts(socket);
	  hw_context_t* hwc = mctop_socket_get_first_hwc(socket);
	  for (uint i = 0; i < n_hwcs; i++)
	    {
	      printf(" %u", hwc->phy_id);
	      hwc = hwc->next;
	    }

	  printf("\n");
	}

      if (topo->has_mem)
	{
	  uint latencies[num_nodes][num_nodes];
	  uint min = -1;
	  for (uint n = 0; n < num_nodes; n++)
	    {
	      socket_t* s0 = mctop_node_to_socket(topo, n);
	      for (uint j = 0; j < num_nodes; j++)
		{
		  socket_t* s1 = mctop_node_to_socket(topo, j);
		  uint lat = mctop_ids_get_latency(topo, s0->id, s1->id);
		  if (lat < min)
		    {
		      min = lat;
		    }
		  latencies[n][j] = lat;
		}
	    }

	  double div = min / 10.0;

	  printf("node distances: \nnode");
	  for (uint n = 0; n < num_nodes; n++)
	    {
	      printf(" %3u", n);
	    }	  
	  printf("\n");
	  for (uint n = 0; n < num_nodes; n++)
	    {
	      printf("%3u:", n);
	      for (uint j = 0; j < num_nodes; j++)
		{
		  printf(" %3u", (uint)(0.5 + (latencies[n][j] / div)));
		}
	      printf("\n");
	    }	  
	}
    }
  return 0;
}
