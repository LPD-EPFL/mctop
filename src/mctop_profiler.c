#include <mctop_profiler.h>
#include <math.h>


const size_t mctop_prof_correction_stdp_limit = 8;

static ticks
mctop_prof_correction_calc(mctop_prof_t* prof, const size_t num_entries)
{
  dvfs_scale_up(1e6, 0.95, NULL);
  size_t std_dev_perc_lim = mctop_prof_correction_stdp_limit;
  mctop_prof_stats_t stats;
  do
    {
      for (volatile size_t i = 0; i < num_entries; i++)
	{
	  MCTOP_PROF_START(prof, ts);
	  COMPILER_BARRIER();
	  MCTOP_PROF_STOP(prof, ts, i);
	}

      mctop_prof_stats_calc(prof, &stats);
    }
  while (stats.std_dev_perc > std_dev_perc_lim++);
  return stats.median;
}

mctop_prof_t*
mctop_prof_create(size_t num_entries)
{
  mctop_prof_t* prof = malloc_assert(sizeof(mctop_prof_t) + (num_entries * sizeof(ticks)));

  prof->size = num_entries;
  prof->correction = 0;
  prof->correction = mctop_prof_correction_calc(prof, num_entries);
  return prof;
}

void
mctop_prof_free(mctop_prof_t* prof)
{
  free(prof);
  prof = NULL;
}


static int
mctop_prof_comp_ticks(const void *elem1, const void *elem2) 
{
  ticks f = *((ticks*)elem1);
  ticks s = *((ticks*)elem2);
  if (f > s) return  1;
  if (f < s) return -1;
  return 0;
}

static inline ticks
mctop_prof_abs_diff(ticks a, ticks b)
{
  if (a > b)
    {
      return a - b;
    }
  return b - a;
}

void
mctop_prof_stats_calc(mctop_prof_t* prof, mctop_prof_stats_t* stats)
{
  qsort(prof->latencies, prof->size, sizeof(ticks), mctop_prof_comp_ticks);
  stats->num_vals = prof->size;
  
  stats->median = prof->latencies[prof->size >> 1];
  const size_t median2x = 2 * stats->median;
  size_t n_elems = 0;
  ticks sum = 0;
  for (size_t i = 0; i < stats->num_vals; i++)
    {
      if (likely(prof->latencies[i] < median2x))
	{
	  sum += prof->latencies[i];
	  n_elems++;
	}
    }

  const ticks avg = sum / n_elems;;
  stats->avg = avg;

  ticks sum_diff_sqr = 0;
  for (size_t i = 0; i < stats->num_vals; i++)
    {
      if (likely(prof->latencies[i] < median2x))
	{
	  ticks adiff = mctop_prof_abs_diff(prof->latencies[i], avg);
	  sum_diff_sqr += (adiff * adiff);
	}
    }
  stats->std_dev = sqrt(sum_diff_sqr / n_elems);
  stats->std_dev_perc = 100 * (1 - (avg - stats->std_dev) / avg);
}

void
mctop_prof_stats_print(mctop_prof_stats_t* stats)
{
  printf("## mctop_prof stats #######\n");
  printf("# #Values       = %zu\n", stats->num_vals);
  printf("# Average       = %zu\n", stats->avg);
  printf("# Median        = %zu\n", stats->median);
  printf("# Std dev       = %.2f\n", stats->std_dev);
  printf("# Std dev%%      = %.2f%%\n", stats->std_dev_perc);
  printf("###########################\n");
}
