#include <mctop.h>
#include <getopt.h>
#include <string.h>

int
main(int argc, char **argv) 
{
  char mct_file[100];
  uint manual_file = 0;
  uint test_do_dot = 1;
  uint max_cross_socket_lvl = 0;

  struct option long_options[] = 
    {
      // These options don't set a flag
      {"help",                      no_argument,             NULL, 'h'},
      {"mct",                       required_argument,       NULL, 'm'},
      {"level",                     required_argument,       NULL, 'l'},
      {"no-dot",                    no_argument,             NULL, 'n'},
      {NULL, 0, NULL, 0}
    };

  int i;
  char c;
  while(1) 
    {
      i = 0;
      c = getopt_long(argc, argv, "hm:l:n", long_options, &i);

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
	case 'l':
	  max_cross_socket_lvl = atoi(optarg);
	  break;
	case 'n':
	  test_do_dot = 0;
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
		 "  -l, --level <int>\n"
		 "        How many latency levels should be printed in the dot file output. This option can be used to\n"
		 "        make cross-socket plots more visible, by restricting which links that connect sockets to be plotted.\n"
		 "        For instance, if a multi-core contains 5 latency levels, out of which 2 are cross socket, -l4 will\n"
		 "        only plot level 4 for cross socket.\n"
		 "  -n, --no-dot\n"
		 "        Do not plot the dot graph representation.\n"
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
      mctop_print(topo);
      if (test_do_dot)
	{
	  mctop_dot_graph_plot(topo, max_cross_socket_lvl);
	}
    }
  return 0;
}
