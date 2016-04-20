#include <mctop_alloc.h>
#include <getopt.h>

void
mct_print_lats(const char* mct_file)
{
  char file_open[100], hostname[100];
  if (mct_file != NULL)
    {
      sprintf(file_open, "%s", mct_file);
    }
  else
    {
      if (gethostname(hostname, 100) != 0)
	{
	  perror("MCTOP Error: Could not get hostname!");
	}
      sprintf(file_open, "./desc/%s.mct", hostname);
    }

  FILE* ifile = fopen(file_open, "r");
  if (ifile == NULL)
    {
      sprintf(file_open, "/usr/share/mctop/%s.mct", hostname);
      ifile = fopen(file_open, "r");
      if (ifile == NULL)
	{
	  fprintf(stderr, "MCTOP Error: Cannot find MCT file!\n");
	  return;
	}
      else
	{
	  fprintf(stdout, "MCTOP Info: Opened %s!\n", file_open);
	}
    }

  uint n_hwcs, n_sockets, is_smt;
  char discard[4][30];
  int ret = fscanf(ifile, "%s %s %u %s %u %s %u",
		   discard[0], discard[1], &n_hwcs, discard[2], &n_sockets, discard[3], &is_smt);
  if (ret != 7)
    {
      fprintf(stderr, "MCTOP Error: Incorrect MCT file %s! Reason: Header line\n", file_open);
      return;
    }

  uint lat_table[n_hwcs][n_hwcs];

  uint correct = 1;
  for (uint x = 0; x < (n_hwcs * n_hwcs); x++)
    {
      uint xc, yc, lat;
      if (fscanf(ifile, "%u %u %u", &xc, &yc, &lat) != 3)
	{
	  correct = 0;
	  break;
	}
      lat_table[xc][yc] = lat;
    }

  if (correct)
    {
      printf("    ");
      for (uint x = 0; x < n_hwcs; x++)
	{
	  printf("%-3u ", x);
	}
      printf("\n");
      for (uint x = 0; x < n_hwcs; x++)
	{
	  printf("%-3u ", x);
	  for (uint y = 0; y < n_hwcs; y++)
	    {
	      printf("%-3u ", lat_table[x][y]);
	    }
	  printf("\n");
	}
    }
}

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
	  mctop_alloc_help();
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

  if (topo)
    {
      uint lats[topo->n_levels][topo->n_hwcs][topo->n_hwcs];
      uint* nlvl = calloc(topo->n_levels, sizeof(uint));

      //      const uint n_levels = mctop_get_num_levels(topo);
      for (uint lvl = 0; lvl <= topo->socket_level; lvl++)
	{
	  hwc_gs_t* gs0 = mctop_get_first_gs_at_lvl(topo, lvl);
	  while (gs0)
	    {
	      nlvl[lvl]++;
	      //	      printf("%u -", gs0->id);
	      hwc_gs_t* gs1 = mctop_get_first_gs_at_lvl(topo, lvl);
	      while (gs1)
		{
		  const uint id0 = mctop_id_no_lvl(gs0->id);
		  const uint id1 = mctop_id_no_lvl(gs1->id);
		  lats[lvl][id0][id1] = mctop_ids_get_latency(topo, gs0->id, gs1->id);
		  gs1 = gs1->next;
		}
	      gs0 = gs0->next;
	    }
	  //	  printf("\n");
	}

      for (uint lvl = 0; lvl <= topo->socket_level; lvl++)
	{
	  printf("## Lvl %u ############################################################################\n", lvl);
	  printf("    ");
	  for (uint x = 0; x < nlvl[lvl]; x++)
	    {
	      printf("%-3u ", x);
	    }
	  printf("\n");
	  for (uint x = 0; x < nlvl[lvl]; x++)
	    {
	      printf("%-3u ", x);
	      for (uint y = 0; y < nlvl[lvl]; y++)
		{
		  printf("%-3u ", lats[lvl][x][y]);
		}
	      printf("\n");
	    }
	}



      mctop_free(topo);
      free(nlvl);
    }
  return 0;
}

