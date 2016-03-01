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
	  exit(0);
	case '?':
	  printf("Use -h or --help for help\n");
	  exit(0);
	default:
	  exit(1);
	}
    }

  mctopo_t* topo;
  if (manual_file)
    {
      topo = mctopo_load(mct_file);
    }
  else
    {
      topo = mctopo_load(NULL);
    }

  if (topo != NULL)
    {
      mctopo_print(topo);
      mctopo_dot_graph_plot(topo);

      mctopo_free(topo);
    }
  return 0;
}
