#include <mctop.h>
#include <mctop_internal.h>
#include <helper.h>
#include <time.h>

double*** mctop_power_measurements_create(const uint n_sockets);
void mctop_power_measurements_free(double*** m, const uint n_sockets);

typedef enum
  {
    MEM_LAT,
    MEM_BW_READ,
    MEM_BW1_READ,
    MEM_BW_WRITE,
    MEM_BW1_WRITE,
    CACHE,
    POWER,
    UKNOWN,
    MCTOP_DTYPE_N,
  }  mctop_dtype_t;

const char* mctop_dtypes[] =
  {
    "#Mem_latencies",
    "#Mem_bw-READ",
    "#Mem_bw1-READ",
    "#Mem_bw-WRITE",
    "#Mem_bw1-WRITE",
    "#Cache_levels",
    "#Power_measurements",
    "Uknown header",
    "Invalid measurements",
  };

static mctop_dtype_t
mctop_dtype_get(const char* in)
{
  for (int i = 0; i < MCTOP_DTYPE_N; i++)
    {
      if (strcmp(in, mctop_dtypes[i]) == 0)
	{
	  return (mctop_dtype_t) i;
	}
    }
  return UKNOWN;
}


static uint
mctop_load_mem_lat(FILE* ifile, const uint n_hwcs, const uint n_sockets, uint64_t** mem_lat_table)
{
  for (uint x = 0; x < (n_hwcs * n_sockets); x++)
    {
      uint xc, yc, mlat;
      if (fscanf(ifile, "%u %u %u", &xc, &yc, &mlat) != 3)
	{
	  return 0;
	}
      mem_lat_table[xc][yc] = mlat;
    }
  return 1;
}

static uint
mctop_load_mem_bw(FILE* ifile, const uint n_sockets, double** bw_table)
{
  for (uint x = 0; x < (n_sockets * n_sockets); x++)
    {
      uint xc, yc;
      double bw;
      if (fscanf(ifile, "%u %u %lf", &xc, &yc, &bw) != 3)
	{
	  return 0;
	}
      bw_table[xc][yc] = bw;
    }
  return 1;
}

static mctop_cache_info_t*
mctop_load_cache_info(mctop_cache_info_t* existing, FILE* ifile, const uint n_levels)
{
  mctop_cache_info_t* mci = mctop_cache_info_create(n_levels);
  for (int i = 0; i < n_levels; i++)
    {
      char null[4][100];
      int l;
      uint64_t lat, size_OS, size_est;
      if(fscanf(ifile, "%s %d %s %zu %s %zu %s %zu",
		null[0], &l, null[1], &lat, null[2], &size_OS, null[3], &size_est) != 8)
	{
	  mctop_cache_info_free(mci);
	  return existing;
	}
      mci->latencies[i] = lat;
      mci->sizes_OS[i] = size_OS;
      mci->sizes_estimated[i] = size_est;
    }

  if (existing != NULL)
    {
      mctop_cache_info_free(existing);
    }

  return mci;
}
		   
static uint
mctop_load_pow_info(FILE* ifile, const uint n_sockets, double*** pm)
{
  for (uint s = 0; s <= n_sockets; s++)
    {
      char nothing[40];
      double pow[MCTOP_POW_COMP_TYPE_NUM];
      for (uint type = 0; type < MCTOP_POW_TYPE_NUM; type++)
	{
	  if (fscanf(ifile, "%s %lf %lf %lf %lf %lf", nothing, &pow[0], &pow[1], &pow[2], &pow[3], &pow[4]) != 6)
	    {
	      return 0;
	    }
	  __copy_doubles(pm[type][s], pow, MCTOP_POW_COMP_TYPE_NUM, 1);
	}
    }
  return 1;
}

