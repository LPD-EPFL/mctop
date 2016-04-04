#include <cdf.h>

static int
cdf_comp(const void* elem1, const void* elem2) 
{
  uint64_t f = *((uint64_t*) elem1);
  uint64_t s = *((uint64_t*) elem2);
  if (f > s)
    {
      return  1;
    }
  if (f < s)
    {
      return -1;
    }
  return 0;
}

cdf_t*
cdf_calc(uint64_t* vals, size_t n_vals)
{
  cdf_t* cdf = calloc(1, sizeof(cdf_t));
  assert(cdf != NULL);

  cdf_point_t* points = malloc(n_vals * sizeof(cdf_point_t));
  assert(points != NULL);

  uint64_t* vals_sorted = malloc(n_vals * sizeof(uint64_t));
  assert(vals_sorted != NULL);
  memcpy(vals_sorted, vals, n_vals * sizeof(uint64_t));
  qsort(vals_sorted, n_vals, sizeof(uint64_t), cdf_comp);

  size_t n_points = 0;
  uint64_t curr = vals_sorted[0];
  for (size_t i = 1; i < n_vals; i++)
    {
      if (vals_sorted[i] != curr)
	{
	  points[n_points].val = curr;
	  points[n_points].percentile = (100.0 * i) / n_vals;
	  n_points++;
	  curr = vals_sorted[i];
	}
    }
  points[n_points].val = curr;
  points[n_points].percentile = 100.0;
  n_points++;

  cdf->n_points = n_points;
  cdf->points = malloc(n_points * sizeof(cdf_point_t));
  assert(cdf->points != NULL);
  memcpy(cdf->points, points, n_points * sizeof(cdf_point_t));

  free(points);
  free(vals_sorted);
  return cdf;
}

void
cdf_free(cdf_t* cdf)
{
  free(cdf->points);
  free(cdf);
  cdf = NULL;
}

void
cdf_print(cdf_t* cdf)
{
  printf("## CDF #######################\n");
  for (size_t i = 0; i < cdf->n_points; i++)
    {
      printf("%%%-7.2f %zu\n", cdf->points[i].percentile, cdf->points[i].val);
    }
}

#define CDF_CLUSTER_DEBUG 0
#if CDF_CLUSTER_DEBUG == 1
#  define CDF_CLUSTER_DEBUG_PRINT(args...) fprintf(stderr, args)
#else
#  define CDF_CLUSTER_DEBUG_PRINT(args...)
#endif

cdf_cluster_t*
cdf_cluster(cdf_t* cdf, const uint sens, const uint target_n_clusters)
{
  int direction = 0;
  uint stop = 0;
  int sensitivity = sens;
  cdf_cluster_t* cc = NULL;


  do
    {
      cdf_cluster_point_t clusters[cdf->n_points];

      size_t pprev = 0, pprev_min = 0, n_clusters = 0, median = 0, cluster_size = 0;
      for (int i = 0; i < cdf->n_points; i++)
	{
	  if (cdf->points[i].val > (pprev + sensitivity) || (pprev == 0 && cdf->points[i].val > 0))
	    {
	      CDF_CLUSTER_DEBUG_PRINT(" -- size %zu\n", cluster_size);
	      clusters[n_clusters].idx = n_clusters;

	      clusters[n_clusters].size = cluster_size;
	      clusters[n_clusters].val_min = pprev_min;
	      clusters[n_clusters].val_max = cdf->points[i - 1].val;
	      clusters[n_clusters++].median = cdf->points[median].val;
	      pprev_min = cdf->points[i].val;
	      median = i;
	      cluster_size = 0;
	    }
	  CDF_CLUSTER_DEBUG_PRINT("%-3zu ", cdf->points[i].val);
	  pprev = cdf->points[i].val;
	  median += (cluster_size++) & 0x1; /* increment once every two */
	}

      CDF_CLUSTER_DEBUG_PRINT(" -- size %zu\n", cluster_size);
      clusters[n_clusters].idx = n_clusters;
      clusters[n_clusters].size = cluster_size;
      clusters[n_clusters].val_min = pprev_min;
      clusters[n_clusters].val_max = cdf->points[cdf->n_points - 1].val;
      clusters[n_clusters++].median = cdf->points[median].val;


      cc = malloc_assert(sizeof(cdf_cluster_t));
      cc->clusters = malloc_assert(n_clusters * sizeof(cdf_cluster_point_t));

      cc->n_clusters = n_clusters;
      for (int i = 0; i < n_clusters; i++)
	{
	  cc->clusters[i].idx = clusters[i].idx;
	  cc->clusters[i].size = clusters[i].size;
	  cc->clusters[i].val_min = clusters[i].val_min;
	  cc->clusters[i].val_max = clusters[i].val_max;
	  cc->clusters[i].median = clusters[i].median;
	}

      if (target_n_clusters && n_clusters != target_n_clusters)
	{
	  //	  cdf_cluster_print(cc);
	  cdf_cluster_free(cc);
	  int dir = (target_n_clusters - n_clusters);
	  if ((direction < 0 && dir > 0) || (direction > 0 && dir < 0))
	    {
	      stop = 1;
	    }
	  direction = dir;
	  sensitivity -= dir;
	  fprintf(stderr, "## Warning: Found %zu clusters, expected %u. Retrying with sensitivity: %-2u!\n",
		  n_clusters, target_n_clusters, sensitivity);
	  cc = NULL;
	}
      else
	{
	  break;
	}
    }
  while (sensitivity > 0 && !stop);

  return cc;
}

uint64_t
cdf_cluster_get_min_latency(cdf_cluster_t* cc)
{
  uint64_t min_lat = cc->clusters[0].median;
  for (int i = 1; i < cc->n_clusters; i++)
    {
      if (min_lat == 0 || (cc->clusters[i].median < min_lat))
	{
	  min_lat = cc->clusters[i].median;
	}
    }
  return min_lat;
}

cdf_cluster_t*
cdf_cluster_create_empty(const int n_clusters)
{
  cdf_cluster_t* cc = calloc_assert(1, sizeof(cdf_cluster_t));
  cc->clusters = calloc_assert(n_clusters, sizeof(cdf_cluster_point_t));
  return cc;
}

void
cdf_cluster_free(cdf_cluster_t* cc)
{
  free(cc->clusters);
  free(cc);
  cc = NULL;
}

void
cdf_cluster_print(cdf_cluster_t* cc)
{
  printf("## CDF Clusters ##############################################\n");
  for (int i = 0; i < cc->n_clusters; i++)
    {
      printf("#### #%-3d : size %-5zu / range %-5zu - %-5zu / median: %-5zu #\n",
	     cc->clusters[i].idx, 
	     cc->clusters[i].size, 
	     cc->clusters[i].val_min, 
	     cc->clusters[i].val_max, 
	     cc->clusters[i].median);
    }
  printf("##############################################################\n");
}

inline uint64_t
cdf_cluster_value_to_cluster_median(cdf_cluster_t* cc, const uint64_t val)
{
  for (int i = 0; i < cc->n_clusters; i++)
    {
      if (val >= cc->clusters[i].val_min && val <= cc->clusters[i].val_max)
	{
	  return cc->clusters[i].median;
	}
    }

  fprintf(stderr, "[ERROR] Value %zu does not belong to cdf_cluster\n", val);
  return 0;
}
