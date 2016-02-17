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

void
cdf_cluster(cdf_t* cdf, const int sensitivity)
{
  cdf_cluster_point_t clusters[cdf->n_points];

  size_t pprev = 0, pprev_min = 0, n_clusters = 0, median = 0, cluster_len = 0;
  for (int i = 0; i < cdf->n_points; i++)
    {
      if (cdf->points[i].val > (pprev + sensitivity))
	{
	  printf("\n");
	  clusters[n_clusters].idx = n_clusters;
	  clusters[n_clusters].val_min = pprev_min;
	  clusters[n_clusters].val_max = cdf->points[i - 1].val;
	  clusters[n_clusters++].median = cdf->points[median].val;
	  pprev_min = cdf->points[i].val;
	  median = i;
	  cluster_len = 0;
	}
      pprev = cdf->points[i].val;
      if ((cluster_len++) & 0x1)
	{
	  median++;
	}
      printf("%-3zu ", cdf->points[i].val);
    }

  printf("\n");
  clusters[n_clusters].idx = n_clusters;
  clusters[n_clusters].val_min = pprev_min;
  clusters[n_clusters].val_max = cdf->points[cdf->n_points - 1].val;
  clusters[n_clusters++].median = cdf->points[median].val;

  for (int i = 0; i < n_clusters; i++)
    {
      printf("**** cluster %-2d : %-5zu - %-5zu (%zu)\n",
	     clusters[i].idx, clusters[i].val_min, clusters[i].val_max, clusters[i].median);
    }
}