mctop_t*
mctop_load(const char* mct_file)
{
  clock_t cstart = clock();

#if defined(__sparc__)
  lgrp_cookie_initialize();
#endif

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
	  fprintf(stdout, "MCTOP Info: Opened %s!\n", file_open);
	}
    }

  mctop_dtype_t type = MCTOP_DTYPE_N;

  uint n_hwcs, n_sockets, is_smt;
  char discard[4][30];
  int ret = fscanf(ifile, "%s %s %u %s %u %s %u",
		   discard[0], discard[1], &n_hwcs, discard[2], &n_sockets, discard[3], &is_smt);
  if (ret != 7)
    {
      fprintf(stderr, "MCTOP Error: Incorrect MCT file %s! Reason: Header line\n", file_open);
      return NULL;
    }

  uint64_t** lat_table = (uint64_t**) table_malloc(n_hwcs, n_hwcs, sizeof(uint64_t));
  uint64_t** mem_lat_table = (uint64_t**) table_calloc(n_hwcs, n_hwcs, sizeof(uint64_t));
  double** mem_bw_table_r = (double**) table_malloc(n_hwcs, n_sockets, sizeof(double));
  double** mem_bw_table1_r = (double**) table_malloc(n_hwcs, n_sockets, sizeof(double));
  double** mem_bw_table_w = (double**) table_malloc(n_hwcs, n_sockets, sizeof(double));
  double** mem_bw_table1_w = (double**) table_malloc(n_hwcs, n_sockets, sizeof(double));
  double*** pow_measurements = mctop_power_measurements_create(n_sockets);
  
  mctop_cache_info_t* cache_info = NULL;

  uint8_t* have_data = calloc_assert(MCTOP_DTYPE_N, sizeof(uint8_t));

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

  while (correct)
    {
      char dtype[128];
      int param;
      if (fscanf(ifile, "%s %d", dtype, &param) == 2)
	{
	  type = mctop_dtype_get(dtype);
	  if (have_data[type])
	    {
	      fprintf(stderr, "MCTOP Warning: Duplicate data for %s in in %s!\n", 
		      mctop_dtypes[type], file_open);
	    }
	  have_data[type] = 1;
	  switch (type)
	    {
	    case MEM_LAT:
	      correct = mctop_load_mem_lat(ifile, n_hwcs, n_sockets, mem_lat_table);
	      break;
	    case MEM_BW_READ:
	      correct = mctop_load_mem_bw(ifile, n_sockets, mem_bw_table_r);
	      break;
	    case MEM_BW1_READ:
	      correct = mctop_load_mem_bw(ifile, n_sockets, mem_bw_table1_r);
	      break;
	    case MEM_BW_WRITE:
	      correct = mctop_load_mem_bw(ifile, n_sockets, mem_bw_table_w);
	      break;
	    case MEM_BW1_WRITE:
	      correct = mctop_load_mem_bw(ifile, n_sockets, mem_bw_table1_w);
	      break;
	    case CACHE:
	      cache_info = mctop_load_cache_info(cache_info, ifile, param);
	      correct = (cache_info != NULL);
	      break;
	    case POWER:
	      correct = mctop_load_pow_info(ifile, n_sockets, pow_measurements);
	      break;
	    case UKNOWN:
	    case MCTOP_DTYPE_N:	/* just for compilation warning */
	      correct = 0;
	      break;
	    }
	}
      else
	{
	  break;
	}
    }

  mctop_t* topo = NULL;
  if (correct)
    {
      uint64_t** mlat = have_data[MEM_LAT] ? mem_lat_table : NULL;
      topo = mctop_construct(lat_table, n_hwcs, mlat, n_sockets, NULL, is_smt);
      uint has_mem_bw_all = have_data[MEM_BW_READ] && have_data[MEM_BW1_READ] &&
      	have_data[MEM_BW_WRITE] && have_data[MEM_BW1_WRITE];
      uint has_mem_bw_any = have_data[MEM_BW_READ] && have_data[MEM_BW1_READ] &&
	have_data[MEM_BW_WRITE] && have_data[MEM_BW1_WRITE];
      if (has_mem_bw_all)
	{
	  mctop_mem_bandwidth_add(topo, mem_bw_table_r, mem_bw_table1_r, mem_bw_table_w, mem_bw_table1_w);
	}
      else if (has_mem_bw_any)
	{
	  fprintf(stderr, "MCTOP Warning: Incomplete memory bandwidth data in %s! Ignore.\n", file_open);
	}

      if (cache_info != NULL)
	{
	  mctop_cache_info_add(topo, cache_info);
	}

      if (have_data[POWER])
	{
	  mctop_pow_info_add(topo, pow_measurements);
	}
    }
  else
    {
      fprintf(stderr, "MCTOP Error: Incorrect MCT file %s! Reason: %s\n", file_open, mctop_dtypes[type]);
    }

  table_free((void**) lat_table, n_hwcs);
  table_free((void**) mem_lat_table, n_hwcs);
  table_free((void**) mem_bw_table_r, n_hwcs);
  table_free((void**) mem_bw_table1_r, n_hwcs);
  table_free((void**) mem_bw_table_w, n_hwcs);
  table_free((void**) mem_bw_table1_w, n_hwcs);
  mctop_power_measurements_free(pow_measurements, n_sockets);
  fclose(ifile);

  free(have_data);

#ifdef __x86_64__
  if (topo->has_mem)
    {
      for (uint i = 0; i < topo->n_hwcs; i++)
	{
	  hw_context_t* hwc = &topo->hwcs[i];
	  socket_t* socket = hwc->socket;
	  if (unlikely(socket->local_node != numa_node_of_cpu(hwc->id)))
	    {
	      hwc->local_node_wrong = 1;
	      socket->local_node_wrong = 1;
	    }
	}
    }
#endif

  double dur = (clock() - cstart) / (double) CLOCKS_PER_SEC;
  printf("MCTOP Info: Topology loaded in %.3f ms\n", 1000 * dur);
  return topo;
}
