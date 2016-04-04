#include <mctop.h>
#include <mctop_internal.h>
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


static mctop_cache_info_t*
mctop_cache_info_create(const uint n_levels)
{
  mctop_cache_info_t* mci = malloc_assert(sizeof(mctop_cache_info_t));
  mci->n_levels = n_levels;
  mci->latencies = calloc_assert(n_levels, sizeof(uint64_t));
  mci->sizes_OS = calloc_assert(n_levels, sizeof(uint64_t));
  mci->sizes_estimated = calloc_assert(n_levels, sizeof(uint64_t));
  return mci;
}

void
mctop_cache_info_free(mctop_cache_info_t* mci)
{
  free(mci->latencies);
  free(mci->sizes_OS);
  free(mci->sizes_estimated);
  free(mci);
}

mctop_cache_info_t*
mctop_cache_size_estimate()
{
  /* +1 for 0: i cache */
  mctop_cache_info_t* mci = mctop_cache_info_create(mctop_cache_n_lvls + 1);

  int sizes_OS_ok = mctop_cache_size_read_OS(mci->sizes_OS);
  if (!sizes_OS_ok)
    {
      fprintf(stderr, "MCTOP Warning: Can't read cache sizes from OS\n");
    }

  const size_t KB = 1024;
  
  mci->sizes_estimated[0] = 4;

  const size_t n_steps_fix = 12;
      

  for (int lvl = 1; lvl <= mctop_cache_n_lvls; lvl++)
    {
      size_t n_steps = (lvl * n_steps_fix);
      size_t stp = 4 * mci->sizes_estimated[lvl - 1];
      if (stp > 1024)
	{
	  stp = 1024;
	}
      size_t sensitivity = 1.1 * mci->latencies[lvl - 1];
      int min = stp;
      size_t max = n_steps * stp;
      printf("## Looking for L%d cache size (%d to %zu KB, step %zu KB)\n",
	     lvl, min, max, stp);

      ticks* lat = calloc_assert(n_steps, sizeof(ticks));

      size_t n = 0, kb;
      for (kb = min; kb < max; kb += stp)
	{
	  lat[n] = ll_random_latency(((kb - 4) * KB));
	  #warning optimize by breaking here?
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

      mci->latencies[lvl] = clat;
      mci->sizes_estimated[lvl] = kb;

      printf("#### Cache L%d / Latency: %-4zu / Size:    OS: %5zu KB     Estimated: %5zu KB\n",
	     lvl, clat, mci->sizes_OS[lvl], mci->sizes_estimated[lvl]);

      cdf_cluster_free(cc);
      cdf_free(cdf);

      free(lat);
    }

  mci->sizes_estimated[0] = mci->sizes_OS[0];

  return mci;
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
