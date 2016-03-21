#include <mctop.h>
#include <time.h>

mctop_t*
mctop_load(const char* mct_file)
{
  clock_t cstart = clock();
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
	  return NULL;
	}
      else
	{
	  fprintf(stderr, "MCTOP Info: Opened %s!\n", file_open);
	}
    }

  uint n_hwcs, n_sockets, is_smt;
  char discard[4][30];
  int ret = fscanf(ifile, "%s %s %u %s %u %s %u",
		   discard[0], discard[1], &n_hwcs, discard[2], &n_sockets, discard[3], &is_smt);
  if (ret != 7)
    {
      fprintf(stderr, "MCTOP Error: Incorrect MCT file %s!\n", file_open);
      return NULL;
    }

  uint64_t** lat_table = (uint64_t**) table_malloc(n_hwcs, n_hwcs, sizeof(uint64_t));
  uint64_t** mem_lat_table = (uint64_t**) table_calloc(n_hwcs, n_hwcs, sizeof(uint64_t));
  double** mem_bw_table_r = (double**) table_malloc(n_hwcs, n_sockets, sizeof(double));
  double** mem_bw_table1_r = (double**) table_malloc(n_hwcs, n_sockets, sizeof(double));
  double** mem_bw_table_w = (double**) table_malloc(n_hwcs, n_sockets, sizeof(double));
  double** mem_bw_table1_w = (double**) table_malloc(n_hwcs, n_sockets, sizeof(double));

  uint correct = 1, has_lat = 1;;
  for (uint x = 0; x < (n_hwcs * n_hwcs); x++)
    {
      uint xc, yc, lat;
      int ret = fscanf(ifile, "%u %u %u", &xc, &yc, &lat);
      if (ret != 3)
	{
	  fprintf(stderr, "MCTOP Error: Incorrect MCT file %s!\n", file_open);
	  has_lat = 0;
	  correct = 0;
	  break;
	}
      lat_table[xc][yc] = lat;
    }

  uint has_mem_lat = 1;
  if (correct)
    {
      uint ns;
      int ret = fscanf(ifile, "%s %u", discard[0], &ns);
      if (ret != 2)
	{
	  fprintf(stderr, "MCTOP Warning: No memory latency measurements in %s!\n", file_open);
	  has_mem_lat = 0;
	}
      else
	{
	  for (uint x = 0; x < (n_hwcs * n_sockets); x++)
	    {
	      uint xc, yc, mlat;
	      int ret = fscanf(ifile, "%u %u %u", &xc, &yc, &mlat);
	      if (ret != 3)
		{
		  fprintf(stderr, "MCTOP Error: Incorrect MCT file %s!\n", file_open);
		  correct = 0;
		  has_mem_lat = 0;
		  break;
		}
	      mem_lat_table[xc][yc] = mlat;
	    }
	}
    }

  uint has_mem_bw = 1;
  if (correct)
    {
      uint ns;
      int ret = fscanf(ifile, "%s %u", discard[0], &ns);
      if (ret != 2)
	{
	  fprintf(stderr, "MCTOP Warning: No max read memory bandwidth measurements in %s!\n", file_open);
	  has_mem_bw = 0;
	}
      else
	{
	  for (uint x = 0; x < (n_sockets * n_sockets); x++)
	    {
	      uint xc, yc;
	      double bw;
	      int ret = fscanf(ifile, "%u %u %lf", &xc, &yc, &bw);
	      if (ret != 3)
		{
		  fprintf(stderr, "MCTOP Error: Incorrect MCT file %s!\n", file_open);
		  has_mem_bw = 0;
		  correct = 0;
		  break;
		}
	      mem_bw_table_r[xc][yc] = bw;
	    }
	}
    }

  if (correct)
    {
      uint ns;
      int ret = fscanf(ifile, "%s %u", discard[0], &ns);
      if (ret != 2)
	{
	  fprintf(stderr, "MCTOP Warning: No single-threaded read memory bandwidth measurements in %s!\n", file_open);
	  has_mem_bw = 0;
	}
      else
	{
	  for (uint x = 0; x < (n_sockets * n_sockets); x++)
	    {
	      uint xc, yc;
	      double bw;
	      int ret = fscanf(ifile, "%u %u %lf", &xc, &yc, &bw);
	      if (ret != 3)
		{
		  fprintf(stderr, "MCTOP Error: Incorrect MCT file %s!\n", file_open);
		  has_mem_bw = 0;
		  correct = 0;
		  break;
		}
	      mem_bw_table1_r[xc][yc] = bw;
	    }
	}
    }

  if (correct)
    {
      uint ns;
      int ret = fscanf(ifile, "%s %u", discard[0], &ns);
      if (ret != 2)
	{
	  fprintf(stderr, "MCTOP Warning: No max write memory bandwidth measurements in %s!\n", file_open);
	  has_mem_bw = 0;
	}
      else
	{
	  for (uint x = 0; x < (n_sockets * n_sockets); x++)
	    {
	      uint xc, yc;
	      double bw;
	      int ret = fscanf(ifile, "%u %u %lf", &xc, &yc, &bw);
	      if (ret != 3)
		{
		  fprintf(stderr, "MCTOP Error: Incorrect MCT file %s!\n", file_open);
		  has_mem_bw = 0;
		  correct = 0;
		  break;
		}
	      mem_bw_table_w[xc][yc] = bw;
	    }
	}
    }

  if (correct)
    {
      uint ns;
      int ret = fscanf(ifile, "%s %u", discard[0], &ns);
      if (ret != 2)
	{
	  fprintf(stderr, "MCTOP Warning: No single-threaded write memory bandwidth measurements in %s!\n", file_open);
	  has_mem_bw = 0;
	}
      else
	{
	  for (uint x = 0; x < (n_sockets * n_sockets); x++)
	    {
	      uint xc, yc;
	      double bw;
	      int ret = fscanf(ifile, "%u %u %lf", &xc, &yc, &bw);
	      if (ret != 3)
		{
		  fprintf(stderr, "MCTOP Error: Incorrect MCT file %s!\n", file_open);
		  has_mem_bw = 0;
		  correct = 0;
		  break;
		}
	      mem_bw_table1_w[xc][yc] = bw;
	    }
	}
    }

  mctop_t* topo = NULL;
  if (correct && has_lat)
    {
      uint64_t** mlat = (has_mem_lat) ? mem_lat_table : NULL;
      topo = mctop_construct(lat_table, n_hwcs, mlat, n_sockets, NULL, is_smt);
      if (has_mem_bw)
	{
	  mctop_mem_bandwidth_add(topo, mem_bw_table_r, mem_bw_table1_r, mem_bw_table_w, mem_bw_table1_w);
	}
    }

  table_free((void**) lat_table, n_hwcs);
  table_free((void**) mem_lat_table, n_hwcs);
  table_free((void**) mem_bw_table_r, n_hwcs);
  table_free((void**) mem_bw_table1_r, n_hwcs);
  table_free((void**) mem_bw_table_w, n_hwcs);
  table_free((void**) mem_bw_table1_w, n_hwcs);
  fclose(ifile);

  double dur = (clock() - cstart) / (double) CLOCKS_PER_SEC;
  printf("MCTOP Info: Topology loaded in %.3f ms\n", 1000 * dur);
  return topo;
}
