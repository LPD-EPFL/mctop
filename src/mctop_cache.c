#include <mctop.h>
#include <helper.h>
#include <cdf.h>

void ll_random_create(volatile uint64_t* mem, const size_t size);
ticks ll_random_traverse(volatile uint64_t* list, const size_t reps);
ticks array_get_min(size_t* a, const size_t len);

static ticks
ll_random_latency(const size_t size_bytes)
{
  const size_t n_reps_min = 1e6;

  uint64_t* mem;
  int ret = posix_memalign((void**) &mem, CACHE_LINE_SIZE, size_bytes);
  assert(ret == 0 && mem != NULL);

  ll_random_create(mem, size_bytes);

  const size_t n_reps = size_bytes > n_reps_min ? size_bytes : n_reps_min;
  ticks lat = ll_random_traverse(mem, n_reps);

  free(mem);
  return lat;
}

const size_t mctop_cache_n_lvls = 3;

static int
mctop_cache_size_read_OS(size_t* sizes)
{
  int ret = 1;
  char file[100];
  for (int i = 0; i < mctop_cache_n_lvls + 1; i++)
    {
      sprintf(file, "/sys/devices/system/cpu/cpu0/cache/index%d/size", i);
      FILE* fd = fopen(file, "r");
      if (fd == NULL)
	{
	  fprintf(stderr, "MCTOP Warning: Can't open %s!\n", file);
	  ret = 0;
	  continue;
	}

      uint size;
      int read = fscanf(fd, "%uK", &size);
      if (read != 1)
	{
	  ret = 0;
	  fprintf(stderr, "MCTOP Warning: Can't read %s!\n", file);
	}
      else
	{
	  sizes[i] = size;
	}

      fclose(fd);
    }

  return ret;
}


void
mctop_cache_size_estimate()
{
  size_t sizes_OS[mctop_cache_n_lvls];
  int sizes_OS_ok = mctop_cache_size_read_OS(sizes_OS);
  if (!sizes_OS_ok)
    {
      fprintf(stderr, "MCTOP Warning: Can't read cache sizes from OS\n");
    }

  const size_t KB = 1024;
  
  ticks latencies[mctop_cache_n_lvls + 1];
  ticks sizes[mctop_cache_n_lvls + 1];
  latencies[0] = 0;
  sizes[0] = 4;

  const size_t n_steps_fix = 12;
      

  for (int lvl = 1; lvl <= mctop_cache_n_lvls; lvl++)
    {
      size_t n_steps = (lvl * n_steps_fix);
      size_t stp = 4 * sizes[lvl - 1];
      if (stp > 1024)
	{
	  stp = 1024;
	}
      size_t sensitivity = 1.1 * latencies[lvl - 1];
      int min = stp;
      size_t max = n_steps * stp;
      printf("## Looking for L%d size (%d to %zu KB, step %zu KB)\n",
	     lvl, min, max, stp);

      ticks* lat = calloc_assert(n_steps, sizeof(ticks));

      size_t n = 0, kb;
      for (kb = min; kb < max; kb += stp)
	{
	  lat[n] = ll_random_latency(((kb - 4) * KB));
	  /* printf("[%2zu[ %-5zu KB -> %-5zu cycles\n", n, kb, lat[n]); */
	  n++;
	}

      cdf_t* cdf = cdf_calc(lat + 1, n - 1);
      //      cdf_t* cdf = cdf_calc(lat, n);
      /* cdf_print(cdf); */
      cdf_cluster_t* cc = cdf_cluster(cdf, sensitivity, 0);
      //      cdf_cluster_print(cc);

      size_t tlat = cc->clusters[0].val_max;

      for (kb = min, n = 0; kb < max && lat[n] <= tlat; kb += stp, n++);
      kb -= stp;
    
      size_t clat = array_get_min(lat, n - 1);

      latencies[lvl] = clat;
      sizes[lvl] = kb;

      printf("#### Level %d / Latency: %-4zu / Size:    OS: %5zu KB     Estimated: %5zu KB\n",
	     lvl, clat, sizes_OS[lvl], sizes[lvl]);

      cdf_cluster_free(cc);
      cdf_free(cdf);

      free(lat);
    }
}

//#warning Maybe add the mem. eop numbers in the topology!

ticks
array_get_min(ticks* a, const size_t len)
{
  ticks min = a[0];
  for (size_t i = 1; i < len; i++)
    {
      if (a[i] < min)
	{
	  min = a[i];
	}
    }

  return min;
}
